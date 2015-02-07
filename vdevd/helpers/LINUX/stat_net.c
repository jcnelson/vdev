/*
 * stat_net - reads network device properties, given an interface
 * 
 * Forked from systemd/src/udev/net_id/net_id.c on January 24, 2015
 * (original from http://cgit.freedesktop.org/systemd/systemd/tree/src/udev/net_id/net_id.c)
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/***
  Copyright 2012 Kay Sievers <kay@vrfy.org>

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

/*
 * Predictable network interface device names based on:
 *  - firmware/bios-provided index numbers for on-board devices
 *  - firmware-provided pci-express hotplug slot index number
 *  - physical/geographical location of the hardware
 *  - the interface's MAC address
 *
 * http://www.freedesktop.org/wiki/Software/systemd/PredictableNetworkInterfaceNames
 *
 * Two character prefixes based on the type of interface:
 *   en -- ethernet
 *   sl -- serial line IP (slip)
 *   wl -- wlan
 *   ww -- wwan
 *
 * Type of names:
 *   b<number>                             -- BCMA bus core number
 *   ccw<name>                             -- CCW bus group name
 *   o<index>                              -- on-board device index number
 *   s<slot>[f<function>][d<dev_port>]     -- hotplug slot index number
 *   x<MAC>                                -- MAC address
 *   [P<domain>]p<bus>s<slot>[f<function>][d<dev_port>]
 *                                         -- PCI geographical location
 *   [P<domain>]p<bus>s<slot>[f<function>][u<port>][..][c<config>][i<interface>]
 *                                         -- USB port number chain
 *
 * All multi-function PCI devices will carry the [f<function>] number in the
 * device name, including the function 0 device.
 *
 * When using PCI geography, The PCI domain is only prepended when it is not 0.
 *
 * For USB devices the full chain of port numbers of hubs is composed. If the
 * name gets longer than the maximum number of 15 characters, the name is not
 * exported.
 * The usual USB configuration == 1 and interface == 0 values are suppressed.
 *
 * PCI ethernet card with firmware index "1":
 *   VDEV_NET_NAME_ONBOARD=eno1
 *   VDEV_NET_NAME_ONBOARD_LABEL=Ethernet Port 1
 *
 * PCI ethernet card in hotplug slot with firmware index number:
 *   /sys/devices/pci0000:00/0000:00:1c.3/0000:05:00.0/net/ens1
 *   VDEV_NET_NAME_MAC=enx000000000466
 *   VDEV_NET_NAME_PATH=enp5s0
 *   VDEV_NET_NAME_SLOT=ens1
 *
 * PCI ethernet multi-function card with 2 ports:
 *   /sys/devices/pci0000:00/0000:00:1c.0/0000:02:00.0/net/enp2s0f0
 *   VDEV_NET_NAME_MAC=enx78e7d1ea46da
 *   VDEV_NET_NAME_PATH=enp2s0f0
 *   /sys/devices/pci0000:00/0000:00:1c.0/0000:02:00.1/net/enp2s0f1
 *   VDEV_NET_NAME_MAC=enx78e7d1ea46dc
 *   VDEV_NET_NAME_PATH=enp2s0f1
 *
 * PCI wlan card:
 *   /sys/devices/pci0000:00/0000:00:1c.1/0000:03:00.0/net/wlp3s0
 *   VDEV_NET_NAME_MAC=wlx0024d7e31130
 *   VDEV_NET_NAME_PATH=wlp3s0
 *
 * USB built-in 3G modem:
 *   /sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.4/2-1.4:1.6/net/wwp0s29u1u4i6
 *   VDEV_NET_NAME_MAC=wwx028037ec0200
 *   VDEV_NET_NAME_PATH=wwp0s29u1u4i6
 *
 * USB Android phone:
 *   /sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2:1.0/net/enp0s29u1u2
 *   VDEV_NET_NAME_MAC=enxd626b3450fb5
 *   VDEV_NET_NAME_PATH=enp0s29u1u2
 */

#include "common.h"

#include <net/if.h>
#include <net/if_arp.h>
#include <linux/pci_regs.h>

enum netname_type{
   NET_UNDEF,
   NET_PCI,
   NET_USB,
   NET_BCMA,
   NET_VIRTIO,
   NET_CCWGROUP,
};

struct netnames {
   enum netname_type type;

   uint8_t mac[6];
   bool mac_valid;
   
   char* pcidev;                // sysfs device path
   char pci_slot[IFNAMSIZ+1];
   char pci_path[IFNAMSIZ+1];
   char pci_onboard[IFNAMSIZ+1];
   char *pci_onboard_label;

   char usb_ports[IFNAMSIZ+1];
   char bcma_core[IFNAMSIZ+1];
   char ccw_group[IFNAMSIZ+1];
};

/* retrieve on-board index number and label from firmware */
static int dev_pci_onboard( struct netnames *names) {
   
   char *index;
   size_t index_len;
   
   int idx;
   int rc = 0;

   /* ACPI _DSM  -- device specific method for naming a PCI or PCI Express device */
   rc = vdev_sysfs_read_attr(names->pcidev, "acpi_index", &index, &index_len );
   
   /* SMBIOS type 41 -- Onboard Devices Extended Information */
   if ( rc < 0 ) {
      rc = vdev_sysfs_read_attr(names->pcidev, "index", &index, &index_len );
   }
   
   if( rc < 0 ) { 
      return -ENOENT;
   }
   
   idx = strtoul(index, NULL, 0);
   if (idx <= 0) {
      
      free( index );
      return -EINVAL;
   }
   
   snprintf(names->pci_onboard, sizeof(names->pci_onboard), "o%d", idx);

   vdev_sysfs_read_attr( names->pcidev, "label", &names->pci_onboard_label, &index_len );
   free( index );
   
   return 0;
}

/* read the 256 bytes PCI configuration space to check the multi-function bit */
static bool is_pci_multifunction( char const* sysfs_path ) {
   
   FILE *f = NULL;
   char *filename;
   uint8_t config[64];
   
   filename = (char*)calloc( strlen(sysfs_path) + 1 + strlen("/config") + 1, 1 );
   if( filename == NULL ) {
      return -ENOMEM;
   }
   
   sprintf( filename, "%s/config", sysfs_path );
   
   f = fopen(filename, "re");
   if (!f) {
      
      free( filename );
      return false;
   }
   
   if (fread(&config, sizeof(config), 1, f) != 1) {
      
      free( filename );
      fclose( f );
      return false;
   }

   fclose( f );
   
   /* bit 0-6 header type, bit 7 multi/single function device */
   if ((config[PCI_HEADER_TYPE] & 0x80) != 0) {
      free( filename );
      return true;
   }
   
   free( filename );

   return false;
}


// calculate the pci path and slot of the device
static int dev_pci_slot( char const* dev_path, struct netnames *names) {
   
   unsigned domain, bus, slot, func, dev_port = 0;
   
   char *attr;
   size_t attr_len;
   
   char* pci_sysfs_path = NULL;
   size_t pci_sysfs_path_len = 0;
   
   char* slots = NULL;
   char* address_path = NULL;
   
   DIR *dir = NULL;
   struct dirent *dent;
   int hotplug_slot = 0, err = 0;
   char* pcidev_sysname = NULL;
   size_t pcidev_sysname_len = 0;
   int rc = 0;
   
   rc = vdev_sysfs_get_sysname( names->pcidev, &pcidev_sysname, &pcidev_sysname_len );
   if( rc != 0 ) {
      return rc;
   }

   if (sscanf( pcidev_sysname, "%x:%x:%x.%u", &domain, &bus, &slot, &func) != 4) {
      
      free( pcidev_sysname );
      return -ENOENT;
   }
   
   free( pcidev_sysname );

   /* kernel provided multi-device index */
   rc = vdev_sysfs_read_attr(dev_path, "dev_port", &attr, &attr_len );
   if ( rc == 0 ) {
      dev_port = strtol(attr, NULL, 10);
      free( attr );
   }

   /* compose a name based on the raw kernel's PCI bus, slot numbers */
   memset( names->pci_path, 0, IFNAMSIZ+1 );
   
   if (domain > 0) {
      
      sprintf( names->pci_path + strlen(names->pci_path), "P%u", domain );
   }
   
   sprintf( names->pci_path + strlen(names->pci_path), "p%us%u", bus, slot );
   
   if (func > 0 || is_pci_multifunction(names->pcidev)) {
      
      sprintf( names->pci_path + strlen(names->pci_path), "f%u", func );
   }
   
   if (dev_port > 0) {
      
      sprintf( names->pci_path + strlen(names->pci_path), "d%u", dev_port );
   }
   
   if (strlen(names->pci_path) == 0) {
      names->pci_path[0] = '\0';
   }

   /* ACPI _SUN  -- slot user number */
   rc = vdev_sysfs_device_path_from_subsystem_sysname("/sys", "subsystem", "pci", &pci_sysfs_path, &pci_sysfs_path_len);
   if ( rc != 0 ) {
      err = -ENOENT;
      goto out;
   }
   
   slots = (char*)calloc( strlen(pci_sysfs_path) + strlen("/slots") + 1, 1 );
   if( slots == NULL ) {
      err = -ENOMEM;
      
      free( pci_sysfs_path );
      goto out;
   }
   
   sprintf( slots, "%s/slots", pci_sysfs_path );
   
   free( pci_sysfs_path );
   
   dir = opendir(slots);
   if (!dir) {
      err = -errno;
      goto out;
   }

   for (dent = readdir(dir); dent != NULL; dent = readdir(dir)) {
      int i;
      char *rest;
      char *address;
      size_t address_len = 0;

      if (dent->d_name[0] == '.') {
         continue;
      }
      
      i = strtol(dent->d_name, &rest, 10);
      
      if (rest[0] != '\0') {
         continue;
      }
      
      if (i < 1) {
         continue;
      }
      
      address_path = (char*)calloc( strlen(slots) + strlen(dent->d_name) + 3 + strlen("address"), 1 );
      if( address_path == NULL ) {
         err = -ENOMEM;
         goto out;
      }
      
      sprintf( address_path, "%s/%s/address", slots, dent->d_name );
      
      rc = vdev_read_file( address_path, &address, &address_len );
      if( rc != 0 ) {
         
         free( address_path );
         continue;
      }
      
      free( address_path );
      
      // take the first line only 
      if( index( address, '\n' ) != NULL ) {
         
         *(index(address, '\n')) = '\0';
      }
      
      /* match slot address with device by stripping the function */
      if (strneq(address, names->pcidev, strlen(address))) {
         hotplug_slot = i;
      }
      
      free(address);
      
      if (hotplug_slot > 0) {
         break;
      }
   }
   
   closedir( dir );

   if (hotplug_slot > 0) {
            
      memset( names->pci_slot, 0, IFNAMSIZ+1 );
      
      if (domain > 0) {
         
         sprintf( names->pci_slot + strlen( names->pci_slot ), "P%d", domain );
      }
      
      sprintf( names->pci_slot + strlen( names->pci_slot ), "s%d", hotplug_slot );
      
      if (func > 0 || is_pci_multifunction(names->pcidev)) {
         
         sprintf( names->pci_slot + strlen( names->pci_slot ), "f%d", func );
      }
      
      if (dev_port > 0) {
         
         sprintf( names->pci_slot + strlen( names->pci_slot ), "d%d", dev_port );
      }
      
      if (strlen(names->pci_slot) == 0) {
         names->pci_slot[0] = '\0';
      }
   }
out:
   return err;
}

static int names_pci( char const* dev_path, struct netnames *names ) {
   
   char* parent_device_path = NULL;
   size_t parent_device_path_len = 0;
   
   char* parent_device_subsystem = NULL;
   size_t parent_device_subsystem_len = 0;
   
   int rc = 0;
   
   // get parent device 
   rc = vdev_sysfs_get_parent_device( dev_path, &parent_device_path, &parent_device_path_len );
   if( rc != 0 ) {
      
      if( rc != -ENOENT ) {
         // some other fatal error 
         log_error("WARN: vdev_sysfs_get_parent_device('%s') rc = %d\n", dev_path, rc );
      }
      
      return rc;
   }
   
   // get parent subsystem 
   rc = vdev_sysfs_read_subsystem( parent_device_path, &parent_device_subsystem, &parent_device_subsystem_len );
   if( rc != 0 ) {
      
      free( parent_device_path );
      
      log_error("WARN: vdev_sysfs_read_subsystem('%s') rc = %d\n", parent_device_path, rc );
      return rc;
   }
   
   /* check if our direct parent is a PCI device with no other bus in-between */
   if ( strcmp("pci", parent_device_subsystem) == 0 ) {
      
      names->type = NET_PCI;
      names->pcidev = strdup( parent_device_path );
      
      if( names->pcidev == NULL ) {
         free( parent_device_path );
         free( parent_device_subsystem );
         return -ENOMEM;
      }
   }
   else {
      
      size_t pcidev_len = 0;
      rc = vdev_sysfs_get_parent_with_subsystem_devtype( dev_path, "pci", NULL, &names->pcidev, &pcidev_len );
      
      if( rc != 0 ) {
         
         if( rc != -ENOENT ) {
            log_error("WARN: vdev_sysfs_get_parent_with_subsystem_devtype('%s', 'pci', NULL) rc = %d\n", dev_path, rc );
         }
         
         return rc;
      }
   }
   
   free( parent_device_subsystem );
   free( parent_device_path );
   
   dev_pci_onboard( names );
   dev_pci_slot(dev_path, names);
   return 0;
}

static int names_usb( char const* dev_path, struct netnames *names) {
        
   char *ports;
   char *config;
   char *interf;
   char *s;
   char* usbdev_path = NULL;
   char* usbdev_sysname = NULL;
   size_t usbdev_path_len = 0;
   size_t usbdev_sysname_len = NULL;
   int rc = 0;
   
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( dev_path, "usb", "usb_interface", &usbdev_path, &usbdev_path_len );
   if( rc != 0 ) {
      
      if( rc != -ENOENT ) {
         log_error("WARN: vdev_sysfs_get_parent_with_subsystem_devtype('%s', 'usb', 'usb_interface') rc = %d\n", dev_path, rc );
      }
      
      return rc;
   }
   
   rc = vdev_sysfs_get_sysname( usbdev_path, &usbdev_sysname, &usbdev_sysname_len );
   if( rc != 0 ) {
      
      free( usbdev_path );
      return rc;
   }
   
   free( usbdev_path );
   
   /* get USB port number chain, configuration, interface */
   s = strchr(usbdev_sysname, '-');
   if (!s) {
      
      free( usbdev_sysname );
      return -EINVAL;
   }
   
   ports = s+1;

   s = strchr(ports, ':');
   if (!s) {
      
      free( usbdev_sysname );
      return -EINVAL;
   }
   
   s[0] = '\0';
   config = s+1;

   s = strchr(config, '.');
   if (!s) {
      
      free( usbdev_sysname );
      return -EINVAL;
   }
   
   s[0] = '\0';
   interf = s+1;

   /* prefix every port number in the chain with "u" */
   s = ports;
   while ((s = strchr(s, '.'))) {
      s[0] = 'u';
   }
   
   // set up usb_ports
   s = names->usb_ports;
   memset( s, 0, IFNAMSIZ+1 );
   
   strncat( s, "u", IFNAMSIZ );
   strncat( s, ports, IFNAMSIZ );
   
   /* append USB config number, suppress the common config == 1 */
   if( strcmp(config, "1") != 0 ) {
      strncat( s, "c", IFNAMSIZ );
      strncat( s, ports, IFNAMSIZ );
   }
   
   /* append USB interface number, suppress the interface == 0 */
   if( strcmp(interf, "0") != 0 ) {
      strncat( s, "i", IFNAMSIZ );
      strncat( s, interf, IFNAMSIZ );
   }
   
   if( strlen(s) == 0 ) {
      
      free( usbdev_sysname );
      return -ENAMETOOLONG;
   }
   
   free( usbdev_sysname );
   names->type = NET_USB;
   return 0;
}

static int names_bcma( char const* devpath, struct netnames *names) {

   char* bcmadev_path = NULL;
   size_t bcmadev_path_len = 0;
   char* bcmadev_sysname = NULL;
   size_t bcmadev_sysname_len = 0;
   unsigned int core;
   int rc = 0;

   rc = vdev_sysfs_get_parent_with_subsystem_devtype( devpath, "bcma", NULL, &bcmadev_path, &bcmadev_path_len );
   if( rc != 0 ) {
      
      if( rc != -ENOENT ) {
         log_error("vdev_sysfs_get_parent_with_subsystem_devtype('%s', 'bcma', NULL) rc = %d\n", devpath, rc );
      }
      
      return rc;
   }
   
   rc = vdev_sysfs_get_sysname( devpath, &bcmadev_sysname, &bcmadev_sysname_len );
   if( rc != 0 ) {
      
      free( bcmadev_path );
      return rc;
   }
   
   /* bus num:core num */
   if (sscanf( bcmadev_sysname, "bcma%*u:%u", &core) != 1) {
      
      free( bcmadev_path );
      free( bcmadev_sysname );
      return -EINVAL;
   }
   
   /* suppress the common core == 0 */
   if (core > 0) {
      snprintf(names->bcma_core, sizeof(names->bcma_core), "b%u", core);
   }

   names->type = NET_BCMA;

   free( bcmadev_path );
   free( bcmadev_sysname );
   return 0;
}


static int names_ccw( char const* devpath, struct netnames *names) {

   char *bus_id = NULL;
   size_t bus_id_len = 0;
   int rc;
   
   char* parent_devpath = NULL;
   size_t parent_devpath_len = 0;
   
   char* parent_subsystem = NULL;
   size_t parent_subsystem_len = 0;
   
   /* Retrieve the associated CCW device */
   rc = vdev_sysfs_get_parent_device( devpath, &parent_devpath, &parent_devpath_len );
   if( rc != 0 ) {
      return -ENOENT;
   }
   
   // get parent subsystem 
   rc = vdev_sysfs_read_subsystem( parent_devpath, &parent_subsystem, &parent_subsystem_len );
   if( rc != 0 ) {
      
      free( parent_devpath );
      return rc;
   }
   
   /* Network devices are always grouped CCW devices */
   if( strcmp("ccwgroup", parent_subsystem) != 0 ) {
      
      free( parent_devpath );
      free( parent_subsystem );
      return -ENOENT;
   }

   /* Retrieve bus-ID of the grouped CCW device.  The bus-ID uniquely
   * identifies the network device on the Linux on System z channel
   * subsystem.  Note that the bus-ID contains lowercase characters.
   */
   rc = vdev_sysfs_get_sysname( parent_devpath, &bus_id, &bus_id_len );
   if( rc != 0 ) {
      
      free( parent_devpath );
      free( parent_subsystem );
      return -ENOENT;
   }
   
   /* Check the length of the bus-ID.  Rely on that the kernel provides
   * a correct bus-ID; alternatively, improve this check and parse and
   * verify each bus-ID part...
   */
   if (bus_id_len == 0 || bus_id_len < 8 || bus_id_len > 9) {
      
      free( parent_devpath );
      free( parent_subsystem );
      free( bus_id );
      
      return -EINVAL;
   }

   /* Store the CCW bus-ID for use as network device name */
   rc = snprintf(names->ccw_group, sizeof(names->ccw_group), "ccw%s", bus_id);
   if (rc >= 0 && rc < (int)sizeof(names->ccw_group)) {
      names->type = NET_CCWGROUP;
   }
   
   free( parent_devpath );
   free( parent_subsystem );
   free( bus_id );
   
   return 0;
}

static int names_mac( char const* devpath, struct netnames *names) {

   unsigned int i;
   unsigned int a1, a2, a3, a4, a5, a6;
   int rc = 0;
   
   char* addr_assign_type = NULL;
   size_t addr_assign_type_len = 0;
   
   char* address = NULL;
   size_t address_len = 0;
   
   rc = vdev_sysfs_read_attr( devpath, "addr_assign_type", &addr_assign_type, &addr_assign_type_len );
   if( rc != 0 ) {
      return rc;
   }
   
   i = strtoul( addr_assign_type, NULL, 0);
   if (i != 0) {
      
      free( addr_assign_type );
      return 0;
   }
   
   rc = vdev_sysfs_read_attr( devpath, "address", &address, &address_len );
   if( rc != 0 ) {
      
      free( addr_assign_type );
      return rc;
   }
   
   rc = sscanf(address, "%x:%x:%x:%x:%x:%x", &a1, &a2, &a3, &a4, &a5, &a6);
   if( rc != 6 ) {
      
      free( addr_assign_type );
      free( address );
      return -EINVAL;
   }

   free( addr_assign_type );
   free( address );
   
   /* skip empty MAC addresses */
   if( a1 == 0 && a2 == 0 && a3 == 0 && a4 == 0 && a5 == 0 && a6 == 0 ) {
      return -EINVAL;
   }
   
   names->mac[0] = a1;
   names->mac[1] = a2;
   names->mac[2] = a3;
   names->mac[3] = a4;
   names->mac[4] = a5;
   names->mac[5] = a6;
   names->mac_valid = true;
   return 0;
}

/* IEEE Organizationally Unique Identifier vendor string */
static int ieee_oui( char const* devpath, struct netnames *names ) {
   char str[32];

   if (!names->mac_valid) {
      return -ENOENT;
   }
   
   /* skip commonly misused 00:00:00 (Xerox) prefix */
   if (memcmp(names->mac, "\0\0\0", 3) == 0) {
      return -EINVAL;
   }
   
   snprintf(str, sizeof(str), "OUI:%02X%02X%02X%02X%02X%02X",
            names->mac[0], names->mac[1], names->mac[2],
            names->mac[3], names->mac[4], names->mac[5]);
   
   // TODO: find out how to replace this
   // udev_builtin_hwdb_lookup(dev, NULL, str, NULL, test);
   
   return 0;
}


void usage( char const* progname ) {
   fprintf(stderr, "Usage: %s INTERFACE\n", progname );
}

int main( int argc, char **argv ) {
   
   unsigned int i;
   const char *prefix = "en";
   struct netnames names;
   int err = 0;
   int rc = 0;
   char* tmp = NULL;    // for realpath success check
   bool is_net_device = false;
   
   char devpath_class[4097];
   char devpath[4097];
   char devpath_uevent[4097];
   
   char* devtype = NULL;
   size_t devtype_len = 0;
   
   // sysfs attr buf
   char* attr = NULL;
   char* attr_iflink = NULL;
   char* attr_ifindex = NULL;
   size_t attr_len = 0;
   
   char* uevent_buf = NULL;
   size_t uevent_buf_len = 0;
   
   memset( &names, 0, sizeof(struct netnames) );
   
   if( argc != 2 ) {
      usage( argv[0] );
   }
   
   // get the device from the interface 
   sprintf( devpath_class, "/sys/class/net/%s", argv[1] );
   
   tmp = realpath( devpath_class, devpath );
   if( tmp == NULL ) {
      rc = -errno;
      fprintf(stderr, "Failed to locate sysfs entry for %s\n", argv[1] );
      exit(1);
   }
   
   // only care about ethernet and SLIP devices 
   rc = vdev_sysfs_read_attr( devpath, "type", &attr, &attr_len );
   if( rc != 0 ) {
      
      // not found 
      fprintf(stderr, "Failed to find interface type for %s (sysfs path '%s'), rc = %d\n", argv[1], devpath, rc );
      exit(1);
   }
   
   i = strtoul(attr, NULL, 0);
   
   free( attr );
   
   switch (i) {
   case ARPHRD_ETHER:
      prefix = "en";
      break;
   case ARPHRD_SLIP:
      prefix = "sl";
      break;
   default:
      return 0;
   }
   
   // sanity check (i.e. no stacked devices)
   rc = vdev_sysfs_read_attr( devpath, "ifindex", &attr_ifindex, &attr_len );
   if( rc != 0 ) {
      
      log_error("vdev_sysfs_read_attr('%s', 'ifindex') rc = %d\n", devpath, rc );
      exit(1);
   }
   
   rc = vdev_sysfs_read_attr( devpath, "iflink", &attr_iflink, &attr_len );
   if( rc != 0 ) {
      
      log_error("vdev_sysfs_read_attr('%s', 'iflink') rc = %d\n", devpath, rc );
      
      free( attr_ifindex );
      exit(1);
   }
   
   if( strcmp( attr_ifindex, attr_iflink ) != 0 ) {
      exit(0);
   }
   
   free( attr_ifindex );
   free( attr_iflink );
   
   // get device type 
   sprintf( devpath_uevent, "%s/uevent", devpath );

   rc = vdev_read_file( devpath_uevent, &uevent_buf, &uevent_buf_len );
   if( rc != 0 ) {
      
      log_error("vdev_read_file('%s') rc = %d\n", devpath_uevent, rc );
      exit(2);
   }
   
   rc = vdev_sysfs_uevent_get_key( uevent_buf, uevent_buf_len, "DEVTYPE", &devtype, &devtype_len );
   if( rc == 0 ) {
      
      if( strcmp("wlan", devtype) == 0 ) {
         prefix = "wl";
      }
      else if( strcmp("wwan", devtype) == 0 ) {
         prefix = "ww";
      }
   }
   
   free( uevent_buf );
   free( devtype );
   
   err = names_mac( devpath, &names);
   if (err >= 0 && names.mac_valid) {
      
      char str[IFNAMSIZ];

      snprintf(str, sizeof(str), "%sx%02x%02x%02x%02x%02x%02x", prefix,
               names.mac[0], names.mac[1], names.mac[2],
               names.mac[3], names.mac[4], names.mac[5]);
      
      vdev_property_add( "VDEV_NET_NAME_MAC", str );
      
      is_net_device = true;
      ieee_oui(devpath, &names);
   }

   /* get path names for Linux on System z network devices */
   err = names_ccw(devpath, &names);
   if (err >= 0 && names.type == NET_CCWGROUP) {
      
      char str[IFNAMSIZ];

      if (snprintf(str, sizeof(str), "%s%s", prefix, names.ccw_group) < (int)sizeof(str)) {
         
         vdev_property_add( "VDEV_NET_NAME_PATH", str );
         is_net_device = true;
      }
      
      goto out;
   }

   /* get PCI based path names, we compose only PCI based paths */
   err = names_pci(devpath, &names);
   if (err < 0) {
      goto out;
   }

   /* plain PCI device */
   if (names.type == NET_PCI) {
      
      char str[IFNAMSIZ];

      if (names.pci_onboard[0]) {
         if (snprintf(str, sizeof(str), "%s%s", prefix, names.pci_onboard) < (int)sizeof(str)) {
            
            vdev_property_add( "VDEV_NET_NAME_ONBOARD", str );
            is_net_device = true;
         }
      }

      if (names.pci_onboard_label) {
         if (snprintf(str, sizeof(str), "%s%s", prefix, names.pci_onboard_label) < (int)sizeof(str)) {
            
            vdev_property_add( "VDEV_NET_LABEL_ONBOARD", str );
            is_net_device = true;
         }
      }

      if (names.pci_path[0]) {
         if (snprintf(str, sizeof(str), "%s%s", prefix, names.pci_path) < (int)sizeof(str)) {
            
            vdev_property_add( "VDEV_NET_NAME_PATH", str );
            is_net_device = true;
         }
      }

      if (names.pci_slot[0]) {
         if (snprintf(str, sizeof(str), "%s%s", prefix, names.pci_slot) < (int)sizeof(str)) {
            
            vdev_property_add( "VDEV_NET_NAME_SLOT", str );
            is_net_device = true;
         }
      }
      goto out;
   }

   /* USB device */
   err = names_usb(devpath, &names);
   if (err >= 0 && names.type == NET_USB) {
      
      char str[IFNAMSIZ];

      if (names.pci_path[0]) {
         if (snprintf(str, sizeof(str), "%s%s%s", prefix, names.pci_path, names.usb_ports) < (int)sizeof(str)) {
            
            vdev_property_add( "VDEV_NET_NAME_PATH", str );
            is_net_device = true;
         }
      }

      if (names.pci_slot[0]) {
         if (snprintf(str, sizeof(str), "%s%s%s", prefix, names.pci_slot, names.usb_ports) < (int)sizeof(str)) {
            
            vdev_property_add( "VDEV_NET_NAME_SLOT", str );
            is_net_device = true;
         }
      }
      
      goto out;
   }

   /* Broadcom bus */
   err = names_bcma(devpath, &names);
   if (err >= 0 && names.type == NET_BCMA) {
      
      char str[IFNAMSIZ];

      if (names.pci_path[0]) {
         if (snprintf(str, sizeof(str), "%s%s%s", prefix, names.pci_path, names.bcma_core) < (int)sizeof(str)) {
            
            vdev_property_add( "VDEV_NET_NAME_PATH", str );
            is_net_device = true;
         }
      }

      if (names.pci_slot[0]) {
         if (snprintf(str, sizeof(str), "%s%s%s", prefix, names.pci_slot, names.bcma_core) < (int)sizeof(str)) {
            
            vdev_property_add( "VDEV_NET_NAME_SLOT", str );
            is_net_device = true;
         }
      }
      goto out;
   }
out:

   if( is_net_device ) {
      vdev_property_add( "VDEV_NET", "1" );
   }
   
   vdev_property_print();
   vdev_property_free_all();
   
   return EXIT_SUCCESS;
}
