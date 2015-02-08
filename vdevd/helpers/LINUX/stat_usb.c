/*
 * stat_usb - reads usb properties
 * 
 * Forked from systemd/src/udev/udev-builtin-usb_id.c on January 23, 2015.
 * (original from http://cgit.freedesktop.org/systemd/systemd/tree/src/udev/udev-builtin-usb_id.c)
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/*
 * USB device properties and persistent device path
 *
 * Copyright (c) 2005 SUSE Linux Products GmbH, Germany
 *   Author: Hannes Reinecke <hare@suse.de>
 *
 * Copyright (C) 2005-2011 Kay Sievers <kay@vrfy.org>
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

#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#define USB_DT_DEVICE                   0x01
#define USB_DT_INTERFACE                0x04


static void set_usb_iftype(char *to, int if_class_num, size_t len) {
   const char *type = "generic";

   switch (if_class_num) {
   case 1:
            type = "audio";
            break;
   case 2: /* CDC-Control */
            break;
   case 3:
            type = "hid";
            break;
   case 5: /* Physical */
            break;
   case 6:
            type = "media";
            break;
   case 7:
            type = "printer";
            break;
   case 8:
            type = "storage";
            break;
   case 9:
            type = "hub";
            break;
   case 0x0a: /* CDC-Data */
            break;
   case 0x0b: /* Chip/Smart Card */
            break;
   case 0x0d: /* Content Security */
            break;
   case 0x0e:
            type = "video";
            break;
   case 0xdc: /* Diagnostic Device */
            break;
   case 0xe0: /* Wireless Controller */
            break;
   case 0xfe: /* Application-specific */
            break;
   case 0xff: /* Vendor-specific */
            break;
   default:
            break;
   }
   
   strncpy(to, type, len);
   to[len-1] = '\0';
}

static int set_usb_mass_storage_ifsubtype(char *to, const char *from, size_t len) {
   
   int type_num = 0;
   char *eptr;
   const char *type = "generic";

   type_num = strtoul(from, &eptr, 0);
   if (eptr != from) {
      switch (type_num) {
      case 1: /* RBC devices */
            type = "rbc";
            break;
      case 2:
            type = "atapi";
            break;
      case 3:
            type = "tape";
            break;
      case 4: /* UFI */
            type = "floppy";
            break;
      case 6: /* Transparent SPC-2 devices */
            type = "scsi";
            break;
      default:
            break;
      }
   }
   
   strncpy( to, type, (strlen(type) < len ? strlen(type) : len) );
   
   return type_num;
}

static void set_scsi_type(char *to, const char *from, size_t len) {
   
   int type_num;
   char *eptr;
   const char *type = "generic";

   type_num = strtoul(from, &eptr, 0);
   if (eptr != from) {
      switch (type_num) {
      case 0:
      case 0xe:
            type = "disk";
            break;
      case 1:
            type = "tape";
            break;
      case 4:
      case 7:
      case 0xf:
            type = "optical";
            break;
      case 5:
            type = "cd";
            break;
      default:
            break;
      }
   }
   
   strncpy( to, type, (strlen(type) < len ? strlen(type) : len) );
}

// read packed descriptors information from sysfs
static int dev_if_packed_info( char const* sysfs_path, char *ifs_str, size_t len) {
   
   char sysfs_descriptors[4096];
   int fd = -1;
   ssize_t size;
   unsigned char buf[18 + 65535];
   int pos = 0;
   unsigned strpos = 0;
   int rc = 0;
   struct usb_interface_descriptor {
            u_int8_t        bLength;
            u_int8_t        bDescriptorType;
            u_int8_t        bInterfaceNumber;
            u_int8_t        bAlternateSetting;
            u_int8_t        bNumEndpoints;
            u_int8_t        bInterfaceClass;
            u_int8_t        bInterfaceSubClass;
            u_int8_t        bInterfaceProtocol;
            u_int8_t        iInterface;
   } __attribute__((packed));
   
   memset( sysfs_descriptors, 0, 4096 );
   snprintf( sysfs_descriptors, 4095, "%s/descriptors", sysfs_path );
   
   fd = open(sysfs_descriptors, O_RDONLY|O_CLOEXEC);
   if (fd < 0) {
      
      rc = -errno;
      log_debug("open('%s') errno = %d\n", sysfs_descriptors, rc );
      
      return rc;
   }

   size = read(fd, buf, sizeof(buf));
   if (size < 18 || size == sizeof(buf)) {
      
      rc = -errno;
      
      close( fd );
      log_debug("read('%s') rc = %zd, errno = %d\n", sysfs_path, size, rc );
      return -EIO;
   }

   close( fd );
   
   ifs_str[0] = '\0';
   while (pos < size && strpos+7 < len-2) {
      
      struct usb_interface_descriptor *desc;
      char if_str[8];

      desc = (struct usb_interface_descriptor *) &buf[pos];
      if (desc->bLength < 3) {
         break;
      }
      
      pos += desc->bLength;

      if (desc->bDescriptorType != USB_DT_INTERFACE) {
         continue;
      }

      if (snprintf(if_str, 8, ":%02x%02x%02x", desc->bInterfaceClass, desc->bInterfaceSubClass, desc->bInterfaceProtocol) != 7) {
         continue;
      }

      if (strstr(ifs_str, if_str) != NULL) {
         continue;
      }

      memcpy(&ifs_str[strpos], if_str, 8),
      strpos += 7;
   }

   if (strpos > 0) {
      ifs_str[strpos++] = ':';
      ifs_str[strpos++] = '\0';
   }

   return 0;
}


void usage(char const* progname) {
   fprintf(stderr, "Usage: %s /path/to/sysfs/device/directory\n", progname);
   exit(2);
}


// find out what kind of device this is from the sysfs device tree 
// fill in the caller-supplied pointer with a malloc'ed string that encodes the DEVTYPE 
int get_device_type( char const* sysfs_path, char** device_type, size_t* device_type_len ) {
   
   char uevent_path[4097];
   memset( uevent_path, 0, 4097 );
   
   char* uevent_buf = NULL;
   size_t uevent_len = 0;
   
   char* device_type_buf = NULL;
   size_t device_type_buf_len = 0;
   
   int rc = 0;
   
   snprintf(uevent_path, 4096, "%s/uevent", sysfs_path );
   
   rc = vdev_read_file( uevent_path, &uevent_buf, &uevent_len );
   if( rc != 0 ) {
      
      return rc;
   }
   
   rc = vdev_sysfs_uevent_get_key( uevent_buf, uevent_len, "DEVTYPE", &device_type_buf, &device_type_buf_len );
   if( rc != 0 ) {
      
      free( uevent_buf );
      return rc;
   }
   
   *device_type = device_type_buf;
   *device_type_len = device_type_buf_len;
   
   free( uevent_buf );
   
   return 0;
}

/*
 * A unique USB identification is generated like this:
 *
 * 1.) Get the USB device type from InterfaceClass and InterfaceSubClass
 * 2.) If the device type is 'Mass-Storage/SPC-2' or 'Mass-Storage/RBC',
 *     use the SCSI vendor and model as USB-Vendor and USB-model.
 * 3.) Otherwise, use the USB manufacturer and product as
 *     USB-Vendor and USB-model. Any non-printable characters
 *     in those strings will be skipped; a slash '/' will be converted
 *     into a full stop '.'.
 * 4.) If that fails, too, we will use idVendor and idProduct
 *     as USB-Vendor and USB-model.
 * 5.) The USB identification is the USB-vendor and USB-model
 *     string concatenated with an underscore '_'.
 * 6.) If the device supplies a serial number, this number
 *     is concatenated with the identification with an underscore '_'.
 */
int main( int argc, char **argv) {
   
   int rc = 0;
   char vendor_str[256];
   char vendor_str_enc[256];
   char vendor_id[256];
   char model_str[256];
   char model_str_enc[256];
   char product_id[256];
   char serial_str[256];
   char packed_if_str[256];
   char revision_str[256];
   char type_str[256];
   char instance_str[256];
   char driver[256];
   char ifnum[256];
   char serial[4096];
   char sysfs_base[8193];    // /sys/devices entry for this device
   char path_tmp[8193];
   
   bool confirm_usb = false;
   
   char* attr = NULL;    // temporary buffer for pulling attrs out of sysfs
   size_t attr_len = 0;
   
   int if_class_num;
   int protocol = 0;
   size_t l;
   char *s;
   char* devtype_str = NULL;
   size_t devtype_strlen = 0;
   
   // interface information
   char* if_device_path = NULL;
   size_t if_device_path_len= 0;
   
   char* usb_device_path = NULL;
   size_t usb_device_path_len = 0;
   
   char* ifnum_str = NULL;
   size_t ifnum_strlen = 0;
   
   char* driver_str = NULL;
   size_t driver_strlen = 0;
   
   char* if_class_str = NULL;
   size_t if_class_strlen = 0;
   
   char* if_subclass_str = NULL;
   size_t if_subclass_strlen = 0;
   
   char* parent_device_devpath = NULL;
   size_t parent_device_devpath_len = 0;
   
   struct stat sb;
   
   memset( sysfs_base, 0, 8193 );
   memset( path_tmp, 0, 8193 );
   
   memset( vendor_str, 0, 256 );
   memset( vendor_str_enc, 0, 256 );
   memset( vendor_id, 0, 256 );
   memset( model_str, 0, 256 );
   memset( model_str_enc, 0, 256 );
   memset( product_id, 0, 256 );
   memset( serial_str, 0, 256 );
   memset( packed_if_str, 0, 256 );
   memset( revision_str, 0, 256 );
   memset( type_str, 0, 256 );
   memset( instance_str, 0, 256 );
   memset( driver, 0, 256 );
   memset( ifnum, 0, 256 );
   memset( serial, 0, 4096 );
   
   if( argc != 2 ) {
      usage(argv[0] );
   }
   
   if( strlen(argv[1]) >= 4096 ) {
      fprintf(stderr, "Invalid /sys/devices path\n");
      exit(3);
   }
   
   strcpy( sysfs_base, argv[1] );
   
   // find out what kind of device we are...
   get_device_type( sysfs_base, &devtype_str, &devtype_strlen );
   
   if( devtype_str != NULL && strcmp(devtype_str, "usb_device") == 0 ) {
      
      // we're a device, and can get the packed info right now
      dev_if_packed_info( sysfs_base, packed_if_str, sizeof(packed_if_str) );
      
      usb_device_path = strdup( sysfs_base );
      if( usb_device_path == NULL ) {
         exit(4);
      }
      
      usb_device_path_len = strlen(usb_device_path);
      
      goto fallback;
   }
   
   // find USB parent 
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( sysfs_base, "usb", "usb_interface", &if_device_path, &if_device_path_len );
   if( rc != 0 ) {
      
      fprintf(stderr, "FATAL: unabe to access usb_interface device of '%s'\n", sysfs_base );
      exit(1);
   }
   
   // search *this* device instead
   // get interface information 
   rc = vdev_sysfs_read_attr( if_device_path, "bInterfaceNumber", &ifnum_str, &ifnum_strlen );
   if( rc == 0 ) {
      
      vdev_util_rstrip( ifnum_str );
      
      strncpy( ifnum, ifnum_str, (sizeof(ifnum) - 1 < ifnum_strlen ? sizeof(ifnum) - 1 : ifnum_strlen) );
      
      free( ifnum_str );
   }
   
   rc = vdev_sysfs_read_attr( if_device_path, "driver", &driver_str, &driver_strlen );
   if( rc == 0 ) {
      
      vdev_util_rstrip( driver_str );
      
      strncpy( driver, driver_str, (sizeof(driver) - 1 < driver_strlen ? sizeof(driver) - 1 : driver_strlen) );
      
      free( driver_str );
   }
   
   rc = vdev_sysfs_read_attr( if_device_path, "bInterfaceClass", &if_class_str, &if_class_strlen );
   if( rc != 0 ) {
      
      if( devtype_str != NULL ) {
         free( devtype_str );
      }
      
      fprintf(stderr, "FATAL: vdev_sysfs_read_attr('%s/%s') rc = %d\n", if_device_path, "bInterfaceClass", rc );
      exit(1);
   }
   
   // parse fields 
   if_class_num = strtoul( if_class_str, NULL, 16 );
   
   free( if_class_str );
   
   if( if_class_num == 8 ) {
      // indicates mass storage device.
      // check subclass
      rc = vdev_sysfs_read_attr( if_device_path, "bInterfaceSubClass", &if_subclass_str, &if_subclass_strlen );
      if( rc == 0 ) {
         
         // got ourselves a type of storage device 
         protocol = set_usb_mass_storage_ifsubtype( type_str, if_subclass_str, sizeof(type_str) - 1 );
      }
      else {
         
         // set type from interface instead 
         set_usb_iftype( type_str, if_class_num, sizeof(type_str) - 1 );
      }
      
      free( if_subclass_str );
   }
   
   // find the usb_device that is this interface's parent
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( if_device_path, "usb", "usb_device", &usb_device_path, &usb_device_path_len );
   if( rc != 0 ) {
      
      // couldn't find
      if( devtype_str != NULL ) {
         free( devtype_str );
      }
      
      fprintf( stderr, "FATAL: vdev_sysfs_get_parent_with_subsystem_devtype('%s', 'usb', 'usb_device') rc = %d\n", sysfs_base, rc );
      
      exit(1);
   }
   
   // got the device path!
   // get the device info
   dev_if_packed_info( usb_device_path, packed_if_str, sizeof(packed_if_str) );
   
   // is this a SCSI or ATAPI device?
   if ((protocol == 6 || protocol == 2)) {
      
      char* dev_scsi_path = NULL;
      size_t dev_scsi_path_len = 0;
      
      char* scsi_vendor_str = NULL;
      size_t scsi_vendor_str_len = 0;
      
      char* scsi_model_str = NULL;
      size_t scsi_model_str_len = 0;
      
      char* scsi_type_str = NULL;
      size_t scsi_type_str_len = 0;
      
      char* scsi_rev_str = NULL;
      size_t scsi_rev_str_len = 0;
      
      char* dev_scsi_sysname = NULL;
      size_t dev_scsi_sysname_len = 0;
      
      int host = 0;
      int bus = 0;
      int target = 0;
      int lun = 0;
      
      // scsi device?
      rc = vdev_sysfs_get_parent_with_subsystem_devtype( sysfs_base, "scsi", "scsi_device", &dev_scsi_path, &dev_scsi_path_len );
      if( rc != 0 ) {
         
         // nope 
         log_error( "WARN: unable to find parent 'scsi' of '%s'", sysfs_base );
         goto fallback;
      }
      
      rc = vdev_sysfs_get_sysname( dev_scsi_path, &dev_scsi_sysname, &dev_scsi_sysname_len );
      if( rc != 0 ) {
         
         // nope 
         free( dev_scsi_path );
         
         log_error("WARN: vdev_sysfs_get_sysname('%s') rc = %d\n", dev_scsi_path, rc );
         goto fallback;
      }
      
      // find host, bus, target, lun 
      rc = sscanf( dev_scsi_sysname, "%d:%d:%d:%d", &host, &bus, &target, &lun );
      if( rc != 4 ) {
         
         // nope 
         log_error( "WARN: unable to read host, bus, target, and/or lun from '%s'", dev_scsi_path );
         free( dev_scsi_path );
         free( dev_scsi_sysname );
         goto fallback;
      }
      
      // see about vendor?
      rc = vdev_sysfs_read_attr( dev_scsi_path, "vendor", &scsi_vendor_str, &scsi_vendor_str_len );
      if( rc != 0 ) {
         
         // nope 
         log_error("WARN: unable to read vendor string from '%s'", dev_scsi_path );
         free( dev_scsi_path );
         free( dev_scsi_sysname );
         goto fallback;
      }
      
      vdev_util_rstrip( scsi_vendor_str );
      
      vdev_util_encode_string( scsi_vendor_str, vendor_str_enc, sizeof(vendor_str_enc) );
      vdev_util_replace_whitespace( scsi_vendor_str, vendor_str, sizeof(vendor_str) - 1 );
      vdev_util_replace_chars( vendor_str, NULL );
      
      free( scsi_vendor_str );
      
      // see about model?
      rc = vdev_sysfs_read_attr( dev_scsi_path, "model", &scsi_model_str, &scsi_model_str_len );
      if( rc != 0 ) {
         
         // nope 
         log_error("WARN: unable to read model from '%s'", dev_scsi_path );
         free( dev_scsi_path );
         free( dev_scsi_sysname );
         
         goto fallback;
      }
      
      vdev_util_rstrip( scsi_model_str );
      
      vdev_util_encode_string( scsi_model_str, model_str_enc, sizeof(model_str_enc) );
      vdev_util_replace_whitespace( scsi_model_str, model_str, sizeof(model_str) - 1 );
      vdev_util_replace_chars( model_str, NULL );
      
      free( scsi_model_str );
      
      // see about type?
      rc = vdev_sysfs_read_attr( dev_scsi_path, "type", &scsi_type_str, &scsi_type_str_len );
      if( rc != 0 ) {
         
         // nope 
         log_error("WARN: unable to read type from '%s'", dev_scsi_path );
         free( dev_scsi_path );
         free( dev_scsi_sysname );
         
         goto fallback;
      }
      
      set_scsi_type( type_str, scsi_type_str, sizeof(type_str) - 1 );
      
      free( scsi_type_str );
      
      // see about revision?
      rc = vdev_sysfs_read_attr( dev_scsi_path, "rev", &scsi_rev_str, &scsi_rev_str_len );
      if( rc != 0 ) {
         
         // nope 
         log_error("WARN: unable to read revision from '%s'", dev_scsi_path );
         free( dev_scsi_path );
         free( dev_scsi_sysname );
         
         goto fallback;
      }
      
      vdev_util_replace_whitespace( scsi_rev_str, revision_str, sizeof(revision_str) - 1 );
      vdev_util_replace_chars( revision_str, NULL );
      
      free( scsi_rev_str );
      
      sprintf(instance_str, "%d:%d", target, lun );
      
      free( dev_scsi_path );
      free( dev_scsi_sysname );
   }

fallback:

   // not a device, and not an interface.
   // fall back to querying vendor and model information 
   rc = vdev_sysfs_read_attr( usb_device_path, "idVendor", &attr, &attr_len );
   if( rc == 0 ) {
      
      strncpy( vendor_id, attr, (attr_len < sizeof(vendor_id)-1 ? attr_len : sizeof(vendor_id)-1) );
      vdev_util_rstrip( vendor_id );
      
      free( attr );
      attr = NULL;
   }
   
   rc = vdev_sysfs_read_attr( usb_device_path, "idProduct", &attr, &attr_len );
   if( rc == 0 ) {
      
      strncpy( product_id, attr, (attr_len < sizeof(product_id)-1 ? attr_len : sizeof(product_id)-1) );
      vdev_util_rstrip( product_id );
      
      free( attr );
      attr = NULL;
   }
   
   if( vendor_str[0] == 0 ) {
      
      // vendor not known yet.  Try manufaturer, and fall back to idVendor if need be
      char* manufacturer_str = NULL;
      size_t manufacturer_strlen = 0;
      
      rc = vdev_sysfs_read_attr( usb_device_path, "manufacturer", &manufacturer_str, &manufacturer_strlen );
      if( rc != 0 ) {
         
         if( rc == -ENOENT ) {
            
            // fall back to idVendor
            vdev_util_encode_string( vendor_id, vendor_str_enc, sizeof(vendor_str_enc) );
            vdev_util_replace_whitespace( vendor_id, vendor_str, sizeof(vendor_str)-1 );
            vdev_util_replace_chars( vendor_str, NULL );
         }
         else {
            
            log_error("FATAL: vdev_sysfs_read_attr('%s/manufacturer') rc = %d\n", usb_device_path, rc );
            exit(1);
         }
      }
      else {
         
         // success!
         vdev_util_rstrip( manufacturer_str );
         
         vdev_util_encode_string( manufacturer_str, vendor_str_enc, sizeof(vendor_str_enc) );
         vdev_util_replace_whitespace( manufacturer_str, vendor_str, sizeof(vendor_str)-1 );
         vdev_util_replace_chars( vendor_str, NULL );
         
         free( manufacturer_str );
         manufacturer_str = NULL;
      }
   }
   
   if( model_str[0] == 0 ) {
      
      // model not known yet.  Try product, and fall back to idProduct if we fail 
      char* product_str = NULL;
      size_t product_strlen = 0;
      
      rc = vdev_sysfs_read_attr( usb_device_path, "product", &product_str, &product_strlen );
      if( rc != 0 ) {
         
         if( rc == -ENOENT ) {
            
            // fall back to idProduct 
            vdev_util_encode_string( product_id, model_str_enc, sizeof(model_str_enc) );
            vdev_util_replace_whitespace( product_id, vendor_str, sizeof(vendor_str) - 1 );
            vdev_util_replace_chars( vendor_str, NULL );
         }
         else {
            
            log_error("FATAL: vdev_sysfs_read_attr('%s/product') rc = %d\n", usb_device_path, rc );
            exit(1);
         }
      }
      else {
         
         // success!
         vdev_util_rstrip( product_str );
         
         vdev_util_encode_string( product_str, model_str_enc, sizeof(model_str_enc) );
         vdev_util_replace_whitespace( product_str, model_str, sizeof(model_str) - 1 );
         vdev_util_replace_chars( model_str, NULL );
         
         free( product_str );
         product_str = NULL;
      }
   }
   
   if( revision_str[0] == 0 ) {
      
      // revision not known yet. maybe it's in bcdDevice?
      rc = vdev_sysfs_read_attr( usb_device_path, "bcdDevice", &attr, &attr_len );
      if( rc == 0 ) {
         
         vdev_util_rstrip( attr );
         
         vdev_util_replace_whitespace( attr, revision_str, sizeof(revision_str) - 1 );
         vdev_util_replace_chars( revision_str, NULL );
         
         free( attr );
         attr = NULL;
      }
   }
   
   if( serial_str[0] == 0 ) {
      
      // serial number not known. Try 'serial'
      rc = vdev_sysfs_read_attr( usb_device_path, "serial", &attr, &attr_len );
      if( rc == 0 ) {
         
         vdev_util_rstrip( attr );
         
         const unsigned char *p = NULL;
         bool valid = true;

         /* http://msdn.microsoft.com/en-us/library/windows/hardware/gg487321.aspx */
         for (p = (unsigned char *)attr; *p != '\0'; p++) {
            if (*p < 0x20 || *p > 0x7f || *p == ',') {
               valid = false;
               break;
            }
         }
         
         if( valid ) {
            vdev_util_replace_whitespace( attr, serial_str, sizeof(serial_str) - 1 );
            vdev_util_replace_chars( serial_str, NULL );
         }
         
         free( attr );
         attr = NULL;
      }
   }
   
   // serialize everything 
   s = serial;
   sprintf( s, "%s_%s", vendor_str, model_str );
   l = strlen(s);
   
   if( serial_str[0] != 0 ) {
      sprintf( s + l, "_%s", serial_str );
      l = strlen(s);
   }
   
   if( instance_str[0] != 0 ) {
      sprintf( s + l, "-%s", instance_str );
      l = strlen(s);
   }
   
   if( vendor_str[0] != 0 ) {
      vdev_property_add( "VDEV_USB_VENDOR", vendor_str );
      confirm_usb = true;
   }
   
   if( vendor_str_enc[0] != 0 ) {
      vdev_property_add( "VDEV_USB_VENDOR_ENC", vendor_str_enc );
      confirm_usb = true;
   }
   
   if( vendor_id[0] != 0 ) {
      vdev_property_add( "VDEV_USB_VENDOR_ID", vendor_id );
      confirm_usb = true;
   }
   
   if( model_str[0] != 0 ) {
      vdev_property_add( "VDEV_USB_MODEL", model_str );
      confirm_usb = true;
   }
   
   if( model_str_enc[0] != 0 ) {
      vdev_property_add( "VDEV_USB_MODEL_ENC", model_str_enc );
      confirm_usb = true;
   }
   
   if( product_id[0] != 0 ) {
      vdev_property_add( "VDEV_USB_MODEL_ID", product_id );
      confirm_usb = true;
   }
   
   if( revision_str[0] != 0 ) {
      vdev_property_add( "VDEV_USB_REVISION", revision_str );
      confirm_usb = true;
   }
   
   if( strcmp(serial, "_") != 0 ) {
      
      // nonempty serial number 
      vdev_property_add( "VDEV_USB_SERIAL", serial );
      confirm_usb = true;
   }
   
   if( serial_str[0] != 0 ) {
      vdev_property_add( "VDEV_USB_SERIAL_SHORT", serial_str );
      confirm_usb = true;
   }
   
   if( type_str[0] != 0 ) {
      vdev_property_add( "VDEV_USB_TYPE", type_str );
      confirm_usb = true;
   }
   
   if( instance_str[0] != 0 ) {
      vdev_property_add( "VDEV_USB_INSTANCE", instance_str );
      confirm_usb = true;
   }
   
   if( packed_if_str[0] != 0 ) {
      vdev_property_add( "VDEV_USB_INTERFACES", packed_if_str );
      confirm_usb = true;
   }
   
   if( ifnum[0] != 0 ) {
      vdev_property_add( "VDEV_USB_INTERFACE_NUM", ifnum );
      confirm_usb = true;
   }
   
   if( driver[0] != 0 ) {
      vdev_property_add( "VDEV_USB_DRIVER", driver );
      confirm_usb = true;
   }
   
   if( confirm_usb ) {
      vdev_property_add( "VDEV_USB", "1" );
   }
   
   vdev_property_print();
   vdev_property_free_all();
   
   if( devtype_str != NULL ) {
      free( devtype_str );
   }
   
   return 0;
}
