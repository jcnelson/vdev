/*
 * stat_scsi - reads scsi properties
 * 
 * Forked from systemd/src/udev/scsi_id/scsi_serial.c on January 23, 2015
 * (original from http://cgit.freedesktop.org/systemd/systemd/tree/src/udev/scsi_id/scsi_serial.c)
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/*
 * Copyright (C) IBM Corp. 2003
 *
 * Author: Patrick Mansfield<patmans@us.ibm.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"

#include <sys/ioctl.h>
#include <time.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <linux/types.h>
#include <linux/bsg.h>
#include <getopt.h>

#include <scsi/scsi.h>

struct scsi_ioctl_command {
   unsigned int inlen;        /* excluding scsi command length */
   unsigned int outlen;
   unsigned char data[1];
   /* on input, scsi command starts here then opt. data */
};

/*
 * Default 5 second timeout
 */
#define DEF_TIMEOUT        5000

#define SENSE_BUFF_LEN        32

/*
 * The request buffer size passed to the SCSI INQUIRY commands, use 254,
 * as this is a nice value for some devices, especially some of the usb
 * mass storage devices.
 */
#define        SCSI_INQ_BUFF_LEN        254

/*
 * SCSI INQUIRY vendor and model (really product) lengths.
 */
#define VENDOR_LENGTH        8
#define        MODEL_LENGTH        16

#define INQUIRY_CMD     0x12
#define INQUIRY_CMDLEN  6

/*
 * INQUIRY VPD page 0x83 identifier descriptor related values. Reference the
 * SCSI Primary Commands specification for details.
 */

/*
 * id type values of id descriptors. These are assumed to fit in 4 bits.
 */
#define SCSI_ID_VENDOR_SPECIFIC        0
#define SCSI_ID_T10_VENDOR        1
#define SCSI_ID_EUI_64                2
#define SCSI_ID_NAA                3
#define SCSI_ID_RELPORT                4
#define SCSI_ID_TGTGROUP        5
#define SCSI_ID_LUNGROUP        6
#define SCSI_ID_MD5                7
#define SCSI_ID_NAME                8

/*
 * Supported NAA values. These fit in 4 bits, so the "don't care" value
 * cannot conflict with real values.
 */
#define        SCSI_ID_NAA_DONT_CARE                0xff
#define        SCSI_ID_NAA_IEEE_REG                5
#define        SCSI_ID_NAA_IEEE_REG_EXTENDED        6

/*
 * Supported Code Set values.
 */
#define        SCSI_ID_BINARY        1
#define        SCSI_ID_ASCII        2

struct scsi_id_search_values {
        u_char        id_type;
        u_char        naa_type;
        u_char        code_set;
};

/*
 * Following are the "true" SCSI status codes. Linux has traditionally
 * used a 1 bit right and masked version of these. So now CHECK_CONDITION
 * and friends (in <scsi/scsi.h>) are deprecated.
 */
#define SCSI_CHECK_CONDITION 0x2
#define SCSI_CONDITION_MET 0x4
#define SCSI_BUSY 0x8
#define SCSI_IMMEDIATE 0x10
#define SCSI_IMMEDIATE_CONDITION_MET 0x14
#define SCSI_RESERVATION_CONFLICT 0x18
#define SCSI_COMMAND_TERMINATED 0x22
#define SCSI_TASK_SET_FULL 0x28
#define SCSI_ACA_ACTIVE 0x30
#define SCSI_TASK_ABORTED 0x40

#define MAX_PATH_LEN 512

/*
 * MAX_ATTR_LEN: maximum length of the result of reading a sysfs
 * attribute.
 */
#define MAX_ATTR_LEN 256

/*
 * MAX_SERIAL_LEN: the maximum length of the serial number, including
 * added prefixes such as vendor and product (model) strings.
 */
#define MAX_SERIAL_LEN 256

/*
 * MAX_BUFFER_LEN: maximum buffer size and line length used while reading
 * the config file.
 */
#define MAX_BUFFER_LEN 256

struct scsi_id_device {
   char vendor[9];
   char model[17];
   char revision[5];
   char type[33];
   char kernel[64];
   char serial[MAX_SERIAL_LEN];
   char serial_short[MAX_SERIAL_LEN];
   int use_sg;

   /* Always from page 0x80 e.g. 'B3G1P8500RWT' - may not be unique */
   char unit_serial_number[MAX_SERIAL_LEN];

   /* NULs if not set - otherwise hex encoding using lower-case e.g. '50014ee0016eb572' */
   char wwn[17];

   /* NULs if not set - otherwise hex encoding using lower-case e.g. '0xe00000d80000' */
   char wwn_vendor_extension[17];

   /* NULs if not set - otherwise decimal number */
   char tgpt_group[8];
};

int scsi_std_inquiry( struct scsi_id_device *dev_scsi, const char *devname);
int scsi_get_serial( struct scsi_id_device *dev_scsi, const char *devname, int page_code, int len);

/*
 * Page code values.
 */
enum page_code {
   PAGE_83_PRE_SPC3 = -0x83,
   PAGE_UNSPECIFIED = 0x00,
   PAGE_80          = 0x80,
   PAGE_83          = 0x83,
};

/*
 * A priority based list of id, naa, and binary/ascii for the identifier
 * descriptor in VPD page 0x83.
 *
 * Brute force search for a match starting with the first value in the
 * following id_search_list. This is not a performance issue, since there
 * is normally one or some small number of descriptors.
 */
static const struct scsi_id_search_values id_search_list[] = {
   { SCSI_ID_TGTGROUP,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_BINARY },
   { SCSI_ID_NAA,        SCSI_ID_NAA_IEEE_REG_EXTENDED,        SCSI_ID_BINARY },
   { SCSI_ID_NAA,        SCSI_ID_NAA_IEEE_REG_EXTENDED,        SCSI_ID_ASCII },
   { SCSI_ID_NAA,        SCSI_ID_NAA_IEEE_REG,        SCSI_ID_BINARY },
   { SCSI_ID_NAA,        SCSI_ID_NAA_IEEE_REG,        SCSI_ID_ASCII },
   /*
   * Devices already exist using NAA values that are now marked
   * reserved. These should not conflict with other values, or it is
   * a bug in the device. As long as we find the IEEE extended one
   * first, we really don't care what other ones are used. Using
   * don't care here means that a device that returns multiple
   * non-IEEE descriptors in a random order will get different
   * names.
   */
   { SCSI_ID_NAA,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_BINARY },
   { SCSI_ID_NAA,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_ASCII },
   { SCSI_ID_EUI_64,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_BINARY },
   { SCSI_ID_EUI_64,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_ASCII },
   { SCSI_ID_T10_VENDOR,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_BINARY },
   { SCSI_ID_T10_VENDOR,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_ASCII },
   { SCSI_ID_VENDOR_SPECIFIC,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_BINARY },
   { SCSI_ID_VENDOR_SPECIFIC,        SCSI_ID_NAA_DONT_CARE,        SCSI_ID_ASCII },
};

static const char hex_str[]="0123456789abcdef";

/*
 * Values returned in the result/status, only the ones used by the code
 * are used here.
 */

#define DID_NO_CONNECT                        0x01        /* Unable to connect before timeout */
#define DID_BUS_BUSY                        0x02        /* Bus remain busy until timeout */
#define DID_TIME_OUT                        0x03        /* Timed out for some other reason */
#define DRIVER_TIMEOUT                        0x06
#define DRIVER_SENSE                        0x08        /* Sense_buffer has been set */

/* The following "category" function returns one of the following */
#define SG_ERR_CAT_CLEAN                0        /* No errors or other information */
#define SG_ERR_CAT_MEDIA_CHANGED        1        /* interpreted from sense buffer */
#define SG_ERR_CAT_RESET                2        /* interpreted from sense buffer */
#define SG_ERR_CAT_TIMEOUT                3
#define SG_ERR_CAT_RECOVERED                4        /* Successful command after recovered err */
#define SG_ERR_CAT_NOTSUPPORTED                5        /* Illegal / unsupported command */
#define SG_ERR_CAT_SENSE                98        /* Something else in the sense buffer */
#define SG_ERR_CAT_OTHER                99        /* Some other error/warning */


static const struct option options[] = {
   { "device",             required_argument, NULL, 'd' },
   { "config",             required_argument, NULL, 'f' },
   { "page",               required_argument, NULL, 'p' },
   { "replace-whitespace", no_argument,       NULL, 'u' },
   { "sg-version",         required_argument, NULL, 's' },
   { "verbose",            no_argument,       NULL, 'v' },
   { "version",            no_argument,       NULL, 'V' }, /* don't advertise -V */
   { "help",               no_argument,       NULL, 'h' },
   {}
};

static bool all_good = true;
static bool dev_specified = false;
static char config_file[MAX_PATH_LEN] = "/etc/scsi_id.config";
static enum page_code default_page_code = PAGE_UNSPECIFIED;
static int sg_version = 4;
static bool reformat_serial = false;
static char vendor_str[64];
static char model_str[64];
static char vendor_enc_str[256];
static char model_enc_str[256];
static char revision_str[16];
static char type_str[16];

static int do_scsi_page80_inquiry(struct scsi_id_device *dev_scsi, int fd, char *serial, char *serial_short, int max_len);

static int sg_err_category_new(int scsi_status, int msg_status, int host_status, int driver_status, const unsigned char *sense_buffer, int sb_len) {

   scsi_status &= 0x7e;

   /*
   * XXX change to return only two values - failed or OK.
   */

   if (!scsi_status && !host_status && !driver_status) {
      return SG_ERR_CAT_CLEAN;
   }

   if ((scsi_status == SCSI_CHECK_CONDITION) ||
      (scsi_status == SCSI_COMMAND_TERMINATED) ||
      ((driver_status & 0xf) == DRIVER_SENSE)) {
      
      if (sense_buffer && (sb_len > 2)) {
         int sense_key;
         unsigned char asc;

         if (sense_buffer[0] & 0x2) {
            sense_key = sense_buffer[1] & 0xf;
            asc = sense_buffer[2];
         } else {
            sense_key = sense_buffer[2] & 0xf;
            asc = (sb_len > 12) ? sense_buffer[12] : 0;
         }

         if (sense_key == RECOVERED_ERROR) {
            return SG_ERR_CAT_RECOVERED;
         }
         else if (sense_key == UNIT_ATTENTION) {
            if (0x28 == asc) {
               return SG_ERR_CAT_MEDIA_CHANGED;
            }
            if (0x29 == asc) {
               return SG_ERR_CAT_RESET;
            }
         } else if (sense_key == ILLEGAL_REQUEST) {
            return SG_ERR_CAT_NOTSUPPORTED;
         }
      }
      return SG_ERR_CAT_SENSE;
   }
   if (host_status) {
      if ((host_status == DID_NO_CONNECT) ||
         (host_status == DID_BUS_BUSY) ||
         (host_status == DID_TIME_OUT)) {
            return SG_ERR_CAT_TIMEOUT;
      }
   }
   if (driver_status) {
      if (driver_status == DRIVER_TIMEOUT) {
         return SG_ERR_CAT_TIMEOUT;
      }
   }
   return SG_ERR_CAT_OTHER;
}

static int sg_err_category3( struct sg_io_hdr *hp) {
   return sg_err_category_new(hp->status, hp->msg_status, hp->host_status, hp->driver_status, hp->sbp, hp->sb_len_wr);
}

static int sg_err_category4( struct sg_io_v4 *hp) {
   return sg_err_category_new( hp->device_status, 0, hp->transport_status, hp->driver_status, (unsigned char *)(uintptr_t)hp->response, hp->response_len);
}

static int scsi_dump_sense( struct scsi_id_device *dev_scsi, unsigned char *sense_buffer, int sb_len) {

   int s;
   int code;
   int sense_class;
   int sense_key;
   int asc, ascq;
#ifdef DUMP_SENSE
   char out_buffer[256];
   int i, j;
#endif

   /*
   * Figure out and print the sense key, asc and ascq.
   *
   * If you want to suppress these for a particular drive model, add
   * a black list entry in the scsi_id config file.
   *
   * XXX We probably need to: lookup the sense/asc/ascq in a retry
   * table, and if found return 1 (after dumping the sense, asc, and
   * ascq). So, if/when we get something like a power on/reset,
   * we'll retry the command.
   */

   if (sb_len < 1) {
      log_debug("%s: sense buffer empty", dev_scsi->kernel);
      return -1;
   }

   sense_class = (sense_buffer[0] >> 4) & 0x07;
   code = sense_buffer[0] & 0xf;

   if (sense_class == 7) {
      /*
      * extended sense data.
      */
      s = sense_buffer[7] + 8;
      if (sb_len < s) {
         log_debug("%s: sense buffer too small %d bytes, %d bytes too short", dev_scsi->kernel, sb_len, s - sb_len);
         return -1;
      }
      if ((code == 0x0) || (code == 0x1)) {
         sense_key = sense_buffer[2] & 0xf;
         if (s < 14) {
            /*
            * Possible?
            */
            log_debug("%s: sense result too" " small %d bytes", dev_scsi->kernel, s);
            return -1;
         }
         asc = sense_buffer[12];
         ascq = sense_buffer[13];
      } else if ((code == 0x2) || (code == 0x3)) {
         sense_key = sense_buffer[1] & 0xf;
         asc = sense_buffer[2];
         ascq = sense_buffer[3];
      } else {
         log_debug("%s: invalid sense code 0x%x", dev_scsi->kernel, code);
         return -1;
      }
      log_debug("%s: sense key 0x%x ASC 0x%x ASCQ 0x%x", dev_scsi->kernel, sense_key, asc, ascq);
   }
   else {
      if (sb_len < 4) {
         log_debug("%s: sense buffer too small %d bytes, %d bytes too short", dev_scsi->kernel, sb_len, 4 - sb_len);
         return -1;
      }

      if (sense_buffer[0] < 15) {
         log_debug("%s: old sense key: 0x%x", dev_scsi->kernel, sense_buffer[0] & 0x0f);
      }
      
      else {
         log_debug("%s: sense = %2x %2x", dev_scsi->kernel, sense_buffer[0], sense_buffer[2]);
      }
      log_debug("%s: non-extended sense class %d code 0x%0x", dev_scsi->kernel, sense_class, code);

   }

#ifdef DUMP_SENSE
   for (i = 0, j = 0; (i < s) && (j < 254); i++) {
      out_buffer[j++] = hex_str[(sense_buffer[i] & 0xf0) >> 4];
      out_buffer[j++] = hex_str[sense_buffer[i] & 0x0f];
      out_buffer[j++] = ' ';
   }
   out_buffer[j] = '\0';
   log_debug("%s: sense dump:", dev_scsi->kernel);
   log_debug("%s: %s", dev_scsi->kernel, out_buffer);

#endif
   return -1;
}

static int scsi_dump( struct scsi_id_device *dev_scsi, struct sg_io_hdr *io) {

   if (!io->status && !io->host_status && !io->msg_status && !io->driver_status) {
      /*
      * Impossible, should not be called.
      */
      log_debug("%s: called with no error", __FUNCTION__);
      return -1;
   }

   log_debug("%s: sg_io failed status 0x%x 0x%x 0x%x 0x%x", dev_scsi->kernel, io->driver_status, io->host_status, io->msg_status, io->status);
   if (io->status == SCSI_CHECK_CONDITION) {
      return scsi_dump_sense( dev_scsi, io->sbp, io->sb_len_wr);
   }
   else {
      return -1;
   }
}

static int scsi_dump_v4( struct scsi_id_device *dev_scsi, struct sg_io_v4 *io) {
        
   if (!io->device_status && !io->transport_status && !io->driver_status) {
      /*
      * Impossible, should not be called.
      */
      log_debug("%s: called with no error", __FUNCTION__);
      return -1;
   }

   log_debug("%s: sg_io failed status 0x%x 0x%x 0x%x", dev_scsi->kernel, io->driver_status, io->transport_status, io->device_status);
   
   if (io->device_status == SCSI_CHECK_CONDITION) {
      return scsi_dump_sense( dev_scsi, (unsigned char *)(uintptr_t)io->response, io->response_len);
   }
   else {
      return -1;
   }
}

static int scsi_inquiry(struct scsi_id_device *dev_scsi, int fd, unsigned char evpd, unsigned char page, unsigned char *buf, unsigned int buflen) {

   unsigned char inq_cmd[INQUIRY_CMDLEN] = { INQUIRY_CMD, evpd, page, 0, buflen, 0 };
   unsigned char sense[SENSE_BUFF_LEN];
   void *io_buf;
   struct sg_io_v4 io_v4;
   struct sg_io_hdr io_hdr;
   int retry = 3; /* rather random */
   int retval;
   int rc = 0;

   if (buflen > SCSI_INQ_BUFF_LEN) {
      log_debug("buflen %d too long", buflen);
      return -1;
   }

resend:
   if (dev_scsi->use_sg == 4) {
      memzero(&io_v4, sizeof(struct sg_io_v4));
      io_v4.guard = 'Q';
      io_v4.protocol = BSG_PROTOCOL_SCSI;
      io_v4.subprotocol = BSG_SUB_PROTOCOL_SCSI_CMD;
      io_v4.request_len = sizeof(inq_cmd);
      io_v4.request = (uintptr_t)inq_cmd;
      io_v4.max_response_len = sizeof(sense);
      io_v4.response = (uintptr_t)sense;
      io_v4.din_xfer_len = buflen;
      io_v4.din_xferp = (uintptr_t)buf;
      io_buf = (void *)&io_v4;
   }
   else {
      memzero(&io_hdr, sizeof(struct sg_io_hdr));
      io_hdr.interface_id = 'S';
      io_hdr.cmd_len = sizeof(inq_cmd);
      io_hdr.mx_sb_len = sizeof(sense);
      io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
      io_hdr.dxfer_len = buflen;
      io_hdr.dxferp = buf;
      io_hdr.cmdp = inq_cmd;
      io_hdr.sbp = sense;
      io_hdr.timeout = DEF_TIMEOUT;
      io_buf = (void *)&io_hdr;
   }

   retval = ioctl(fd, SG_IO, io_buf);
   if (retval < 0) {
      if ((errno == EINVAL || errno == ENOSYS) && dev_scsi->use_sg == 4) {
         dev_scsi->use_sg = 3;
         goto resend;
      }
      
      rc = -errno;
      log_debug( "%s: ioctl failed: errno = %d", dev_scsi->kernel, rc);
      goto error;
   }

   if (dev_scsi->use_sg == 4) {
      retval = sg_err_category4(io_buf);
   }
   else {
      retval = sg_err_category3(io_buf);
   }

   switch (retval) {
   case SG_ERR_CAT_NOTSUPPORTED:
      buf[1] = 0;
   /* Fallthrough */
   case SG_ERR_CAT_CLEAN:
   case SG_ERR_CAT_RECOVERED:
      retval = 0;
      break;

   default:
      if (dev_scsi->use_sg == 4) {
         retval = scsi_dump_v4(dev_scsi, io_buf);
      }
      else {
         retval = scsi_dump(dev_scsi, io_buf);
      }
   }

   if (!retval) {
      retval = buflen;
   } else if (retval > 0) {
      if (--retry > 0) {
      goto resend;
      }
      retval = -1;
   }

error:
   if (retval < 0) {
      log_debug("%s: Unable to get INQUIRY vpd %d page 0x%x.", dev_scsi->kernel, evpd, page);
   }
   
   return retval;
}

/* Get list of supported EVPD pages */
static int do_scsi_page0_inquiry( struct scsi_id_device *dev_scsi, int fd, unsigned char *buffer, unsigned int len) {

   int retval;

   memzero(buffer, len);
   retval = scsi_inquiry( dev_scsi, fd, 1, 0x0, buffer, len);
   if (retval < 0) {
      return 1;
   }

   if (buffer[1] != 0) {
      log_debug("%s: page 0 not available.", dev_scsi->kernel);
      return 1;
   }
   if (buffer[3] > len) {
      log_debug("%s: page 0 buffer too long %d", dev_scsi->kernel, buffer[3]);
      return 1;
   }

   /*
   * Following check is based on code once included in the 2.5.x
   * kernel.
   *
   * Some ill behaved devices return the standard inquiry here
   * rather than the evpd data, snoop the data to verify.
   */
   if (buffer[3] > MODEL_LENGTH) {
      /*
      * If the vendor id appears in the page assume the page is
      * invalid.
      */
      if (strneq((char *)&buffer[VENDOR_LENGTH], dev_scsi->vendor, VENDOR_LENGTH)) {
            log_debug("%s: invalid page0 data", dev_scsi->kernel);
            return 1;
      }
   }
   return 0;
}

/*
 * The caller checks that serial is long enough to include the vendor +
 * model.
 */
static int prepend_vendor_model(struct scsi_id_device *dev_scsi, char *serial) {

   int ind;

   strncpy(serial, dev_scsi->vendor, VENDOR_LENGTH);
   strncat(serial, dev_scsi->model, MODEL_LENGTH);
   ind = strlen(serial);

   /*
   * This is not a complete check, since we are using strncat/cpy
   * above, ind will never be too large.
   */
   if (ind != (VENDOR_LENGTH + MODEL_LENGTH)) {
      log_debug("%s: expected length %d, got length %d", dev_scsi->kernel, (VENDOR_LENGTH + MODEL_LENGTH), ind);
      return -1;
   }
   return ind;
}

/*
 * check_fill_0x83_id - check the page 0x83 id, if OK allocate and fill
 * serial number.
 */
static int check_fill_0x83_id(struct scsi_id_device *dev_scsi,
                              unsigned char *page_83,
                              const struct scsi_id_search_values
                              *id_search, char *serial, char *serial_short,
                              int max_len, char *wwn,
                              char *wwn_vendor_extension, char *tgpt_group) {

   int i, j, s, len;

   /*
   * ASSOCIATION must be with the device (value 0)
   * or with the target port for SCSI_ID_TGTPORT
   */
   if ((page_83[1] & 0x30) == 0x10) {
      if (id_search->id_type != SCSI_ID_TGTGROUP) {
         return 1;
      }
   }
   else if ((page_83[1] & 0x30) != 0) {
      return 1;
   }

   if ((page_83[1] & 0x0f) != id_search->id_type) {
      return 1;
   }

   /*
   * Possibly check NAA sub-type.
   */
   if ((id_search->naa_type != SCSI_ID_NAA_DONT_CARE) && (id_search->naa_type != (page_83[4] & 0xf0) >> 4)) {
      return 1;
   }

   /*
   * Check for matching code set - ASCII or BINARY.
   */
   if ((page_83[0] & 0x0f) != id_search->code_set) {
      return 1;
   }

   /*
   * page_83[3]: identifier length
   */
   len = page_83[3];
   if ((page_83[0] & 0x0f) != SCSI_ID_ASCII) {
      /*
      * If not ASCII, use two bytes for each binary value.
      */
      len *= 2;
   }

   /*
   * Add one byte for the NUL termination, and one for the id_type.
   */
   len += 2;
   if (id_search->id_type == SCSI_ID_VENDOR_SPECIFIC) {
      len += VENDOR_LENGTH + MODEL_LENGTH;
   }

   if (max_len < len) {
      log_debug("%s: length %d too short - need %d", dev_scsi->kernel, max_len, len);
      return 1;
   }

   if (id_search->id_type == SCSI_ID_TGTGROUP && tgpt_group != NULL) {
      unsigned int group;

      group = ((unsigned int)page_83[6] << 8) | page_83[7];
      sprintf(tgpt_group,"%x", group);
      return 1;
   }

   serial[0] = hex_str[id_search->id_type];

   /*
   * For SCSI_ID_VENDOR_SPECIFIC prepend the vendor and model before
   * the id since it is not unique across all vendors and models,
   * this differs from SCSI_ID_T10_VENDOR, where the vendor is
   * included in the identifier.
   */
   if (id_search->id_type == SCSI_ID_VENDOR_SPECIFIC) {
      if (prepend_vendor_model( dev_scsi, &serial[1]) < 0) {
         return 1;
      }
   }

   i = 4; /* offset to the start of the identifier */
   s = j = strlen(serial);
   if ((page_83[0] & 0x0f) == SCSI_ID_ASCII) {
      /*
      * ASCII descriptor.
      */
      while (i < (4 + page_83[3])) {
            serial[j++] = page_83[i++];
      }
   } else {
      /*
      * Binary descriptor, convert to ASCII, using two bytes of
      * ASCII for each byte in the page_83.
      */
      while (i < (4 + page_83[3])) {
            serial[j++] = hex_str[(page_83[i] & 0xf0) >> 4];
            serial[j++] = hex_str[page_83[i] & 0x0f];
            i++;
      }
   }

   strcpy(serial_short, &serial[s]);

   if (id_search->id_type == SCSI_ID_NAA && wwn != NULL) {
      strncpy(wwn, &serial[s], 16);
      if (wwn_vendor_extension != NULL) {
         strncpy(wwn_vendor_extension, &serial[s + 16], 16);
      }
   }

   return 0;
}

/* Extract the raw binary from VPD 0x83 pre-SPC devices */
static int check_fill_0x83_prespc3(struct scsi_id_device *dev_scsi,
                                   unsigned char *page_83,
                                   const struct scsi_id_search_values
                                   *id_search, char *serial, char *serial_short, int max_len) {
   int i, j;

   serial[0] = hex_str[id_search->id_type];
   /* serial has been memset to zero before */
   j = strlen(serial);        /* j = 1; */

   for (i = 0; (i < page_83[3]) && (j < max_len-3); ++i) {
      serial[j++] = hex_str[(page_83[4+i] & 0xf0) >> 4];
      serial[j++] = hex_str[ page_83[4+i] & 0x0f];
   }
   serial[max_len-1] = 0;
   strncpy(serial_short, serial, max_len-1);
   return 0;
}


/* Get device identification VPD page */
static int do_scsi_page83_inquiry(struct scsi_id_device *dev_scsi, int fd,
                                  char *serial, char *serial_short, int len,
                                  char *unit_serial_number, char *wwn,
                                  char *wwn_vendor_extension, char *tgpt_group) {
   int retval;
   unsigned int id_ind, j;
   unsigned char page_83[SCSI_INQ_BUFF_LEN];

   /* also pick up the page 80 serial number */
   do_scsi_page80_inquiry( dev_scsi, fd, NULL, unit_serial_number, MAX_SERIAL_LEN);

   memzero(page_83, SCSI_INQ_BUFF_LEN);
   retval = scsi_inquiry( dev_scsi, fd, 1, PAGE_83, page_83, SCSI_INQ_BUFF_LEN);
   if (retval < 0) {
      return 1;
   }

   if (page_83[1] != PAGE_83) {
      log_debug("%s: Invalid page 0x83", dev_scsi->kernel);
      return 1;
   }

   /*
   * XXX Some devices (IBM 3542) return all spaces for an identifier if
   * the LUN is not actually configured. This leads to identifiers of
   * the form: "1            ".
   */

   /*
   * Model 4, 5, and (some) model 6 EMC Symmetrix devices return
   * a page 83 reply according to SCSI-2 format instead of SPC-2/3.
   *
   * The SCSI-2 page 83 format returns an IEEE WWN in binary
   * encoded hexi-decimal in the 16 bytes following the initial
   * 4-byte page 83 reply header.
   *
   * Both the SPC-2 and SPC-3 formats return an IEEE WWN as part
   * of an Identification descriptor.  The 3rd byte of the first
   * Identification descriptor is a reserved (BSZ) byte field.
   *
   * Reference the 7th byte of the page 83 reply to determine
   * whether the reply is compliant with SCSI-2 or SPC-2/3
   * specifications.  A zero value in the 7th byte indicates
   * an SPC-2/3 conformant reply, (i.e., the reserved field of the
   * first Identification descriptor).  This byte will be non-zero
   * for a SCSI-2 conformant page 83 reply from these EMC
   * Symmetrix models since the 7th byte of the reply corresponds
   * to the 4th and 5th nibbles of the 6-byte OUI for EMC, that is,
   * 0x006048.
   */

   if (page_83[6] != 0) {
      return check_fill_0x83_prespc3(dev_scsi, page_83, id_search_list, serial, serial_short, len);
   }
   
   /*
   * Search for a match in the prioritized id_search_list - since WWN ids
   * come first we can pick up the WWN in check_fill_0x83_id().
   */
   for (id_ind = 0; id_ind < sizeof(id_search_list)/sizeof(id_search_list[0]); id_ind++) {
      /*
      * Examine each descriptor returned. There is normally only
      * one or a small number of descriptors.
      */
      for (j = 4; j <= (unsigned int)page_83[3] + 3; j += page_83[j + 3] + 4) {

         retval = check_fill_0x83_id( dev_scsi, &page_83[j],
                                       &id_search_list[id_ind],
                                       serial, serial_short, len,
                                       wwn, wwn_vendor_extension,
                                       tgpt_group);
         if (!retval) {
            return retval;
         }
         else if (retval < 0) {
            return retval;
         }
      }
   }
   return 1;
}

/*
 * Get device identification VPD page for older SCSI-2 device which is not
 * compliant with either SPC-2 or SPC-3 format.
 *
 * Return the hard coded error code value 2 if the page 83 reply is not
 * conformant to the SCSI-2 format.
 */
static int do_scsi_page83_prespc3_inquiry(struct scsi_id_device *dev_scsi, int fd, char *serial, char *serial_short, int len) {
   int retval;
   int i, j;
   unsigned char page_83[SCSI_INQ_BUFF_LEN];

   memzero(page_83, SCSI_INQ_BUFF_LEN);
   retval = scsi_inquiry(dev_scsi, fd, 1, PAGE_83, page_83, SCSI_INQ_BUFF_LEN);
   if (retval < 0) {
      return 1;
   }

   if (page_83[1] != PAGE_83) {
      log_debug("%s: Invalid page 0x83", dev_scsi->kernel);
      return 1;
   }
   /*
   * Model 4, 5, and (some) model 6 EMC Symmetrix devices return
   * a page 83 reply according to SCSI-2 format instead of SPC-2/3.
   *
   * The SCSI-2 page 83 format returns an IEEE WWN in binary
   * encoded hexi-decimal in the 16 bytes following the initial
   * 4-byte page 83 reply header.
   *
   * Both the SPC-2 and SPC-3 formats return an IEEE WWN as part
   * of an Identification descriptor.  The 3rd byte of the first
   * Identification descriptor is a reserved (BSZ) byte field.
   *
   * Reference the 7th byte of the page 83 reply to determine
   * whether the reply is compliant with SCSI-2 or SPC-2/3
   * specifications.  A zero value in the 7th byte indicates
   * an SPC-2/3 conformant reply, (i.e., the reserved field of the
   * first Identification descriptor).  This byte will be non-zero
   * for a SCSI-2 conformant page 83 reply from these EMC
   * Symmetrix models since the 7th byte of the reply corresponds
   * to the 4th and 5th nibbles of the 6-byte OUI for EMC, that is,
   * 0x006048.
   */
   if (page_83[6] == 0) {
      return 2;
   }

   serial[0] = hex_str[id_search_list[0].id_type];
   /*
   * The first four bytes contain data, not a descriptor.
   */
   i = 4;
   j = strlen(serial);
   /*
   * Binary descriptor, convert to ASCII,
   * using two bytes of ASCII for each byte
   * in the page_83.
   */
   while (i < (page_83[3]+4)) {
      serial[j++] = hex_str[(page_83[i] & 0xf0) >> 4];
      serial[j++] = hex_str[page_83[i] & 0x0f];
      i++;
   }
   return 0;
}

/* Get unit serial number VPD page */
static int do_scsi_page80_inquiry(struct scsi_id_device *dev_scsi, int fd,
                                  char *serial, char *serial_short, int max_len) {
        
   int retval;
   int ser_ind;
   int i;
   int len;
   unsigned char buf[SCSI_INQ_BUFF_LEN];

   memzero(buf, SCSI_INQ_BUFF_LEN);
   retval = scsi_inquiry( dev_scsi, fd, 1, PAGE_80, buf, SCSI_INQ_BUFF_LEN);
   if (retval < 0) {
      return retval;
   }

   if (buf[1] != PAGE_80) {
      log_debug("%s: Invalid page 0x80", dev_scsi->kernel);
      return 1;
   }

   len = 1 + VENDOR_LENGTH + MODEL_LENGTH + buf[3];
   if (max_len < len) {
      log_debug("%s: length %d too short - need %d", dev_scsi->kernel, max_len, len);
      return 1;
   }
   /*
   * Prepend 'S' to avoid unlikely collision with page 0x83 vendor
   * specific type where we prepend '0' + vendor + model.
   */
   len = buf[3];
   if (serial != NULL) {
      serial[0] = 'S';
      ser_ind = prepend_vendor_model( dev_scsi, &serial[1]);
      if (ser_ind < 0) {
         return 1;
      }
      ser_ind++; /* for the leading 'S' */
      for (i = 4; i < len + 4; i++, ser_ind++) {
         serial[ser_ind] = buf[i];
      }
   }
   if (serial_short != NULL) {
      memcpy(serial_short, &buf[4], len);
      serial_short[len] = '\0';
   }
   return 0;
}

int scsi_std_inquiry(struct scsi_id_device *dev_scsi, const char *devname) {
        
   int fd;
   unsigned char buf[SCSI_INQ_BUFF_LEN];
   struct stat statbuf;
   int err = 0;

   fd = open(devname, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
   if (fd < 0) {
      err = -errno;
      log_debug( "scsi_id: cannot open %s: errno = %d", devname, err);
      return 1;
   }

   if (fstat(fd, &statbuf) < 0) {
      
      err = -errno;
      log_debug( "scsi_id: cannot stat %s: errno = %d", devname, err);
      err = 2;
      goto out;
   }
   
   sprintf(dev_scsi->kernel,"%d:%d", major(statbuf.st_rdev), minor(statbuf.st_rdev));

   memzero(buf, SCSI_INQ_BUFF_LEN);
   err = scsi_inquiry( dev_scsi, fd, 0, 0, buf, SCSI_INQ_BUFF_LEN);
   if (err < 0) {
      goto out;
   }

   err = 0;
   memcpy(dev_scsi->vendor, buf + 8, 8);
   dev_scsi->vendor[8] = '\0';
   memcpy(dev_scsi->model, buf + 16, 16);
   dev_scsi->model[16] = '\0';
   memcpy(dev_scsi->revision, buf + 32, 4);
   dev_scsi->revision[4] = '\0';
   sprintf(dev_scsi->type,"%x", buf[0] & 0x1f);

out:
   close(fd);
   return err;
}

int scsi_get_serial(struct scsi_id_device *dev_scsi, const char *devname, int page_code, int len) {

   unsigned char page0[SCSI_INQ_BUFF_LEN];
   int fd = -1;
   int cnt;
   int ind;
   int retval;

   memzero(dev_scsi->serial, len);
   initialize_srand();
   
   for (cnt = 20; cnt > 0; cnt--) {
      struct timespec duration;

      fd = open(devname, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
      if (fd >= 0 || errno != EBUSY) {
         break;
      }
      
      duration.tv_sec = 0;
      duration.tv_nsec = (200 * 1000 * 1000) + (rand() % 100 * 1000 * 1000);
      nanosleep(&duration, NULL);
   }
   if (fd < 0) {
      return 1;
   }

   if (page_code == PAGE_80) {
      if (do_scsi_page80_inquiry( dev_scsi, fd, dev_scsi->serial, dev_scsi->serial_short, len)) {
            retval = 1;
            goto completed;
      } else  {
            retval = 0;
            goto completed;
      }
   }
   else if (page_code == PAGE_83) {
      if (do_scsi_page83_inquiry( dev_scsi, fd, dev_scsi->serial, dev_scsi->serial_short, len, dev_scsi->unit_serial_number, dev_scsi->wwn, dev_scsi->wwn_vendor_extension, dev_scsi->tgpt_group)) {
         retval = 1;
         goto completed;
      } else  {
         retval = 0;
         goto completed;
      }
   }
   else if (page_code == PAGE_83_PRE_SPC3) {
      retval = do_scsi_page83_prespc3_inquiry( dev_scsi, fd, dev_scsi->serial, dev_scsi->serial_short, len);
      if (retval) {
         /*
            * Fallback to servicing a SPC-2/3 compliant page 83
            * inquiry if the page 83 reply format does not
            * conform to pre-SPC3 expectations.
            */
         if (retval == 2) {
            if (do_scsi_page83_inquiry( dev_scsi, fd, dev_scsi->serial, dev_scsi->serial_short, len, dev_scsi->unit_serial_number, dev_scsi->wwn, dev_scsi->wwn_vendor_extension, dev_scsi->tgpt_group)) {
               retval = 1;
               goto completed;
            } else  {
               retval = 0;
               goto completed;
            }
         }
         else {
            retval = 1;
            goto completed;
         }
      } else  {
            retval = 0;
            goto completed;
      }
   } else if (page_code != 0x00) {
      log_debug("%s: unsupported page code 0x%d", dev_scsi->kernel, page_code);
      retval = 1;
      goto completed;
   }

   /*
   * Get page 0, the page of the pages. By default, try from best to
   * worst of supported pages: 0x83 then 0x80.
   */
   if (do_scsi_page0_inquiry( dev_scsi, fd, page0, SCSI_INQ_BUFF_LEN)) {
      /*
      * Don't try anything else. Black list if a specific page
      * should be used for this vendor+model, or maybe have an
      * optional fall-back to page 0x80 or page 0x83.
      */
      retval = 1;
      goto completed;
   }

   for (ind = 4; ind <= page0[3] + 3; ind++) {
      if (page0[ind] == PAGE_83) {
         if (!do_scsi_page83_inquiry( dev_scsi, fd, dev_scsi->serial, dev_scsi->serial_short, len, dev_scsi->unit_serial_number, dev_scsi->wwn, dev_scsi->wwn_vendor_extension, dev_scsi->tgpt_group)) {
            /*
            * Success
            */
            retval = 0;
            goto completed;
         }
      }
   }

   for (ind = 4; ind <= page0[3] + 3; ind++) {
      if (page0[ind] == PAGE_80) {
         if (!do_scsi_page80_inquiry( dev_scsi, fd, dev_scsi->serial, dev_scsi->serial_short, len)) {
            /*
            * Success
            */
            retval = 0;
            goto completed;
         }
      }
   }
   retval = 1;

completed:
   close(fd);
   return retval;
}



static void set_type(const char *from, char *to, size_t len)
{
   int type_num;
   char *eptr;
   const char *type = "generic";

   type_num = strtoul(from, &eptr, 0);
   if (eptr != from) {
      switch (type_num) {
      case 0:
            type = "disk";
            break;
      case 1:
            type = "tape";
            break;
      case 4:
            type = "optical";
            break;
      case 5:
            type = "cd";
            break;
      case 7:
            type = "optical";
            break;
      case 0xe:
            type = "disk";
            break;
      case 0xf:
            type = "optical";
            break;
      default:
            break;
      }
   }
   strscpy(to, len, type);
}

/*
 * get_value:
 *
 * buf points to an '=' followed by a quoted string ("foo") or a string ending
 * with a space or ','.
 *
 * Return a pointer to the NUL terminated string, returns NULL if no
 * matches.
 */
static char *get_value(char **buffer) {
   
   static const char *quote_string = "\"\n";
   static const char *comma_string = ",\n";
   char *val;
   const char *end;

   if (**buffer == '"') {
      /*
      * skip leading quote, terminate when quote seen
      */
      (*buffer)++;
      end = quote_string;
   } else {
      end = comma_string;
   }
   val = strsep(buffer, end);
   if (val && end == quote_string) {
      /*
      * skip trailing quote
      */
      (*buffer)++;
   }
   
   while (isspace(**buffer)) {
      (*buffer)++;
   }
   return val;
}

static int argc_count(char *opts) {

   int i = 0;
   while (*opts != '\0') {
      if (*opts++ == ' ') {
         i++;
      }
   }
   return i;
}

/*
 * get_file_options:
 *
 * If vendor == NULL, find a line in the config file with only "OPTIONS=";
 * if vendor and model are set find the first OPTIONS line in the config
 * file that matches. Set argc and argv to match the OPTIONS string.
 *
 * vendor and model can end in '\n'.
 */
static int get_file_options(const char *vendor, const char *model, int *argc, char ***newargv) {

   char *buffer;
   FILE *f = NULL;
   char *buf;
   char *str1;
   char *vendor_in, *model_in, *options_in; /* read in from file */
   int lineno;
   int c;
   int retval = 0;
   int rc = 0;

   f = fopen(config_file, "re");
   if (f == NULL) {
      if (errno == ENOENT) {
         return 1;
      }
      else {
         
         rc = -errno;
         log_error( "can't open %s: errno = %d", config_file, rc);
         return -1;
      }
   }

   /*
   * Allocate a buffer rather than put it on the stack so we can
   * keep it around to parse any options (any allocated newargv
   * points into this buffer for its strings).
   */
   buffer = (char*)malloc(MAX_BUFFER_LEN);
   if (!buffer) {
      
      fclose(f);
      return -ENOMEM;
   }

   *newargv = NULL;
   lineno = 0;
   while (1) {
      vendor_in = model_in = options_in = NULL;

      buf = fgets(buffer, MAX_BUFFER_LEN, f);
      
      if (buf == NULL) {
         break;
      }
      
      lineno++;
      if (buf[strlen(buffer) - 1] != '\n') {
         log_error("Config file line %d too long", lineno);
         break;
      }

      while (isspace(*buf)) {
         buf++;
      }

      /* blank or all whitespace line */
      if (*buf == '\0') {
         continue;
      }

      /* comment line */
      if (*buf == '#') {
         continue;
      }
      str1 = strsep(&buf, "=");
      if (str1 && strcaseeq(str1, "VENDOR")) {
         str1 = get_value(&buf);
         if (!str1) {
            retval = -ENOMEM;
            break;
         }
         vendor_in = str1;

         str1 = strsep(&buf, "=");
         if (str1 && strcaseeq(str1, "MODEL")) {
            str1 = get_value(&buf);
            if (!str1) {
               retval = -ENOMEM;
               break;
            }
            model_in = str1;
            str1 = strsep(&buf, "=");
         }
      }

      if (str1 && strcaseeq(str1, "OPTIONS")) {
         str1 = get_value(&buf);
         if (!str1) {
            retval = -ENOMEM;
            break;
         }
         options_in = str1;
      }

      /*
      * Only allow: [vendor=foo[,model=bar]]options=stuff
      */
      if (!options_in || (!vendor_in && model_in)) {
         log_error("Error parsing config file line %d '%s'", lineno, buffer);
         retval = -1;
         break;
      }
      if (vendor == NULL) {
         if (vendor_in == NULL) {
            break;
         }
      }
      else if (vendor_in && strneq(vendor, vendor_in, strlen(vendor_in)) && (!model_in || (strneq(model, model_in, strlen(model_in))))) {
         /*
         * Matched vendor and optionally model.
         *
         * Note: a short vendor_in or model_in can
         * give a partial match (that is FOO
         * matches FOOBAR).
         */
         break;
      }
   }
   
   fclose( f );

   if (retval == 0) {
      if (vendor_in != NULL || model_in != NULL || options_in != NULL) {
         /*
         * Something matched. Allocate newargv, and store
         * values found in options_in.
         */
         strcpy(buffer, options_in);
         c = argc_count(buffer) + 2;
         *newargv = calloc(c, sizeof(**newargv));
         if (!*newargv) {
            
            retval = -ENOMEM;
         }
         else {
            *argc = c;
            c = 0;
            /*
            * argv[0] at 0 is skipped by getopt, but
            * store the buffer address there for
            * later freeing
            */
            (*newargv)[c] = buffer;
            for (c = 1; c < *argc; c++) {
               (*newargv)[c] = strsep(&buffer, " \t");
            }
         }
      } else {
         /* No matches  */
         retval = 1;
      }
   }
   if (retval != 0) {
      free(buffer);
   }
   
   return retval;
}

static void help( char const* progname ) {
   printf("Usage: %s [OPTION...] /path/to/device/file\n\n"
      "SCSI device identification.\n\n"
      "  -h --help                        Print this message\n"
      "     --version                     Print version of the program\n\n"
      "  -d --device=                     Device node for SG_IO commands\n"
      "  -f --config=                     Location of config file\n"
      "  -p --page=0x80|0x83|pre-spc3-83  SCSI page (0x80, 0x83, pre-spc3-83)\n"
      "  -s --sg-version=3|4              Use SGv3 or SGv4\n"
      "  -u --replace-whitespace          Replace all whitespace by underscores\n"
      "  -v --verbose                     Verbose logging\n"
      , progname);

}

static int set_options( int argc, char **argv, char *maj_min_dev) {

   int option;

   /*
   * optind is a global extern used by getopt. Since we can call
   * set_options twice (once for command line, and once for config
   * file) we have to reset this back to 1.
   */
   optind = 1;
   while ((option = getopt_long(argc, argv, "d:f:gp:uvVh", options, NULL)) >= 0)
      switch (option) {

      case 'd':
            dev_specified = true;
            strscpy(maj_min_dev, MAX_PATH_LEN, optarg);
            break;

      case 'f':
            strscpy(config_file, MAX_PATH_LEN, optarg);
            break;

      case 'h':
            help( argv[0] );
            exit(0);

      case 'p':
            if (streq(optarg, "0x80"))
               default_page_code = PAGE_80;
            else if (streq(optarg, "0x83"))
               default_page_code = PAGE_83;
            else if (streq(optarg, "pre-spc3-83"))
               default_page_code = PAGE_83_PRE_SPC3;
            else {
               fprintf(stderr, "[ERROR] %s: Unknown page code '%s'\n", argv[0], optarg);
               return -1;
            }
            break;

      case 's':
            sg_version = atoi(optarg);
            if (sg_version < 3 || sg_version > 4) {
               fprintf(stderr, "[ERROR] %s: Unknown SG version '%s'\n", argv[0], optarg);
               return -1;
            }
            break;

      case 'u':
            reformat_serial = true;
            break;

      case 'v':
            break;

      case 'V':
            printf("%s\n", "1.0");
            exit(0);

      case '?':
            return -1;

      default:
            fprintf(stderr, "[ERROR] %s: Unknown option -%c\n", argv[0], option);
            return -1;
   }

   if (optind < argc && !dev_specified) {
      dev_specified = true;
      strscpy(maj_min_dev, MAX_PATH_LEN, argv[optind]);
   }

   return 0;
}

static int per_dev_options(struct scsi_id_device *dev_scsi, int *good_bad, int *page_code) {

   int retval;
   int newargc;
   char **newargv = NULL;
   int option;

   *good_bad = all_good;
   *page_code = default_page_code;

   retval = get_file_options( vendor_str, model_str, &newargc, &newargv);

   optind = 1; /* reset this global extern */
   while (retval == 0) {
      option = getopt_long(newargc, newargv, "p:", options, NULL);
      if (option == -1)
            break;

      switch (option) {
      
      case 'p':
         if (streq(optarg, "0x80")) {
            *page_code = PAGE_80;
         } else if (streq(optarg, "0x83")) {
            *page_code = PAGE_83;
         } else if (streq(optarg, "pre-spc3-83")) {
            *page_code = PAGE_83_PRE_SPC3;
         } else {
            log_error("Unknown page code '%s'", optarg);
            retval = -1;
         }
         break;

      default:
         log_error("Unknown or bad option '%c' (0x%x)", option, option);
         retval = -1;
         break;
      }
   }

   if (newargv) {
      free(newargv[0]);
      free(newargv);
   }
   return retval;
}


static int set_inq_values( struct scsi_id_device *dev_scsi, const char *path) {
   
   int retval;

   dev_scsi->use_sg = sg_version;

   retval = scsi_std_inquiry(dev_scsi, path);
   if (retval) {
      
      log_error("scsi_std_inquiry('%s') rc = %d\n", path, retval);
      return retval;
   }

   vdev_util_encode_string(dev_scsi->vendor, vendor_enc_str, sizeof(vendor_enc_str));
   vdev_util_encode_string(dev_scsi->model, model_enc_str, sizeof(model_enc_str));

   vdev_util_replace_whitespace(dev_scsi->vendor, vendor_str, sizeof(vendor_str));
   vdev_util_replace_chars(vendor_str, NULL);
   vdev_util_replace_whitespace(dev_scsi->model, model_str, sizeof(model_str));
   vdev_util_replace_chars(model_str, NULL);
   set_type(dev_scsi->type, type_str, sizeof(type_str));
   vdev_util_replace_whitespace(dev_scsi->revision, revision_str, sizeof(revision_str));
   vdev_util_replace_chars(revision_str, NULL);
   return 0;
}

/*
 * scsi_id: try to get an id, if one is found, printf it to stdout.
 * returns a value passed to exit() - 0 if printed an id, else 1.
 */
static int scsi_id( char *maj_min_dev) {

   struct scsi_id_device dev_scsi = {};
   int good_dev = 1;
   int page_code = 0;
   int retval = 0;

   if (set_inq_values( &dev_scsi, maj_min_dev) < 0) {
      retval = 1;
      return retval;
   }

   /* get per device (vendor + model) options from the config file */
   per_dev_options( &dev_scsi, &good_dev, &page_code);
   if (!good_dev) {
      
      log_error("per_dev_options: good_dev = %d", good_dev );
      retval = 1;
      return retval;
   }

   /* read serial number from mode pages (no values for optical drives) */
   scsi_get_serial( &dev_scsi, maj_min_dev, page_code, MAX_SERIAL_LEN);

   char serial_str[MAX_SERIAL_LEN];

   vdev_property_add("VDEV_SCSI", "1" );
   vdev_property_add("VDEV_SCSI_VENDOR", vendor_str );
   vdev_property_add("VDEV_SCSI_VENDOR_ENC", vendor_enc_str );
   vdev_property_add("VDEV_SCSI_MODEL", model_str );
   vdev_property_add("VDEV_SCSI_MODEL_ENC", model_enc_str );
   vdev_property_add("VDEV_SCSI_REVISION", revision_str );
   vdev_property_add("VDEV_SCSI_TYPE", type_str );
   
   if (dev_scsi.serial[0] != '\0') {
      
      vdev_util_replace_whitespace(dev_scsi.serial, serial_str, sizeof(serial_str));
      vdev_util_replace_chars(serial_str, NULL);
      
      vdev_property_add("VDEV_SCSI_SERIAL", serial_str );
      
      vdev_util_replace_whitespace(dev_scsi.serial_short, serial_str, sizeof(serial_str));
      vdev_util_replace_chars(serial_str, NULL);
      
      vdev_property_add("VDEV_SCSI_SERIAL_SHORT", serial_str );
   }
   if (dev_scsi.wwn[0] != '\0') {
      
      char wwn_buf[100];
      char wwn_vendor_buf[100];
      
      memset( wwn_buf, 0, 100 );
      memset( wwn_vendor_buf, 0, 100 );
      
      vdev_property_add("VDEV_SCSI_WWN", dev_scsi.wwn );
      
      if (dev_scsi.wwn_vendor_extension[0] != '\0') {
         
         snprintf( wwn_buf, 99, "0x%s%s", dev_scsi.wwn, dev_scsi.wwn_vendor_extension );
         snprintf( wwn_vendor_buf, 99, "0x%s", dev_scsi.wwn_vendor_extension );
         
         vdev_property_add("VDEV_SCSI_WWN_VENDOR_EXTENSION", wwn_vendor_buf );
         vdev_property_add("VDEV_SCSI_WWN_WITH_EXTENSION", wwn_buf );
      }
      
      else {
         
         snprintf( wwn_buf, 99, "0x%s", dev_scsi.wwn );
         
         vdev_property_add("VDEV_SCSI_WWN_WITH_EXTENSION", wwn_buf );
      }
   }
   if (dev_scsi.tgpt_group[0] != '\0') {
      
      vdev_property_add("VDEV_SCSI_TARGET_PORT", dev_scsi.tgpt_group );
   }
   if (dev_scsi.unit_serial_number[0] != '\0') {
      
      vdev_property_add("VDEV_SCSI_UNIT_SERIAL", dev_scsi.unit_serial_number );
   }

   return retval;
}


// entry point
int main(int argc, char **argv) {
   
   int retval = 0;
   char maj_min_dev[MAX_PATH_LEN];
   int newargc;
   char **newargv = NULL;

   /*
   * Get config file options.
   */
   retval = get_file_options( NULL, NULL, &newargc, &newargv);
   if (retval < 0) {
      retval = 1;
      goto exit;
   }
   if (retval == 0) {
      
      if( newargv == NULL ) {
         retval = 4;
         goto exit;
      }

      if (set_options( newargc, newargv, maj_min_dev) < 0) {
         retval = 2;
         goto exit;
      }
   }

   /*
   * Get command line options (overriding any config file settings).
   */
   if (set_options( argc, argv, maj_min_dev) < 0) {
      exit(1);
   }

   if (!dev_specified) {
      log_error("%s", "No device specified.");
      retval = 1;
      goto exit;
   }

   retval = scsi_id( maj_min_dev);

exit:
   if (newargv) {
      free(newargv[0]);
      free(newargv);
   }
   
   vdev_property_print();
   vdev_property_free_all();
   
   return retval;
}
