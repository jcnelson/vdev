/*
 * stat_ata - reads product/serial number from ATA drives
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

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "common.h"

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

#define COMMAND_TIMEOUT_MSEC (30 * 1000)

static int
disk_scsi_inquiry_command (int fd, void *buf, size_t buf_len)
{

  uint8_t cdb[6];
  uint8_t sense[32];
  struct sg_io_v4 io_v4;
  struct sg_io_hdr io_hdr;
  int ret = 0;

  memset (sense, 0, 32 * sizeof (uint8_t));
  memset (&io_v4, 0, sizeof (struct sg_io_v4));
  memset (&io_hdr, 0, sizeof (struct sg_io_hdr));
  memset (cdb, 0, 6 * sizeof (uint8_t));

  /*
   * INQUIRY, see SPC-4 section 6.4
   */
  cdb[0] = 0x12;		/* OPERATION CODE: INQUIRY */
  cdb[3] = (buf_len >> 8);	/* ALLOCATION LENGTH */
  cdb[4] = (buf_len & 0xff);

  io_v4.guard = 'Q', io_v4.protocol = BSG_PROTOCOL_SCSI;
  io_v4.subprotocol = BSG_SUB_PROTOCOL_SCSI_CMD;
  io_v4.request_len = sizeof (cdb);
  io_v4.request = (uintptr_t) cdb;
  io_v4.max_response_len = sizeof (sense);
  io_v4.response = (uintptr_t) sense;
  io_v4.din_xfer_len = buf_len;
  io_v4.din_xferp = (uintptr_t) buf;
  io_v4.timeout = COMMAND_TIMEOUT_MSEC;

  ret = ioctl (fd, SG_IO, &io_v4);
  if (ret != 0)
    {
      /* could be that the driver doesn't do version 4, try version 3 */
      if (errno == EINVAL)
	{

	  io_hdr.interface_id = 'S';
	  io_hdr.cmdp = (unsigned char *) cdb;
	  io_hdr.cmd_len = sizeof (cdb);
	  io_hdr.dxferp = buf;
	  io_hdr.dxfer_len = buf_len;
	  io_hdr.sbp = sense;
	  io_hdr.mx_sb_len = sizeof (sense);
	  io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	  io_hdr.timeout = COMMAND_TIMEOUT_MSEC;

	  ret = ioctl (fd, SG_IO, &io_hdr);
	  if (ret != 0)
	    {

	      return ret;
	    }

	  /* even if the ioctl succeeds, we need to check the return value */
	  if (!(io_hdr.status == 0 &&
		io_hdr.host_status == 0 && io_hdr.driver_status == 0))
	    {
	      errno = EIO;

	      return -1;
	    }
	}
      else
	{

	  return ret;
	}
    }

  /* even if the ioctl succeeds, we need to check the return value */
  if (!(io_v4.device_status == 0 &&
	io_v4.transport_status == 0 && io_v4.driver_status == 0))
    {
      errno = EIO;

      return -1;
    }

  return 0;
}

static int
disk_identify_command (int fd, void *buf, size_t buf_len)
{

  uint8_t cdb[12];
  uint8_t sense[32] = { };
  uint8_t *desc = sense + 8;
  struct sg_io_v4 io_v4;
  struct sg_io_hdr io_hdr;
  int ret = 0;

  memset (cdb, 0, sizeof (uint8_t) * 12);
  memset (&io_v4, 0, sizeof (struct sg_io_v4));
  memset (&io_hdr, 0, sizeof (struct sg_io_hdr));

  /*
   * ATA Pass-Through 12 byte command, as described in
   *
   *  T10 04-262r8 ATA Command Pass-Through
   *
   * from http://www.t10.org/ftp/t10/document.04/04-262r8.pdf
   */
  cdb[0] = 0xa1;		/* OPERATION CODE: 12 byte pass through */
  cdb[1] = 4 << 1;		/* PROTOCOL: PIO Data-in */
  cdb[2] = 0x2e;		/* OFF_LINE=0, CK_COND=1, T_DIR=1, BYT_BLOK=1, T_LENGTH=2 */
  cdb[3] = 0;			/* FEATURES */
  cdb[4] = 1;			/* SECTORS */
  cdb[5] = 0;			/* LBA LOW */
  cdb[6] = 0;			/* LBA MID */
  cdb[7] = 0;			/* LBA HIGH */
  cdb[8] = 0 & 0x4F;		/* SELECT */
  cdb[9] = 0xEC;		/* Command: ATA IDENTIFY DEVICE */

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

  ret = ioctl (fd, SG_IO, &io_v4);
  if (ret != 0)
    {
      /* could be that the driver doesn't do version 4, try version 3 */
      if (errno == EINVAL)
	{

	  io_hdr.interface_id = 'S', io_hdr.cmdp = (unsigned char *) cdb;
	  io_hdr.cmd_len = sizeof (cdb);
	  io_hdr.dxferp = buf;
	  io_hdr.dxfer_len = buf_len;
	  io_hdr.sbp = sense;
	  io_hdr.mx_sb_len = sizeof (sense);
	  io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	  io_hdr.timeout = COMMAND_TIMEOUT_MSEC;

	  ret = ioctl (fd, SG_IO, &io_hdr);
	  if (ret != 0)
	    {
	      return ret;
	    }
	}
      else
	{
	  return ret;
	}
    }

  if (!(sense[0] == 0x72 && desc[0] == 0x9 && desc[1] == 0x0c))
    {
      errno = EIO;
      return -1;
    }

  return 0;
}

static int
disk_identify_packet_device_command (int fd, void *buf, size_t buf_len)
{

  uint8_t cdb[16];
  uint8_t sense[32];
  struct sg_io_v4 io_v4;
  struct sg_io_hdr io_hdr;
  uint8_t *desc = sense + 8;
  int ret = 0;

  memset (sense, 0, 32 * sizeof (uint8_t));
  memset (&io_v4, 0, sizeof (struct sg_io_v4));
  memset (&io_hdr, 0, sizeof (struct sg_io_hdr));

  /*
   * ATA Pass-Through 16 byte command, as described in
   *
   *  T10 04-262r8 ATA Command Pass-Through
   *
   * from http://www.t10.org/ftp/t10/document.04/04-262r8.pdf
   */
  cdb[0] = 0x85;		/* OPERATION CODE: 16 byte pass through */
  cdb[1] = 4 << 1;		/* PROTOCOL: PIO Data-in */
  cdb[2] = 0x2e;		/* OFF_LINE=0, CK_COND=1, T_DIR=1, BYT_BLOK=1, T_LENGTH=2 */
  cdb[3] = 0;			/* FEATURES */
  cdb[4] = 0;			/* FEATURES */
  cdb[5] = 0;			/* SECTORS */
  cdb[6] = 1;			/* SECTORS */
  cdb[7] = 0;			/* LBA LOW */
  cdb[8] = 0;			/* LBA LOW */
  cdb[9] = 0;			/* LBA MID */
  cdb[10] = 0;			/* LBA MID */
  cdb[11] = 0;			/* LBA HIGH */
  cdb[12] = 0;			/* LBA HIGH */
  cdb[13] = 0;			/* DEVICE */
  cdb[14] = 0xA1;		/* Command: ATA IDENTIFY PACKET DEVICE */
  cdb[15] = 0;			/* CONTROL */

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

  ret = ioctl (fd, SG_IO, &io_v4);
  if (ret != 0)
    {
      /* could be that the driver doesn't do version 4, try version 3 */
      if (errno == EINVAL)
	{

	  io_hdr.interface_id = 'S';
	  io_hdr.cmdp = (unsigned char *) cdb;
	  io_hdr.cmd_len = sizeof (cdb);
	  io_hdr.dxferp = buf;
	  io_hdr.dxfer_len = buf_len;
	  io_hdr.sbp = sense;
	  io_hdr.mx_sb_len = sizeof (sense);
	  io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	  io_hdr.timeout = COMMAND_TIMEOUT_MSEC;

	  ret = ioctl (fd, SG_IO, &io_hdr);
	  if (ret != 0)
	    {
	      return ret;
	    }

	}
      else
	{
	  return ret;
	}
    }

  if (!(sense[0] == 0x72 && desc[0] == 0x9 && desc[1] == 0x0c))
    {
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
static void
disk_identify_get_string (uint8_t identify[512],
			  unsigned int offset_words, char *dest,
			  size_t dest_len)
{

  unsigned int c1;
  unsigned int c2;

  while (dest_len > 0)
    {
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

static void
disk_identify_fixup_string (uint8_t identify[512],
			    unsigned int offset_words, size_t len)
{

  disk_identify_get_string (identify, offset_words,
			    (char *) identify + offset_words * 2, len);
}

static void
disk_identify_fixup_uint16 (uint8_t identify[512], unsigned int offset_words)
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
static int
disk_identify (int fd, uint8_t out_identify[512], int *out_is_packet_device)
{

  int ret;
  uint8_t inquiry_buf[36];
  int peripheral_device_type = 0;
  int all_nul_bytes = 1;
  int n = 0;
  int is_packet_device = 0;

  /* init results */
  memset (out_identify, 0, 512);

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
    {

      goto out;
    }

  /* SPC-4, section 6.4.2: Standard INQUIRY data */
  peripheral_device_type = inquiry_buf[0] & 0x1f;
  if (peripheral_device_type == 0x05)
    {

      is_packet_device = 1;
      ret = disk_identify_packet_device_command (fd, out_identify, 512);
      goto check_nul_bytes;
    }
  if (peripheral_device_type != 0x00)
    {
      ret = -1;
      errno = EIO;

      goto out;
    }

  /* OK, now issue the IDENTIFY DEVICE command */
  ret = disk_identify_command (fd, out_identify, 512);
  if (ret != 0)
    {

      goto out;
    }

check_nul_bytes:
  /* Check if IDENTIFY data is all NUL bytes - if so, bail */
  all_nul_bytes = 1;
  for (n = 0; n < 512; n++)
    {
      if (out_identify[n] != '\0')
	{
	  all_nul_bytes = 0;
	  break;
	}
    }

  if (all_nul_bytes)
    {

      ret = -1;
      errno = EIO;
      goto out;
    }

out:
  if (out_is_packet_device != NULL)
    {
      *out_is_packet_device = is_packet_device;
    }
  return ret;
}

// entry point 
int
main (int argc, char **argv)
{

  struct hd_driveid id;
  union
  {
    uint8_t byte[512];
    uint16_t wyde[256];
    uint64_t octa[64];
  } identify;

  int fd = 0;
  char model[41];
  char model_enc[256];
  char serial[21];
  char revision[9];
  const char *node = NULL;
  uint16_t word = 0;
  int is_packet_device = 0;
  int rc = 0;

  // check usage 
  if (argc != 2)
    {
      fprintf (stderr, "[ERROR] %s: Usage: %s /path/to/device/file\n",
	       argv[0], argv[0]);
      exit (1);
    }

  node = argv[1];

  fd = open (node, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    {
      fprintf (stderr, "[ERROR] %s: unable to open '%s'\n", argv[0], node);
      exit (1);
    }

  rc = disk_identify (fd, identify.byte, &is_packet_device);
  if (rc == 0)
    {
      /*
       * fix up only the fields from the IDENTIFY data that we are going to
       * use and copy it into the hd_driveid struct for convenience
       */
      disk_identify_fixup_string (identify.byte, 10, 20);	/* serial */
      disk_identify_fixup_string (identify.byte, 23, 8);	/* fwrev */
      disk_identify_fixup_string (identify.byte, 27, 40);	/* model */
      disk_identify_fixup_uint16 (identify.byte, 0);	/* configuration */
      disk_identify_fixup_uint16 (identify.byte, 75);	/* queue depth */
      disk_identify_fixup_uint16 (identify.byte, 75);	/* SATA capabilities */
      disk_identify_fixup_uint16 (identify.byte, 82);	/* command set supported */
      disk_identify_fixup_uint16 (identify.byte, 83);	/* command set supported */
      disk_identify_fixup_uint16 (identify.byte, 84);	/* command set supported */
      disk_identify_fixup_uint16 (identify.byte, 85);	/* command set supported */
      disk_identify_fixup_uint16 (identify.byte, 86);	/* command set supported */
      disk_identify_fixup_uint16 (identify.byte, 87);	/* command set supported */
      disk_identify_fixup_uint16 (identify.byte, 89);	/* time required for SECURITY ERASE UNIT */
      disk_identify_fixup_uint16 (identify.byte, 90);	/* time required for enhanced SECURITY ERASE UNIT */
      disk_identify_fixup_uint16 (identify.byte, 91);	/* current APM values */
      disk_identify_fixup_uint16 (identify.byte, 94);	/* current AAM value */
      disk_identify_fixup_uint16 (identify.byte, 128);	/* device lock function */
      disk_identify_fixup_uint16 (identify.byte, 217);	/* nominal media rotation rate */
      memcpy (&id, identify.byte, sizeof id);
    }
  else
    {

      /* If this fails, then try HDIO_GET_IDENTITY */
      if (ioctl (fd, HDIO_GET_IDENTITY, &id) != 0)
	{
	  int errsv = -errno;
	  fprintf (stderr,
		   "[ERROR] %s: HDIO_GET_IDENTITY failed for '%s': errno = %d\n",
		   argv[0], node, errsv);
	  close (fd);
	  exit (2);
	}
    }

  memcpy (model, id.model, 40);
  model[40] = '\0';

  vdev_util_encode_string (model, model_enc, sizeof (model_enc));
  vdev_util_replace_whitespace ((char *) id.model, model, 40);
  vdev_util_replace_chars (model, NULL);
  vdev_util_replace_whitespace ((char *) id.serial_no, serial, 20);
  vdev_util_replace_chars (serial, NULL);
  vdev_util_replace_whitespace ((char *) id.fw_rev, revision, 8);
  vdev_util_replace_chars (revision, NULL);

  /* Set this to convey the disk speaks the ATA protocol */
  vdev_property_add ("VDEV_ATA", "1");

  if ((id.config >> 8) & 0x80)
    {
      /* This is an ATAPI device */
      switch ((id.config >> 8) & 0x1f)
	{
	case 0:

	  vdev_property_add ("VDEV_ATA_TYPE", "cd");
	  break;

	case 1:

	  vdev_property_add ("VDEV_ATA_TYPE", "tape");
	  break;

	case 5:

	  vdev_property_add ("VDEV_ATA_TYPE", "cd");
	  break;

	case 7:

	  vdev_property_add ("VDEV_ATA_TYPE", "optical");
	  break;

	default:

	  vdev_property_add ("VDEV_ATA_TYPE", "generic");
	  break;
	}
    }
  else
    {

      vdev_property_add ("VDEV_ATA_TYPE", "disk");
    }

  vdev_property_add ("VDEV_ATA_MODEL", model);
  vdev_property_add ("VDEV_ATA_MODEL_ENC", model_enc);
  vdev_property_add ("VDEV_ATA_REVISION", revision);

  if (serial[0] != '\0')
    {

      char serial_buf[100];
      memset (serial_buf, 0, 100);

      snprintf (serial_buf, 99, "%s_%s", model, serial);

      vdev_property_add ("VDEV_ATA_SERIAL", serial_buf);
      vdev_property_add ("VDEV_ATA_SERIAL_SHORT", serial);

    }
  else
    {

      vdev_property_add ("VDEV_ATA_SERIAL", model);
    }

  if (id.command_set_1 & (1 << 5))
    {

      vdev_property_add ("VDEV_ATA_WRITE_CACHE", "1");
      vdev_property_add ("VDEV_ATA_WRITE_CACHE_ENABLED",
			 (id.cfs_enable_1 & (1 << 5)) ? "1" : "0");
    }
  if (id.command_set_1 & (1 << 10))
    {

      vdev_property_add ("VDEV_ATA_FEATURE_SET_HPA", "1");
      vdev_property_add ("VDEV_ATA_FEATURE_SET_HPA_ENABLED",
			 (id.cfs_enable_1 & (1 << 10)) ? "1" : "0");

      /*
       * TODO: use the READ NATIVE MAX ADDRESS command to get the native max address
       * so it is easy to check whether the protected area is in use.
       */
    }
  if (id.command_set_1 & (1 << 3))
    {

      vdev_property_add ("VDEV_ATA_FEATURE_SET_PM", "1");
      vdev_property_add ("VDEV_ATA_FEATURE_SET_PM_ENABLED",
			 (id.cfs_enable_1 & (1 << 3)) ? "1" : "0");
    }
  if (id.command_set_1 & (1 << 1))
    {

      char buf[100];
      memset (buf, 0, 100);
      snprintf (buf, 99, "%d", id.trseuc * 2);

      vdev_property_add ("VDEV_ATA_FEATURE_SET_SECURITY", "1");
      vdev_property_add ("VDEV_ATA_FEATURE_SET_SECURITY_ENABLED",
			 (id.cfs_enable_1 & (1 << 1)) ? "1" : "0");
      vdev_property_add ("VDEV_ATA_FEATURE_SET_SECURITY_ERASE_UNIT_MIN", buf);

      if ((id.cfs_enable_1 & (1 << 1)))
	{			/* enabled */

	  if (id.dlf & (1 << 8))
	    {

	      vdev_property_add
		("VDEV_ATA_FEATURE_SET_SECURITY_LEVEL", "maximum");
	    }
	  else
	    {

	      vdev_property_add
		("VDEV_ATA_FEATURE_SET_SECURITY_LEVEL", "high");
	    }
	}
      if (id.dlf & (1 << 5))
	{

	  memset (buf, 0, 100);
	  snprintf (buf, 99, "%d", id.trsEuc * 2);

	  vdev_property_add
	    ("VDEV_ATA_FEATURE_SET_SECURITY_ENHANCED_ERASE_UNIT_MIN", buf);
	}
      if (id.dlf & (1 << 4))
	{

	  vdev_property_add ("VDEV_ATA_FEATURE_SET_SECURITY_EXPIRE", "1");
	}
      if (id.dlf & (1 << 3))
	{

	  vdev_property_add ("VDEV_ATA_FEATURE_SET_SECURITY_FROZEN", "1");
	}
      if (id.dlf & (1 << 2))
	{

	  vdev_property_add ("VDEV_ATA_FEATURE_SET_SECURITY_LEVEL", "1");
	}
    }
  if (id.command_set_1 & (1 << 0))
    {

      vdev_property_add ("VDEV_ATA_FEATURE_SET_SMART", "1");
      vdev_property_add ("VDEV_ATA_FEATURE_SET_SMART_ENABLED",
			 (id.cfs_enable_1 & (1 << 0)) ? "1" : "0");
    }
  if (id.command_set_2 & (1 << 9))
    {

      char aam_buf[100];
      char aam_cur_buf[100];

      memset (aam_buf, 0, 100);
      memset (aam_cur_buf, 0, 100);

      snprintf (aam_buf, 99, "%d", id.acoustic >> 8);
      snprintf (aam_cur_buf, 99, "%d", id.acoustic & 0xff);

      vdev_property_add ("VDEV_ATA_FEATURE_SET_AAM", "1");
      vdev_property_add ("VDEV_ATA_FEATURE_SET_AAM_ENABLED",
			 (id.cfs_enable_2 & (1 << 9)) ? "1" : "0");
      vdev_property_add
	("VDEV_ATA_FEATURE_SET_AAM_VENDOR_RECOMMENDED_VALUE", aam_buf);
      vdev_property_add ("VDEV_ATA_FEATURE_SET_AAM_CURRENT_VALUE",
			 aam_cur_buf);
    }
  if (id.command_set_2 & (1 << 5))
    {

      vdev_property_add ("VDEV_ATA_FEATURE_SET_PUIS", "1");
      vdev_property_add ("VDEV_ATA_FEATURE_SET_PUIS_ENABLED",
			 (id.cfs_enable_2 & (1 << 5)) ? "1" : "0");
    }
  if (id.command_set_2 & (1 << 3))
    {

      vdev_property_add ("VDEV_ATA_FEATURE_SET_APM", "1");
      vdev_property_add ("VDEV_ATA_FEATURE_SET_APM_ENABLED",
			 (id.cfs_enable_2 & (1 << 3)) ? "1" : "0");

      if ((id.cfs_enable_2 & (1 << 3)))
	{

	  char apm_cur_buf[100];

	  memset (apm_cur_buf, 0, 100);

	  snprintf (apm_cur_buf, 99, "%d", id.CurAPMvalues & 0xff);

	  vdev_property_add
	    ("VDEV_ATA_FEATURE_SET_APM_CURRENT_VALUE", apm_cur_buf);
	}
    }
  if (id.command_set_2 & (1 << 0))
    {

      vdev_property_add ("VDEV_ATA_DOWNLOAD_MICROCODE", "1");
    }

  /*
   * Word 76 indicates the capabilities of a SATA device. A PATA device shall set
   * word 76 to 0000h or FFFFh. If word 76 is set to 0000h or FFFFh, then
   * the device does not claim compliance with the Serial ATA specification and words
   * 76 through 79 are not valid and shall be ignored.
   */

  word = identify.wyde[76];
  if (word != 0x0000 && word != 0xffff)
    {

      vdev_property_add ("VDEV_ATA_SATA", "1");
      /*
       * If bit 2 of word 76 is set to one, then the device supports the Gen2
       * signaling rate of 3.0 Gb/s (see SATA 2.6).
       *
       * If bit 1 of word 76 is set to one, then the device supports the Gen1
       * signaling rate of 1.5 Gb/s (see SATA 2.6).
       */
      if (word & (1 << 2))
	{

	  vdev_property_add ("VDEV_ATA_SATA_SIGNAL_RATE_GEN2", "1");
	}
      if (word & (1 << 1))
	{

	  vdev_property_add ("VDEV_ATA_SATA_SIGNAL_RATE_GEN1", "1");
	}
    }

  /* Word 217 indicates the nominal media rotation rate of the device */
  word = identify.wyde[217];
  if (word == 0x0001)
    {

      vdev_property_add ("VDEV_ATA_ROTATION_RATE_RPM", "0");
    }
  else if (word >= 0x0401 && word <= 0xfffe)
    {

      char rpm_buf[100];

      memset (rpm_buf, 0, 100);

      snprintf (rpm_buf, 99, "%d", word);

      vdev_property_add ("VDEV_ATA_ROTATION_RATE_RPM", rpm_buf);
    }

  /*
   * Words 108-111 contain a mandatory World Wide Name (WWN) in the NAA IEEE Registered identifier
   * format. Word 108 bits (15:12) shall contain 5h, indicating that the naming authority is IEEE.
   * All other values are reserved.
   */
  word = identify.wyde[108];
  if ((word & 0xf000) == 0x5000)
    {

      char wwn_buf[100];

      memset (wwn_buf, 0, 100);

      snprintf (wwn_buf, 99, "0x%04x%04x%04x%04x", identify.wyde[108],
		identify.wyde[109], identify.wyde[110], identify.wyde[111]);

      vdev_property_add ("VDEV_ATA_WWN", wwn_buf);
      vdev_property_add ("VDEV_ATA_WWN_WITH_EXTENSION", wwn_buf);
    }

  /* from Linux's include/linux/ata.h */
  if (identify.wyde[0] == 0x848a ||
      identify.wyde[0] == 0x844a || (identify.wyde[83] & 0xc004) == 0x4004)
    {

      vdev_property_add ("VDEV_ATA_CFA", "1");
    }

  vdev_property_print ();
  vdev_property_free_all ();

  return 0;
}
