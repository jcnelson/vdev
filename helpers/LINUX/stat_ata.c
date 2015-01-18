/*
 * ata_id - reads product/serial number from ATA drives
 * 
 * Forked from systemd/src/udev/ata_id/ata_id.c on January 7, 2015.
 * (original from https://github.com/systemd/systemd/blob/master/src/udev/ata_id/ata_id.c)
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/* 
 * Copyright (C) 2005-2008 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2009 Lennart Poettering <lennart@poettering.net>
 * Copyright (C) 2009-2010 David Zeuthen <zeuthen@gmail.com>
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

/* Parts of this file are based on the GLIB utf8 validation functions. The
 * original license text follows. */

/* gutf8.c - Operations on UTF-8 strings.
 *
 * Copyright (C) 1999 Tom Tromey
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <scsi/scsi_ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <linux/cdrom.h>
#include <linux/bsg.h>
#include <arpa/inet.h>
#include <inttypes.h>

#define COMMAND_TIMEOUT_MSEC (30 * 1000)

static int disk_scsi_inquiry_command(int fd, void *buf, size_t buf_len) {
      
   uint8_t cdb[6];
   uint8_t sense[32];
   struct sg_io_v4 io_v4;
   struct sg_io_hdr io_hdr;
   int ret = 0;
   
   memset( sense, 0, 32 * sizeof(uint8_t));
   memset( &io_v4, 0, sizeof(struct sg_io_v4) );
   memset( &io_hdr, 0, sizeof(struct sg_io_hdr) );
   
   /*
   * INQUIRY, see SPC-4 section 6.4
   */
   cdb[0] = 0x12;                 /* OPERATION CODE: INQUIRY */
   cdb[3] = (buf_len >> 8);      /* ALLOCATION LENGTH */
   cdb[4] = (buf_len & 0xff);
   
   io_v4.guard = 'Q',
   io_v4.protocol = BSG_PROTOCOL_SCSI;
   io_v4.subprotocol = BSG_SUB_PROTOCOL_SCSI_CMD;
   io_v4.request_len = sizeof(cdb);
   io_v4.request = (uintptr_t) cdb;
   io_v4.max_response_len = sizeof(sense);
   io_v4.response = (uintptr_t) sense;
   io_v4.din_xfer_len = buf_len;
   io_v4.din_xferp = (uintptr_t) buf;
   io_v4.timeout = COMMAND_TIMEOUT_MSEC;

   ret = ioctl(fd, SG_IO, &io_v4);
   if (ret != 0) {
      /* could be that the driver doesn't do version 4, try version 3 */
      if (errno == EINVAL) {
         
         io_hdr.interface_id = 'S';
         io_hdr.cmdp = (unsigned char*) cdb;
         io_hdr.cmd_len = sizeof (cdb);
         io_hdr.dxferp = buf;
         io_hdr.dxfer_len = buf_len;
         io_hdr.sbp = sense;
         io_hdr.mx_sb_len = sizeof(sense);
         io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
         io_hdr.timeout = COMMAND_TIMEOUT_MSEC;

         ret = ioctl(fd, SG_IO, &io_hdr);
         if (ret != 0) {
            return ret;
         }

         /* even if the ioctl succeeds, we need to check the return value */
         if (!(io_hdr.status == 0 &&
               io_hdr.host_status == 0 &&
               io_hdr.driver_status == 0)) {
                  errno = EIO;
                  return -1;
         }
      } else {
         return ret;
      }
   }

   /* even if the ioctl succeeds, we need to check the return value */
   if (!(io_v4.device_status == 0 &&
         io_v4.transport_status == 0 &&
         io_v4.driver_status == 0)) {
            errno = EIO;
            return -1;
   }

   return 0;
}

static int disk_identify_command(int fd, void *buf, size_t buf_len) {
   
   uint8_t cdb[12];
   uint8_t sense[32] = {};
   uint8_t *desc = sense + 8;
   struct sg_io_v4 io_v4;
   struct sg_io_hdr io_hdr;
   int ret = 0;
   
   memset( cdb, 0, sizeof(uint8_t) * 12 );
   memset( &io_v4, 0, sizeof(struct sg_io_v4) );
   memset( &io_hdr, 0, sizeof(struct sg_io_hdr) );

   /*
   * ATA Pass-Through 12 byte command, as described in
   *
   *  T10 04-262r8 ATA Command Pass-Through
   *
   * from http://www.t10.org/ftp/t10/document.04/04-262r8.pdf
   */
   cdb[0] = 0xa1;     /* OPERATION CODE: 12 byte pass through */
   cdb[1] = 4 << 1;   /* PROTOCOL: PIO Data-in */
   cdb[2] = 0x2e;     /* OFF_LINE=0, CK_COND=1, T_DIR=1, BYT_BLOK=1, T_LENGTH=2 */
   cdb[3] = 0;        /* FEATURES */
   cdb[4] = 1;        /* SECTORS */
   cdb[5] = 0;        /* LBA LOW */
   cdb[6] = 0;        /* LBA MID */
   cdb[7] = 0;        /* LBA HIGH */
   cdb[8] = 0 & 0x4F; /* SELECT */
   cdb[9] = 0xEC;     /* Command: ATA IDENTIFY DEVICE */
      
   io_v4.guard = 'Q';
   io_v4.protocol = BSG_PROTOCOL_SCSI;
   io_v4.subprotocol = BSG_SUB_PROTOCOL_SCSI_CMD;
   io_v4.request_len = sizeof(cdb);
   io_v4.request = (uintptr_t) cdb;
   io_v4.max_response_len = sizeof(sense);
   io_v4.response = (uintptr_t) sense;
   io_v4.din_xfer_len = buf_len;
   io_v4.din_xferp = (uintptr_t) buf;
   io_v4.timeout = COMMAND_TIMEOUT_MSEC;
   
   ret = ioctl(fd, SG_IO, &io_v4);
   if (ret != 0) {
      /* could be that the driver doesn't do version 4, try version 3 */
      if (errno == EINVAL) {
         
         io_hdr.interface_id = 'S',
         io_hdr.cmdp = (unsigned char*) cdb;
         io_hdr.cmd_len = sizeof (cdb);
         io_hdr.dxferp = buf;
         io_hdr.dxfer_len = buf_len;
         io_hdr.sbp = sense;
         io_hdr.mx_sb_len = sizeof (sense);
         io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
         io_hdr.timeout = COMMAND_TIMEOUT_MSEC;

         ret = ioctl(fd, SG_IO, &io_hdr);
         if (ret != 0) {
            return ret;
         }
      }
      else {
         return ret;
      }
   }

   if (!(sense[0] == 0x72 && desc[0] == 0x9 && desc[1] == 0x0c)) {
      errno = EIO;
      return -1;
   }

   return 0;
}


static int disk_identify_packet_device_command(int fd, void *buf, size_t buf_len) {
   
   uint8_t cdb[16];
   uint8_t sense[32];
   struct sg_io_v4 io_v4;
   struct sg_io_hdr io_hdr;
   uint8_t *desc = sense + 8;
   int ret = 0;
   
   memset( sense, 0, 32 * sizeof(uint8_t) );
   memset( &io_v4, 0, sizeof(struct sg_io_v4) );
   memset( &io_hdr, 0, sizeof(struct sg_io_hdr) );
   
   /*
   * ATA Pass-Through 16 byte command, as described in
   *
   *  T10 04-262r8 ATA Command Pass-Through
   *
   * from http://www.t10.org/ftp/t10/document.04/04-262r8.pdf
   */
   cdb[0] = 0x85;   /* OPERATION CODE: 16 byte pass through */
   cdb[1] = 4 << 1; /* PROTOCOL: PIO Data-in */
   cdb[2] = 0x2e;   /* OFF_LINE=0, CK_COND=1, T_DIR=1, BYT_BLOK=1, T_LENGTH=2 */
   cdb[3] = 0;      /* FEATURES */
   cdb[4] = 0;      /* FEATURES */
   cdb[5] = 0;      /* SECTORS */
   cdb[6] = 1;      /* SECTORS */
   cdb[7] = 0;      /* LBA LOW */
   cdb[8] = 0;      /* LBA LOW */
   cdb[9] = 0;      /* LBA MID */
   cdb[10] = 0;     /* LBA MID */
   cdb[11] = 0;     /* LBA HIGH */
   cdb[12] = 0;     /* LBA HIGH */
   cdb[13] = 0;     /* DEVICE */
   cdb[14] = 0xA1;  /* Command: ATA IDENTIFY PACKET DEVICE */
   cdb[15] = 0;     /* CONTROL */
   
   io_v4.guard = 'Q';
   io_v4.protocol = BSG_PROTOCOL_SCSI;
   io_v4.subprotocol = BSG_SUB_PROTOCOL_SCSI_CMD;
   io_v4.request_len = sizeof (cdb);
   io_v4.request = (uintptr_t) cdb;
   io_v4.max_response_len = sizeof (sense);
   io_v4.response = (uintptr_t) sense;
   io_v4.din_xfer_len = buf_len;
   io_v4.din_xferp = (uintptr_t) buf;
   io_v4.timeout = COMMAND_TIMEOUT_MSEC;

   ret = ioctl(fd, SG_IO, &io_v4);
   if (ret != 0) {
      /* could be that the driver doesn't do version 4, try version 3 */
      if (errno == EINVAL) {
         
         io_hdr.interface_id = 'S';
         io_hdr.cmdp = (unsigned char*) cdb;
         io_hdr.cmd_len = sizeof (cdb);
         io_hdr.dxferp = buf;
         io_hdr.dxfer_len = buf_len;
         io_hdr.sbp = sense;
         io_hdr.mx_sb_len = sizeof (sense);
         io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
         io_hdr.timeout = COMMAND_TIMEOUT_MSEC;

         ret = ioctl(fd, SG_IO, &io_hdr);
         if (ret != 0) {
            return ret;
         }
         
      }
      else {
         return ret;
      }
   }

   if (!(sense[0] == 0x72 && desc[0] == 0x9 && desc[1] == 0x0c)) {
      errno = EIO;
      return -1;
   }

   return 0;
}

/**
 * disk_identify_get_string:
 * @identify: A block of IDENTIFY data
 * @offset_words: Offset of the string to get, in words.
 * @dest: Destination buffer for the string.
 * @dest_len: Length of destination buffer, in bytes.
 *
 * Copies the ATA string from @identify located at @offset_words into @dest.
 */
static void disk_identify_get_string(uint8_t identify[512],
                                     unsigned int offset_words,
                                     char *dest,
                                     size_t dest_len)
{
        unsigned int c1;
        unsigned int c2;

        while (dest_len > 0) {
                c1 = identify[offset_words * 2 + 1];
                c2 = identify[offset_words * 2];
                *dest = c1;
                dest++;
                *dest = c2;
                dest++;
                offset_words++;
                dest_len -= 2;
        }
}

static void disk_identify_fixup_string(uint8_t identify[512],
                                       unsigned int offset_words,
                                       size_t len)
{
        disk_identify_get_string(identify, offset_words,
                                 (char *) identify + offset_words * 2, len);
}

static void disk_identify_fixup_uint16 (uint8_t identify[512], unsigned int offset_words)
{
        uint16_t *p;

        p = (uint16_t *) identify;
        p[offset_words] = le16toh (p[offset_words]);
}

/**
 * disk_identify:

 * @fd: File descriptor for the block device.
 * @out_identify: Return location for IDENTIFY data.
 * @out_is_packet_device: Return location for whether returned data is from a IDENTIFY PACKET DEVICE.
 *
 * Sends the IDENTIFY DEVICE or IDENTIFY PACKET DEVICE command to the
 * device represented by @fd. If successful, then the result will be
 * copied into @out_identify and @out_is_packet_device.
 *
 * This routine is based on code from libatasmart, Copyright 2008
 * Lennart Poettering, LGPL v2.1.
 *
 * Returns: 0 if the data was successfully obtained, otherwise
 * non-zero with errno set.
 */
static int disk_identify(int fd,
                         uint8_t out_identify[512],
                         int *out_is_packet_device)
{
        int ret;
        uint8_t inquiry_buf[36];
        int peripheral_device_type;
        int all_nul_bytes;
        int n;
        int is_packet_device = 0;

        /* init results */
        memset(out_identify, 0, 512);

        /* If we were to use ATA PASS_THROUGH (12) on an ATAPI device
         * we could accidentally blank media. This is because MMC's BLANK
         * command has the same op-code (0x61).
         *
         * To prevent this from happening we bail out if the device
         * isn't a Direct Access Block Device, e.g. SCSI type 0x00
         * (CD/DVD devices are type 0x05). So we send a SCSI INQUIRY
         * command first... libata is handling this via its SCSI
         * emulation layer.
         *
         * This also ensures that we're actually dealing with a device
         * that understands SCSI commands.
         *
         * (Yes, it is a bit perverse that we're tunneling the ATA
         * command through SCSI and relying on the ATA driver
         * emulating SCSI well-enough...)
         *
         * (See commit 160b069c25690bfb0c785994c7c3710289179107 for
         * the original bug-fix and see http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=556635
         * for the original bug-report.)
         */
        ret = disk_scsi_inquiry_command (fd, inquiry_buf, sizeof (inquiry_buf));
        if (ret != 0)
                goto out;

        /* SPC-4, section 6.4.2: Standard INQUIRY data */
        peripheral_device_type = inquiry_buf[0] & 0x1f;
        if (peripheral_device_type == 0x05)
          {
            is_packet_device = 1;
            ret = disk_identify_packet_device_command(fd, out_identify, 512);
            goto check_nul_bytes;
          }
        if (peripheral_device_type != 0x00) {
                ret = -1;
                errno = EIO;
                goto out;
        }

        /* OK, now issue the IDENTIFY DEVICE command */
        ret = disk_identify_command(fd, out_identify, 512);
        if (ret != 0)
                goto out;

 check_nul_bytes:
         /* Check if IDENTIFY data is all NUL bytes - if so, bail */
        all_nul_bytes = 1;
        for (n = 0; n < 512; n++) {
                if (out_identify[n] != '\0') {
                        all_nul_bytes = 0;
                        break;
                }
        }

        if (all_nul_bytes) {
                ret = -1;
                errno = EIO;
                goto out;
        }

out:
        if (out_is_packet_device != NULL)
                *out_is_packet_device = is_packet_device;
        return ret;
}

int util_replace_whitespace(const char *str, char *to, size_t len)
{
        size_t i, j;

        /* strip trailing whitespace */
        len = strnlen(str, len);
        while (len && isspace(str[len-1]))
                len--;

        /* strip leading whitespace */
        i = 0;
        while (isspace(str[i]) && (i < len))
                i++;

        j = 0;
        while (i < len) {
                /* substitute multiple whitespace with a single '_' */
                if (isspace(str[i])) {
                        while (isspace(str[i]))
                                i++;
                        to[j++] = '_';
                }
                to[j++] = str[i++];
        }
        to[j] = '\0';
        return 0;
}


int whitelisted_char_for_devnode(char c, const char *white) {
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            strchr("#+-.:=@_", c) != NULL ||
            (white != NULL && strchr(white, c) != NULL))
                return 1;
        return 0;
}


static inline bool is_unicode_valid(uint32_t ch) {

        if (ch >= 0x110000) /* End of unicode space */
                return false;
        if ((ch & 0xFFFFF800) == 0xD800) /* Reserved area for UTF-16 */
                return false;
        if ((ch >= 0xFDD0) && (ch <= 0xFDEF)) /* Reserved */
                return false;
        if ((ch & 0xFFFE) == 0xFFFE) /* BOM (Byte Order Mark) */
                return false;

        return true;
}

/* count of characters used to encode one unicode char */
static int utf8_encoded_expected_len(const char *str) {
        unsigned char c;
        
        c = (unsigned char) str[0];
        if (c < 0x80)
                return 1;
        if ((c & 0xe0) == 0xc0)
                return 2;
        if ((c & 0xf0) == 0xe0)
                return 3;
        if ((c & 0xf8) == 0xf0)
                return 4;
        if ((c & 0xfc) == 0xf8)
                return 5;
        if ((c & 0xfe) == 0xfc)
                return 6;

        return 0;
}

/* decode one unicode char */
int utf8_encoded_to_unichar(const char *str) {
        int unichar, len, i;

        len = utf8_encoded_expected_len(str);

        switch (len) {
        case 1:
                return (int)str[0];
        case 2:
                unichar = str[0] & 0x1f;
                break;
        case 3:
                unichar = (int)str[0] & 0x0f;
                break;
        case 4:
                unichar = (int)str[0] & 0x07;
                break;
        case 5:
                unichar = (int)str[0] & 0x03;
                break;
        case 6:
                unichar = (int)str[0] & 0x01;
                break;
        default:
                return -EINVAL;
        }

        for (i = 1; i < len; i++) {
                if (((int)str[i] & 0xc0) != 0x80)
                        return -EINVAL;
                unichar <<= 6;
                unichar |= (int)str[i] & 0x3f;
        }

        return unichar;
}

/* expected size used to encode one unicode char */
static int utf8_unichar_to_encoded_len(int unichar) {

        if (unichar < 0x80)
                return 1;
        if (unichar < 0x800)
                return 2;
        if (unichar < 0x10000)
                return 3;
        if (unichar < 0x200000)
                return 4;
        if (unichar < 0x4000000)
                return 5;

        return 6;
}

/* validate one encoded unicode char and return its length */
int utf8_encoded_valid_unichar(const char *str) {
        int len, unichar, i;

        len = utf8_encoded_expected_len(str);
        if (len == 0)
                return -EINVAL;

        /* ascii is valid */
        if (len == 1)
                return 1;

        /* check if expected encoded chars are available */
        for (i = 0; i < len; i++)
                if ((str[i] & 0x80) != 0x80)
                        return -EINVAL;

        unichar = utf8_encoded_to_unichar(str);

        /* check if encoded length matches encoded value */
        if (utf8_unichar_to_encoded_len(unichar) != len)
                return -EINVAL;

        /* check if value has valid range */
        if (!is_unicode_valid(unichar))
                return -EINVAL;

        return len;
}

/* allow chars in whitelist, plain ascii, hex-escaping and valid utf8 */
int util_replace_chars(char *str, const char *white)
{
        size_t i = 0;
        int replaced = 0;

        while (str[i] != '\0') {
                int len;

                if (whitelisted_char_for_devnode(str[i], white)) {
                        i++;
                        continue;
                }

                /* accept hex encoding */
                if (str[i] == '\\' && str[i+1] == 'x') {
                        i += 2;
                        continue;
                }

                /* accept valid utf8 */
                len = utf8_encoded_valid_unichar(&str[i]);
                if (len > 1) {
                        i += len;
                        continue;
                }

                /* if space is allowed, replace whitespace with ordinary space */
                if (isspace(str[i]) && white != NULL && strchr(white, ' ') != NULL) {
                        str[i] = ' ';
                        i++;
                        replaced++;
                        continue;
                }

                /* everything else is replaced with '_' */
                str[i] = '_';
                i++;
                replaced++;
        }
        return replaced;
}

/**
 * udev_util_encode_string:
 * @str: input string to be encoded
 * @str_enc: output string to store the encoded input string
 * @len: maximum size of the output string, which may be
 *       four times as long as the input string
 *
 * Encode all potentially unsafe characters of a string to the
 * corresponding 2 char hex value prefixed by '\x'.
 *
 * Returns: 0 if the entire string was copied, non-zero otherwise.
 **/
int udev_util_encode_string(const char *str, char *str_enc, size_t len)
{
        size_t i, j;

        if (str == NULL || str_enc == NULL)
                return -EINVAL;

        for (i = 0, j = 0; str[i] != '\0'; i++) {
                int seqlen;

                seqlen = utf8_encoded_valid_unichar(&str[i]);
                if (seqlen > 1) {
                        if (len-j < (size_t)seqlen)
                                goto err;
                        memcpy(&str_enc[j], &str[i], seqlen);
                        j += seqlen;
                        i += (seqlen-1);
                } else if (str[i] == '\\' || !whitelisted_char_for_devnode(str[i], NULL)) {
                        if (len-j < 4)
                                goto err;
                        sprintf(&str_enc[j], "\\x%02x", (unsigned char) str[i]);
                        j += 4;
                } else {
                        if (len-j < 1)
                                goto err;
                        str_enc[j] = str[i];
                        j++;
                }
        }
        if (len-j < 1)
                goto err;
        str_enc[j] = '\0';
        return 0;
err:
        return -EINVAL;
}


int main(int argc, char *argv[])
{
        struct hd_driveid id;
        union {
                uint8_t  byte[512];
                uint16_t wyde[256];
                uint64_t octa[64];
        } identify;
        
        int fd = 0;
        char model[41];
        char model_enc[256];
        char serial[21];
        char revision[9];
        const char *node = NULL;
        int export_vars = 0;
        uint16_t word;
        int is_packet_device = 0;
        static const struct option options[] = {
                { "export", no_argument, NULL, 'x' },
                { "help", no_argument, NULL, 'h' },
                {}
        };

        while (1) {
                int option;

                option = getopt_long(argc, argv, "xh", options, NULL);
                if (option == -1)
                        break;

                switch (option) {
                case 'x':
                        export_vars = 1;
                        break;
                case 'h':
                        printf("Usage: ata_id [--export] [--help] <device>\n"
                               "  -x,--export_vars    print values as environment keys\n"
                               "  -h,--help      print this help text\n\n");
                        return 0;
                }
        }

        node = argv[optind];
        if (node == NULL) {
           
                fprintf(stderr, "no node specified");
                return 1;
        }

        fd = open(node, O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if (fd < 0) {
            fprintf(stderr, "unable to open '%s'\n", node);
            return 1;
        }

        if (disk_identify(fd, identify.byte, &is_packet_device) == 0) {
            /*
            * fix up only the fields from the IDENTIFY data that we are going to
            * use and copy it into the hd_driveid struct for convenience
            */
            disk_identify_fixup_string(identify.byte,  10, 20); /* serial */
            disk_identify_fixup_string(identify.byte,  23,  8); /* fwrev */
            disk_identify_fixup_string(identify.byte,  27, 40); /* model */
            disk_identify_fixup_uint16(identify.byte,  0);      /* configuration */
            disk_identify_fixup_uint16(identify.byte,  75);     /* queue depth */
            disk_identify_fixup_uint16(identify.byte,  75);     /* SATA capabilities */
            disk_identify_fixup_uint16(identify.byte,  82);     /* command set supported */
            disk_identify_fixup_uint16(identify.byte,  83);     /* command set supported */
            disk_identify_fixup_uint16(identify.byte,  84);     /* command set supported */
            disk_identify_fixup_uint16(identify.byte,  85);     /* command set supported */
            disk_identify_fixup_uint16(identify.byte,  86);     /* command set supported */
            disk_identify_fixup_uint16(identify.byte,  87);     /* command set supported */
            disk_identify_fixup_uint16(identify.byte,  89);     /* time required for SECURITY ERASE UNIT */
            disk_identify_fixup_uint16(identify.byte,  90);     /* time required for enhanced SECURITY ERASE UNIT */
            disk_identify_fixup_uint16(identify.byte,  91);     /* current APM values */
            disk_identify_fixup_uint16(identify.byte,  94);     /* current AAM value */
            disk_identify_fixup_uint16(identify.byte, 128);     /* device lock function */
            disk_identify_fixup_uint16(identify.byte, 217);     /* nominal media rotation rate */
            memcpy(&id, identify.byte, sizeof id);
        } else {
            /* If this fails, then try HDIO_GET_IDENTITY */
            if (ioctl(fd, HDIO_GET_IDENTITY, &id) != 0) {
               int errsv = -errno;
               fprintf( stderr, "HDIO_GET_IDENTITY failed for '%s': errno = %d\n", node, errsv);
               return 2;
            }
        }

        memcpy (model, id.model, 40);
        model[40] = '\0';
        
        udev_util_encode_string(model, model_enc, sizeof(model_enc));
        util_replace_whitespace((char *) id.model, model, 40);
        util_replace_chars(model, NULL);
        util_replace_whitespace((char *) id.serial_no, serial, 20);
        util_replace_chars(serial, NULL);
        util_replace_whitespace((char *) id.fw_rev, revision, 8);
        util_replace_chars(revision, NULL);

        if (export_vars) {
                /* Set this to convey the disk speaks the ATA protocol */
                printf("VDEV_ATA=1\n");

                if ((id.config >> 8) & 0x80) {
                        /* This is an ATAPI device */
                        switch ((id.config >> 8) & 0x1f) {
                        case 0:
                                printf("VDEV_ATA_TYPE=cd\n");
                                break;
                        case 1:
                                printf("VDEV_ATA_TYPE=tape\n");
                                break;
                        case 5:
                                printf("VDEV_ATA_TYPE=cd\n");
                                break;
                        case 7:
                                printf("VDEV_ATA_TYPE=optical\n");
                                break;
                        default:
                                printf("VDEV_ATA_TYPE=generic\n");
                                break;
                        }
                } else {
                        printf("VDEV_ATA_TYPE=disk\n");
                }
                printf("VDEV_ATA_BUS=ata\n");
                printf("VDEV_ATA_MODEL=%s\n", model);
                printf("VDEV_ATA_MODEL_ENC=%s\n", model_enc);
                printf("VDEV_ATA_REVISION=%s\n", revision);
                if (serial[0] != '\0') {
                        printf("VDEV_ATA_SERIAL=%s_%s\n", model, serial);
                        printf("VDEV_ATA_SERIAL_SHORT=%s\n", serial);
                } else {
                        printf("VDEV_ATA_SERIAL=%s\n", model);
                }

                if (id.command_set_1 & (1<<5)) {
                        printf("VDEV_ATA_WRITE_CACHE=1\n");
                        printf("VDEV_ATA_WRITE_CACHE_ENABLED=%d\n", (id.cfs_enable_1 & (1<<5)) ? 1 : 0);
                }
                if (id.command_set_1 & (1<<10)) {
                        printf("VDEV_ATA_FEATURE_SET_HPA=1\n");
                        printf("VDEV_ATA_FEATURE_SET_HPA_ENABLED=%d\n", (id.cfs_enable_1 & (1<<10)) ? 1 : 0);

                        /*
                         * TODO: use the READ NATIVE MAX ADDRESS command to get the native max address
                         * so it is easy to check whether the protected area is in use.
                         */
                }
                if (id.command_set_1 & (1<<3)) {
                        printf("VDEV_ATA_FEATURE_SET_PM=1\n");
                        printf("VDEV_ATA_FEATURE_SET_PM_ENABLED=%d\n", (id.cfs_enable_1 & (1<<3)) ? 1 : 0);
                }
                if (id.command_set_1 & (1<<1)) {
                        printf("VDEV_ATA_FEATURE_SET_SECURITY=1\n");
                        printf("VDEV_ATA_FEATURE_SET_SECURITY_ENABLED=%d\n", (id.cfs_enable_1 & (1<<1)) ? 1 : 0);
                        printf("VDEV_ATA_FEATURE_SET_SECURITY_ERASE_UNIT_MIN=%d\n", id.trseuc * 2);
                        if ((id.cfs_enable_1 & (1<<1))) /* enabled */ {
                                if (id.dlf & (1<<8))
                                        printf("VDEV_ATA_FEATURE_SET_SECURITY_LEVEL=maximum\n");
                                else
                                        printf("VDEV_ATA_FEATURE_SET_SECURITY_LEVEL=high\n");
                        }
                        if (id.dlf & (1<<5))
                                printf("VDEV_ATA_FEATURE_SET_SECURITY_ENHANCED_ERASE_UNIT_MIN=%d\n", id.trsEuc * 2);
                        if (id.dlf & (1<<4))
                                printf("VDEV_ATA_FEATURE_SET_SECURITY_EXPIRE=1\n");
                        if (id.dlf & (1<<3))
                                printf("VDEV_ATA_FEATURE_SET_SECURITY_FROZEN=1\n");
                        if (id.dlf & (1<<2))
                                printf("VDEV_ATA_FEATURE_SET_SECURITY_LOCKED=1\n");
                }
                if (id.command_set_1 & (1<<0)) {
                        printf("VDEV_ATA_FEATURE_SET_SMART=1\n");
                        printf("VDEV_ATA_FEATURE_SET_SMART_ENABLED=%d\n", (id.cfs_enable_1 & (1<<0)) ? 1 : 0);
                }
                if (id.command_set_2 & (1<<9)) {
                        printf("VDEV_ATA_FEATURE_SET_AAM=1\n");
                        printf("VDEV_ATA_FEATURE_SET_AAM_ENABLED=%d\n", (id.cfs_enable_2 & (1<<9)) ? 1 : 0);
                        printf("VDEV_ATA_FEATURE_SET_AAM_VENDOR_RECOMMENDED_VALUE=%d\n", id.acoustic >> 8);
                        printf("VDEV_ATA_FEATURE_SET_AAM_CURRENT_VALUE=%d\n", id.acoustic & 0xff);
                }
                if (id.command_set_2 & (1<<5)) {
                        printf("VDEV_ATA_FEATURE_SET_PUIS=1\n");
                        printf("VDEV_ATA_FEATURE_SET_PUIS_ENABLED=%d\n", (id.cfs_enable_2 & (1<<5)) ? 1 : 0);
                }
                if (id.command_set_2 & (1<<3)) {
                        printf("VDEV_ATA_FEATURE_SET_APM=1\n");
                        printf("VDEV_ATA_FEATURE_SET_APM_ENABLED=%d\n", (id.cfs_enable_2 & (1<<3)) ? 1 : 0);
                        if ((id.cfs_enable_2 & (1<<3)))
                                printf("VDEV_ATA_FEATURE_SET_APM_CURRENT_VALUE=%d\n", id.CurAPMvalues & 0xff);
                }
                if (id.command_set_2 & (1<<0))
                        printf("VDEV_ATA_DOWNLOAD_MICROCODE=1\n");

                /*
                 * Word 76 indicates the capabilities of a SATA device. A PATA device shall set
                 * word 76 to 0000h or FFFFh. If word 76 is set to 0000h or FFFFh, then
                 * the device does not claim compliance with the Serial ATA specification and words
                 * 76 through 79 are not valid and shall be ignored.
                 */

                word = identify.wyde[76];
                if (word != 0x0000 && word != 0xffff) {
                        printf("VDEV_ATA_SATA=1\n");
                        /*
                         * If bit 2 of word 76 is set to one, then the device supports the Gen2
                         * signaling rate of 3.0 Gb/s (see SATA 2.6).
                         *
                         * If bit 1 of word 76 is set to one, then the device supports the Gen1
                         * signaling rate of 1.5 Gb/s (see SATA 2.6).
                         */
                        if (word & (1<<2))
                                printf("VDEV_ATA_SATA_SIGNAL_RATE_GEN2=1\n");
                        if (word & (1<<1))
                                printf("VDEV_ATA_SATA_SIGNAL_RATE_GEN1=1\n");
                }

                /* Word 217 indicates the nominal media rotation rate of the device */
                word = identify.wyde[217];
                if (word == 0x0001)
                        printf ("VDEV_ATA_ROTATION_RATE_RPM=0\n"); /* non-rotating e.g. SSD */
                else if (word >= 0x0401 && word <= 0xfffe)
                        printf ("VDEV_ATA_ROTATION_RATE_RPM=%d\n", word);

                /*
                 * Words 108-111 contain a mandatory World Wide Name (WWN) in the NAA IEEE Registered identifier
                 * format. Word 108 bits (15:12) shall contain 5h, indicating that the naming authority is IEEE.
                 * All other values are reserved.
                 */
                word = identify.wyde[108];
                if ((word & 0xf000) == 0x5000)
                        printf("VDEV_ATA_WWN=0x%1$" PRIu64 "x\n"
                               "VDEV_ATA_WWN_WITH_EXTENSION=0x%1$" PRIu64 "x\n",
                               identify.octa[108/4]);

                /* from Linux's include/linux/ata.h */
                if (identify.wyde[0] == 0x848a ||
                    identify.wyde[0] == 0x844a ||
                    (identify.wyde[83] & 0xc004) == 0x4004)
                        printf("VDEV_ATA_CFA=1\n");
        } else {
                if (serial[0] != '\0')
                        printf("%s_%s\n", model, serial);
                else
                        printf("%s\n", model);
        }

        return 0;
}
