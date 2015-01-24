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


#define DEBUG 1

#define log_debug(x, ...) if( DEBUG ) { fprintf(stderr, "DEBUG: " x "\n", __VA_ARGS__); }


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
   strscpy(to, len, type);
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
   strscpy(to, len, type);
}

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
      log_debug("read('%s') rc = %zd, errno = %d\n", size, rc );
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
   fprintf(stderr, "Usage: %s /path/to/usb/device/file\n", progname);
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
   
   char vendor_str[64];
   char vendor_str_enc[256];
   const char *vendor_id;
   char model_str[64];
   char model_str_enc[256];
   const char *product_id;
   char serial_str[UTIL_NAME_SIZE];
   char packed_if_str[UTIL_NAME_SIZE];
   char revision_str[64];
   char type_str[64];
   char instance_str[64];
   const char *ifnum = NULL;
   const char *driver = NULL;
   char serial[256];
   char sysfs_base[8193];
   char path_tmp[8193];
   
   struct udev_device *dev_interface = NULL;
   struct udev_device *dev_usb = NULL;
   const char *if_class, *if_subclass;
   int if_class_num;
   int protocol = 0;
   size_t l;
   char *s;
   struct stat sb;
   char* devtype_str = getenv("VDEV_OS_DEVTYPE");
   
   vendor_str[0] = '\0';
   model_str[0] = '\0';
   serial_str[0] = '\0';
   packed_if_str[0] = '\0';
   revision_str[0] = '\0';
   type_str[0] = '\0';
   instance_str[0] = '\0';
   memset( sysfs_base, 0, 8193 );
   
   // does this correspond to a usb block or character device?
   if( argv >= 2 ) {
         
      rc = stat( argv[1], &sb );
      if( rc != 0 ) {
         
         rc = -errno;
         fprintf(stderr, "stat('%s') errno = %d\n", argv[1], rc );
         exit(2);
      }
      
      if( S_ISCHR( sb.st_mode ) ) {
         
         sprintf(sysfs_base, "/sys/dev/char/%u:%u", major(sb.st_rdev), minor(sb.st_rdev) );
      }
      else if( S_ISBLK( sb.st_mode ) ) {
         
         sprintf(sysfs_base, "/sys/dev/block/%u:%u", major(sb.st_rdev), minor(sb.st_rdev) );
      }
      
      // look up the /devices path 
      rc = readlink( sysfs_base, path_tmp, 8193 );
      if( rc < 0 ) {
         
         rc = -errno;
         fprintf(stderr, "readlink('%s') errno = %d\n", sysfs_base, rc );
         exit(2);
      }
      
      strcat( sysfs_base, path_tmp );
      char* tmp = realpath( sysfs_base, path_tmp );
      
      if( tmp == NULL ) {
         
         rc = -errno;
         fprintf(stderr, "realpath('%s') errno = %d\n", sysfs_base, rc );
         exit(2);
      }
   }
   else {
      
      // some other USB device
      if( getenv("VDEV_OS_DEVPATH") != NULL ) {
         sprintf( sysfs_base, "/sys/%s", getenv("VDEV_OS_DEVPATH") );
      }
      else {
         // deduce from vdev environment variables 
         // TODO 
         usage(argv[0]);
         exit(1);
      }
   }
   
   if( devtype_str != NULL && strcmp(devtype_str, "usb_device") == 0 ) {
      
      // we're a device!
      dev_if_packed_info( sysfs_base, packed_if_str, sizeof(packed_if_str) );
      
      // dev_usb = dev
      goto fallback;
   }
   
   if( devtype_str != NULL && strcmp(devtype_str, "usb_interface") == 0 ) {
      
      // we're an interface
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
      
      rc = vdev_sysfs_read_attr( sysfs_base, "bInterfaceNumber", &ifnum_str, &ifnum_strlen );
      if( rc != 0 ) {
         
         fprintf(stderr, "FATAL: vdev_sysfs_read_attr('%s/%s') rc = %d\n", sysfs_base, "bInterfaceNumber", rc );
         exit(1);
      }
      
      rc = vdev_sysfs_read_attr( sysfs_base, "driver", &driver_str, &driver_strlen );
      if( rc != 0 ) {
         
         free( ifnum_str );
         
         fprintf(stderr, "FATAL: vdev_sysfs_read_attr('%s/%s') rc = %d\n", sysfs_base, "driver", rc );
         exit(1);
      }
      
      rc = vdev_sysfs_read_attr( sysfs_base, "bInterfaceClass", &if_class_str, &if_class_strlen );
      if( rc != 0 ) {
         
         free( ifnum_str );
         free( driver_str );
         
         fprintf(stderr, "FATAL: vdev_sysfs_read_attr('%s/%s') rc = %d\n", sysfs_base, "bInterfaceClass", rc );
         exit(1);
      }
      
      // parse fields 
      if_class_num = strtoul( if_class, NULL, 16 );
      
      if( if_class_num == 8 ) {
         // indicates mass storage device.
         // check subclass
         rc = vdev_sysfs_read_attr( sysfs_base, "bInterfaceSubClass", &if_subclass_str, &if_subclass_strlen );
         if( rc == 0 ) {
            
            // got ourselves a type of storage device 
            protocol = set_usb_mass_storage_ifsubtype( type_str, if_subclass_str, sizeof(type_str) - 1 );
            
            // SCSI or ATAPI?
            if ((protocol == 6 || protocol == 2)) {
         
               char* dev_scsi_path = NULL;
               size_t dev_scsi_path_len = 0;
               
               int host = 0;
               int bus = 0;
               int target = 0;
               int lun = 0;
               
               // scsi device?
               rc = vdev_sysfs_get_parent_with_subsystem_devtype( sysfs_base, "scsi", "scsi_device", &dev_scsi_path, &dev_scsi_path_len );
               if( rc != 0 ) {
                  
                  // nope 
                  fprintf(stderr, "WARN: unable to find parent 'scsi' of '%s'\n", sysfs_base );
                  goto fallback;
               }
               
               // find host, bus, target, lun 
               
               
               
               
               
               
               
               
               
               
               
               
               
               
               
               
               
               struct udev_device *dev_scsi;
               const char *scsi_model, *scsi_vendor, *scsi_type, *scsi_rev;
               int host, bus, target, lun;

               /* get scsi device */
               dev_scsi = udev_device_get_parent_with_subsystem_devtype(dev, "scsi", "scsi_device");
               if (dev_scsi == NULL) {
                     log_debug("unable to find parent 'scsi' device of '%s'",
                           udev_device_get_syspath(dev));
                     goto fallback;
               }
               if (sscanf(udev_device_get_sysname(dev_scsi), "%d:%d:%d:%d", &host, &bus, &target, &lun) != 4) {
                     log_debug("invalid scsi device '%s'", udev_device_get_sysname(dev_scsi));
                     goto fallback;
               }

               /* Generic SPC-2 device */
               scsi_vendor = udev_device_get_sysattr_value(dev_scsi, "vendor");
               if (!scsi_vendor) {
                     log_debug("%s: cannot get SCSI vendor attribute",
                           udev_device_get_sysname(dev_scsi));
                     goto fallback;
               }
               udev_util_encode_string(scsi_vendor, vendor_str_enc, sizeof(vendor_str_enc));
               util_replace_whitespace(scsi_vendor, vendor_str, sizeof(vendor_str)-1);
               util_replace_chars(vendor_str, NULL);

               scsi_model = udev_device_get_sysattr_value(dev_scsi, "model");
               if (!scsi_model) {
                     log_debug("%s: cannot get SCSI model attribute",
                           udev_device_get_sysname(dev_scsi));
                     goto fallback;
               }
               udev_util_encode_string(scsi_model, model_str_enc, sizeof(model_str_enc));
               util_replace_whitespace(scsi_model, model_str, sizeof(model_str)-1);
               util_replace_chars(model_str, NULL);

               scsi_type = udev_device_get_sysattr_value(dev_scsi, "type");
               if (!scsi_type) {
                     log_debug("%s: cannot get SCSI type attribute",
                           udev_device_get_sysname(dev_scsi));
                     goto fallback;
               }
               set_scsi_type(type_str, scsi_type, sizeof(type_str)-1);

               scsi_rev = udev_device_get_sysattr_value(dev_scsi, "rev");
               if (!scsi_rev) {
                     log_debug("%s: cannot get SCSI revision attribute",
                           udev_device_get_sysname(dev_scsi));
                     goto fallback;
               }
               util_replace_whitespace(scsi_rev, revision_str, sizeof(revision_str)-1);
               util_replace_chars(revision_str, NULL);

               /*
               * some broken devices have the same identifiers
               * for all luns, export the target:lun number
               */
               sprintf(instance_str, "%d:%d", target, lun);
            }
         }
         else {
            
            // set type from interface instead 
            set_usb_iftype( type_str, if_class_num, sizeof(type_str) - 1 );
         }
      }
      
      // find the device itself 
      rc = vdev_sysfs_get_parent_with_subsystem_devtype( sysfs_base, "usb", "usb_device", &parent_device_devpath, &parent_device_devpath_len );
      if( rc != 0 ) {
         
         // couldn't find
         free( ifnum_str );
         free( driver_str );
         
         if( if_subclass_str != NULL ) {
            free( if_subclass_str );
         }
         
         fprintf( stderr, "FATAL: vdev_sysfs_get_parent_with_subsystem_devtype('%s', 'usb', 'usb_device') rc = %d\n", sysfs_base, rc );
         
         exit(1);
      }
      
      // got the device path!
      memset( sysfs_base, 0, 8193 );
      strcpy( sysfs_base, parent_device_devpath );
      
      free( ifnum_str );
      free( driver_str );
      free( parent_device_devpath );
      
      if( if_subclass_str != NULL ) {
         free( if_subclass_str );
      }
      
      // get the device info
      dev_if_packed_info( sysfs_base, packed_if_str, sizeof(packed_if_str) );
   }

   /* shortcut, if we are called directly for a "usb_device" type */
   if (udev_device_get_devtype(dev) != NULL && streq(udev_device_get_devtype(dev), "usb_device")) {
            dev_if_packed_info(dev, packed_if_str, sizeof(packed_if_str));
            dev_usb = dev;
            goto fallback;
   }

   /* usb interface directory */
   dev_interface = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_interface");
   if (dev_interface == NULL) {
            log_debug("unable to access usb_interface device of '%s'",
               udev_device_get_syspath(dev));
            return EXIT_FAILURE;
   }

   ifnum = udev_device_get_sysattr_value(dev_interface, "bInterfaceNumber");
   driver = udev_device_get_sysattr_value(dev_interface, "driver");

   if_class = udev_device_get_sysattr_value(dev_interface, "bInterfaceClass");
   if (!if_class) {
            log_debug("%s: cannot get bInterfaceClass attribute",
               udev_device_get_sysname(dev));
            return EXIT_FAILURE;
   }

   if_class_num = strtoul(if_class, NULL, 16);
   if (if_class_num == 8) {
            /* mass storage */
            if_subclass = udev_device_get_sysattr_value(dev_interface, "bInterfaceSubClass");
            if (if_subclass != NULL)
                  protocol = set_usb_mass_storage_ifsubtype(type_str, if_subclass, sizeof(type_str)-1);
   } else {
            set_usb_iftype(type_str, if_class_num, sizeof(type_str)-1);
   }

   log_debug("%s: if_class %d protocol %d",
         udev_device_get_syspath(dev_interface), if_class_num, protocol);

   /* usb device directory */
   dev_usb = udev_device_get_parent_with_subsystem_devtype(dev_interface, "usb", "usb_device");
   if (!dev_usb) {
            log_debug("unable to find parent 'usb' device of '%s'",
               udev_device_get_syspath(dev));
            return EXIT_FAILURE;
   }

   /* all interfaces of the device in a single string */
   dev_if_packed_info(dev_usb, packed_if_str, sizeof(packed_if_str));

   /* mass storage : SCSI or ATAPI */
   if ((protocol == 6 || protocol == 2)) {
            struct udev_device *dev_scsi;
            const char *scsi_model, *scsi_vendor, *scsi_type, *scsi_rev;
            int host, bus, target, lun;

            /* get scsi device */
            dev_scsi = udev_device_get_parent_with_subsystem_devtype(dev, "scsi", "scsi_device");
            if (dev_scsi == NULL) {
                  log_debug("unable to find parent 'scsi' device of '%s'",
                        udev_device_get_syspath(dev));
                  goto fallback;
            }
            if (sscanf(udev_device_get_sysname(dev_scsi), "%d:%d:%d:%d", &host, &bus, &target, &lun) != 4) {
                  log_debug("invalid scsi device '%s'", udev_device_get_sysname(dev_scsi));
                  goto fallback;
            }

            /* Generic SPC-2 device */
            scsi_vendor = udev_device_get_sysattr_value(dev_scsi, "vendor");
            if (!scsi_vendor) {
                  log_debug("%s: cannot get SCSI vendor attribute",
                        udev_device_get_sysname(dev_scsi));
                  goto fallback;
            }
            udev_util_encode_string(scsi_vendor, vendor_str_enc, sizeof(vendor_str_enc));
            util_replace_whitespace(scsi_vendor, vendor_str, sizeof(vendor_str)-1);
            util_replace_chars(vendor_str, NULL);

            scsi_model = udev_device_get_sysattr_value(dev_scsi, "model");
            if (!scsi_model) {
                  log_debug("%s: cannot get SCSI model attribute",
                        udev_device_get_sysname(dev_scsi));
                  goto fallback;
            }
            udev_util_encode_string(scsi_model, model_str_enc, sizeof(model_str_enc));
            util_replace_whitespace(scsi_model, model_str, sizeof(model_str)-1);
            util_replace_chars(model_str, NULL);

            scsi_type = udev_device_get_sysattr_value(dev_scsi, "type");
            if (!scsi_type) {
                  log_debug("%s: cannot get SCSI type attribute",
                        udev_device_get_sysname(dev_scsi));
                  goto fallback;
            }
            set_scsi_type(type_str, scsi_type, sizeof(type_str)-1);

            scsi_rev = udev_device_get_sysattr_value(dev_scsi, "rev");
            if (!scsi_rev) {
                  log_debug("%s: cannot get SCSI revision attribute",
                        udev_device_get_sysname(dev_scsi));
                  goto fallback;
            }
            util_replace_whitespace(scsi_rev, revision_str, sizeof(revision_str)-1);
            util_replace_chars(revision_str, NULL);

            /*
            * some broken devices have the same identifiers
            * for all luns, export the target:lun number
            */
            sprintf(instance_str, "%d:%d", target, lun);
   }

fallback:
   vendor_id = udev_device_get_sysattr_value(dev_usb, "idVendor");
   product_id = udev_device_get_sysattr_value(dev_usb, "idProduct");

   /* fallback to USB vendor & device */
   if (vendor_str[0] == '\0') {
            const char *usb_vendor = NULL;

            usb_vendor = udev_device_get_sysattr_value(dev_usb, "manufacturer");
            if (!usb_vendor)
                  usb_vendor = vendor_id;
            if (!usb_vendor) {
                  log_debug("No USB vendor information available");
                  return EXIT_FAILURE;
            }
            udev_util_encode_string(usb_vendor, vendor_str_enc, sizeof(vendor_str_enc));
            util_replace_whitespace(usb_vendor, vendor_str, sizeof(vendor_str)-1);
            util_replace_chars(vendor_str, NULL);
   }

   if (model_str[0] == '\0') {
            const char *usb_model = NULL;

            usb_model = udev_device_get_sysattr_value(dev_usb, "product");
            if (!usb_model)
                  usb_model = product_id;
            if (!usb_model)
                  return EXIT_FAILURE;
            udev_util_encode_string(usb_model, model_str_enc, sizeof(model_str_enc));
            util_replace_whitespace(usb_model, model_str, sizeof(model_str)-1);
            util_replace_chars(model_str, NULL);
   }

   if (revision_str[0] == '\0') {
            const char *usb_rev;

            usb_rev = udev_device_get_sysattr_value(dev_usb, "bcdDevice");
            if (usb_rev) {
                  util_replace_whitespace(usb_rev, revision_str, sizeof(revision_str)-1);
                  util_replace_chars(revision_str, NULL);
            }
   }

   if (serial_str[0] == '\0') {
            const char *usb_serial;

            usb_serial = udev_device_get_sysattr_value(dev_usb, "serial");
            if (usb_serial) {
                  const unsigned char *p;

                  /* http://msdn.microsoft.com/en-us/library/windows/hardware/gg487321.aspx */
                  for (p = (unsigned char *)usb_serial; *p != '\0'; p++)
                           if (*p < 0x20 || *p > 0x7f || *p == ',') {
                                    usb_serial = NULL;
                                    break;
                           }
            }

            if (usb_serial) {
                  util_replace_whitespace(usb_serial, serial_str, sizeof(serial_str)-1);
                  util_replace_chars(serial_str, NULL);
            }
   }

   s = serial;
   l = strpcpyl(&s, sizeof(serial), vendor_str, "_", model_str, NULL);
   if (serial_str[0] != '\0')
            l = strpcpyl(&s, l, "_", serial_str, NULL);

   if (instance_str[0] != '\0')
            strpcpyl(&s, l, "-", instance_str, NULL);

   udev_builtin_add_property(dev, test, "ID_VENDOR", vendor_str);
   udev_builtin_add_property(dev, test, "ID_VENDOR_ENC", vendor_str_enc);
   udev_builtin_add_property(dev, test, "ID_VENDOR_ID", vendor_id);
   udev_builtin_add_property(dev, test, "ID_MODEL", model_str);
   udev_builtin_add_property(dev, test, "ID_MODEL_ENC", model_str_enc);
   udev_builtin_add_property(dev, test, "ID_MODEL_ID", product_id);
   udev_builtin_add_property(dev, test, "ID_REVISION", revision_str);
   udev_builtin_add_property(dev, test, "ID_SERIAL", serial);
   if (serial_str[0] != '\0')
            udev_builtin_add_property(dev, test, "ID_SERIAL_SHORT", serial_str);
   if (type_str[0] != '\0')
            udev_builtin_add_property(dev, test, "ID_TYPE", type_str);
   if (instance_str[0] != '\0')
            udev_builtin_add_property(dev, test, "ID_INSTANCE", instance_str);
   udev_builtin_add_property(dev, test, "ID_BUS", "usb");
   if (packed_if_str[0] != '\0')
            udev_builtin_add_property(dev, test, "ID_USB_INTERFACES", packed_if_str);
   if (ifnum != NULL)
            udev_builtin_add_property(dev, test, "ID_USB_INTERFACE_NUM", ifnum);
   if (driver != NULL)
            udev_builtin_add_property(dev, test, "ID_USB_DRIVER", driver);
   return EXIT_SUCCESS;
}

const struct udev_builtin udev_builtin_usb_id = {
        .name = "usb_id",
        .cmd = builtin_usb_id,
        .help = "USB device properties",
        .run_once = true,
};