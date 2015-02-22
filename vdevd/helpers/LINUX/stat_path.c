/*
 * stat_path - builds a persistent path name from sysfs
 * 
 * Forked from systemd/src/udev/udev-builtin-path_id.c on February 21, 2015.
 * (original from http://cgit.freedesktop.org/systemd/systemd/plain/src/udev/udev-builtin-path_id.c)
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/*
 * compose persistent device path
 *
 * Copyright (C) 2009-2011 Kay Sievers <kay@vrfy.org>
 *
 * Logic based on Hannes Reinecke's shell script.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <getopt.h>

static int path_prepend(char **path, const char *fmt, ...) {
   va_list va;
   char *pre;
   int err = 0;

   va_start(va, fmt);
   err = vasprintf(&pre, fmt, va);
   va_end(va);
   if (err < 0) {
      goto out;
   }
   
   if (*path != NULL) {
      char *new;

      err = asprintf(&new, "%s-%s", pre, *path);
      free(pre);
      if (err < 0) {
         goto out;
      }
      free(*path);
      *path = new;
   } else {
      *path = pre;
   }
out:
   return err;
}

/*
** Linux only supports 32 bit luns.
** See drivers/scsi/scsi_scan.c::scsilun_to_int() for more details.
*/
static int format_lun_number( char const* devpath, char **path) {
   
   int lun = 0;
   int rc = vdev_sysfs_get_sysnum( devpath, &lun );
   if( rc != 0 ) {
      return rc;
   }
   
   /* address method 0, peripheral device addressing with bus id of zero */
   if (lun < 256) {
      return path_prepend(path, "lun-%lu", lun);
   }
   
   /* handle all other lun addressing methods by using a variant of the original lun format */
   return path_prepend(path, "lun-0x%04lx%04lx00000000", lun & 0xffff, (lun >> 16) & 0xffff);
}


// walk up the sysfs path and skip all ancestor devices with the given subsystem.
// stop once the parent device has a different subsystem from subsys.
// return 0 on success, and set *next to the path to the deepest ancestor device that either does not have a subsystem, or has one that is different from subsys.
static int skip_subsystem( char const* devpath, char const* subsys, char** next ) {
  
   char* cur_dev = strdup( devpath );
   char* parent = cur_dev;
   
   size_t parent_len = 0;
   int rc = 0;
   
   while( true ) {
      
      char* subsystem = NULL;
      size_t subsystem_len = 0;
      
      int rc = 0;
      
      rc = vdev_sysfs_read_subsystem( parent, &subsystem, &subsystem_len );
      
      // break if not matched 
      if( (rc == -ENOENT || subsystem == NULL) || strcmp( subsystem, subsys ) != 0 ) {
         
         if( subsystem != NULL ) {
            free( subsystem );
         }
         break;
      }
      
      log_debug("skip %s", parent );
      
      free( subsystem );
      
      if( cur_dev != parent ) {
         free( cur_dev );
      }
      
      cur_dev = parent;
      
      // matched; walk up
      rc = vdev_sysfs_get_parent_device( cur_dev, &parent, &parent_len );
      if( rc != 0 ) {
         
         break;
      }
   }
   
   *next = cur_dev;

   if( cur_dev != parent ) {
      free( parent );
   }
   
   log_debug("Skip from '%s' to '%s'\n", devpath, *next );
   
   return rc;
}

// persistent name for scsi fibre channel (named by parent)
// return 0 on success, and set *new_parent to the parent device and update *path
// return negative on failure
static int handle_scsi_fibre_channel( char const* parent, char **path, char** new_parent ) {
        
   char* port = NULL;
   size_t port_len = 0;
   
   char* lun = NULL;
   
   char* targetdev = NULL;
   size_t targetdev_len = 0;
   
   char* targetdev_sysname = NULL;
   size_t targetdev_sysname_len = 0;
   
   char* fcdev = NULL;
   size_t fcdev_len = 0;
   
   int rc = 0;
   
   // look up SCSI parent target
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( parent, "scsi", "scsi_target", &targetdev, &targetdev_len );
   if( rc != 0 ) {
      
      return rc;
   }
   
   // get target device name 
   rc = vdev_sysfs_get_sysname( targetdev, &targetdev_sysname, &targetdev_sysname_len );
   if( rc != 0 ) {
      
      free( targetdev );
      return rc;
   }
   
   // find the corresponding fibrechannel sysfs path 
   rc = vdev_sysfs_device_path_from_subsystem_sysname( "/sys", "fc_transport", targetdev_sysname, &fcdev, &fcdev_len );
   if( rc != 0 ) {
      
      free( targetdev );
      free( targetdev_sysname );
      return rc;
   }
   
   free( targetdev_sysname );
   
   // look up the port name 
   rc = vdev_sysfs_read_attr( fcdev, "port_name", &port, &port_len );
   if( rc != 0 ) {
      
      free( targetdev );
      return rc;
   }
   
   free( fcdev );
   
   // parse port name 
   rc = format_lun_number( parent, &lun );
   
   path_prepend( path, "fc-%s-%s", port, lun );
   
   if( lun != NULL ) {
      free( lun );
   }
   
   // parent stays the same 
   *new_parent = strdup( parent );
   if( *new_parent == NULL ) {
      
      return -ENOMEM;
   }
   
   return 0;
}

// persistent name for SCSI SAS wide port 
// return 0 on success, and update *path with the new persistent name information and set *new_parent to the next parent to parse 
// return negative on error
static int handle_scsi_sas_wide_port( char const *parent, char **path, char** new_parent ) {
   
   char* targetdev = NULL;
   size_t targetdev_len = 0;
   
   char* targetparent = NULL;
   size_t targetparent_len = 0;
   
   char* targetparent_sysname = NULL;
   size_t targetparent_sysname_len = 0;
   
   char* sasdev = NULL;
   size_t sasdev_len = 0;
   
   char* sas_address = NULL;
   size_t sas_address_len = 0;
   
   char* lun = NULL;
   
   int rc = 0;
   
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( parent, "scsi", "scsi_target", &targetdev, &targetdev_len );
   if( rc != 0 ) {
      return rc;
   }
   
   rc = vdev_sysfs_get_parent_device( parent, &targetparent, &targetparent_len );
   if( rc != 0 ) {
      
      free( targetdev );
      return rc;
   }
   
   rc = vdev_sysfs_get_sysname( targetparent, &targetparent_sysname, &targetparent_sysname_len );
   if( rc != 0 ) {
      
      free( targetdev );
      free( targetparent );
      return rc;
   }
   
   // find the sas device 
   rc = vdev_sysfs_device_path_from_subsystem_sysname( "/sys", "sas_device", targetparent_sysname, &sasdev, &sasdev_len );
   
   free( targetparent );
   free( targetparent_sysname );
   
   if( rc != 0 ) {
      
      free( targetdev );
      return rc;
   }
   
   // find the address 
   rc = vdev_sysfs_read_attr( sasdev, "sas_address", &sas_address, &sas_address_len );
   if( rc != 0 ) {
      
      free( targetdev );
      return rc;
   }
   
   format_lun_number( parent, &lun );
   path_prepend( path, "sas-%s-%s", sas_address, lun );
   
   if( lun != NULL ) {
      free( lun );
   }
   
   free( targetdev );
   
   *new_parent = strdup( parent );
   if( *new_parent == NULL ) {
      
      return -ENOMEM;
   }
   
   return 0;
}


// get the persistent path for a SCSI SAS 
// return 0 on success, and update *path with the persistent path information for this device, and set *parent to the next parent to process 
// return negative on error
static int handle_scsi_sas( char const* parent, char **path, char** new_parent ) {
   
   char* targetdev = NULL;
   size_t targetdev_len = 0;
   
   char* target_parent = NULL;
   size_t target_parent_len = 0;
   
   char* target_parent_sysname = NULL;
   size_t target_parent_sysname_len = 0;
   
   char* port = NULL;
   size_t port_len = 0;
   
   char* port_sysname = NULL;
   size_t port_sysname_len = 0;
   
   char* expander = NULL;
   size_t expander_len = 0;
   
   char* expander_sysname = NULL;
   size_t expander_sysname_len = 0;
   
   char* target_sasdev = NULL;
   size_t target_sasdev_len = 0;
   
   char* expander_sasdev = NULL;
   size_t expander_sasdev_len = 0;
   
   char* port_sasdev = NULL;
   size_t port_sasdev_len = 0;
   
   char* sas_address = NULL;
   size_t sas_address_len = 0;
   
   char* phy_id = NULL;
   size_t phy_id_len = 0;
   
   char* phy_count = NULL;
   size_t phy_count_len = 0;
   
   char* lun = NULL;
   
   int rc = 0;
   
   // find scsi target parent device 
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( parent, "scsi", "scsi_target", &targetdev, &targetdev_len );
   if( rc != 0 ) {
      
      return rc;
   }
   
   // find parent of the scsi target 
   rc = vdev_sysfs_get_parent_device( targetdev, &target_parent, &target_parent_len );
   
   free( targetdev );
   
   if( rc != 0 ) {
      
      return rc;
   }
   
   // get parent sysname 
   rc = vdev_sysfs_get_sysname( target_parent, &target_parent_sysname, &target_parent_sysname_len );
   if( rc != 0 ) {
      
      free( target_parent );
      return rc;
   }
   
   // get sas device 
   rc = vdev_sysfs_device_path_from_subsystem_sysname( "/sys", "sas_device", target_parent_sysname, &target_sasdev, &target_sasdev_len );
   
   free( target_parent );
   
   if( rc != 0 ) { 
      
      return rc;
   }
   
   // get the sas port (parent of the sas device)
   rc = vdev_sysfs_get_parent_device( target_parent, &port, &port_len );
   if( rc != 0 ) {
      
      free( target_sasdev );
      return rc;
   }
   
   // get sas port sysname 
   rc = vdev_sysfs_get_sysname( port, &port_sysname, &port_sysname_len );
   if( rc != 0 ) {
      
      free( target_sasdev );
      free( port );
      return rc;
   }
   
   // get the port device 
   rc = vdev_sysfs_device_path_from_subsystem_sysname( "/sys", "sas_port", port_sysname, &port_sasdev, &port_sasdev_len );
   
   free( port_sysname );
   
   if( rc != 0 ) {
      
      free( target_sasdev );
      free( port );
      return rc;
   }
   
   // get phy count for this sas device 
   rc = vdev_sysfs_read_attr( port_sasdev, "num_phys", &phy_count, &phy_count_len );
   if( rc != 0 ) {
      
      free( target_sasdev );
      free( port_sasdev );
      free( port );
      return rc;
   }
   
   // one disk?
   if( strncmp( phy_count, "1", 2 ) != 0 ) {
      
      // wide port 
      rc = handle_scsi_sas_wide_port( parent, path, new_parent );
      
      free( target_sasdev );
      free( port_sasdev );
      
      return rc;
   }
   
   free( phy_count );
   
   // multiple disks...
   // which one is this?
   rc = vdev_sysfs_read_attr( target_sasdev, "phy_identifier", &phy_id, &phy_id_len );
   
   free( target_sasdev );
   
   if( rc != 0 ) {
      
      free( port );
      return rc;
   }
   
   // parent is either an HBA or expander device...
   rc = vdev_sysfs_get_parent_device( port, &expander, &expander_len );
   
   free( port );
   
   if( rc != 0 ) {
      
      free( phy_id );
      return rc;
   }
   
   // get the expander sysname 
   rc = vdev_sysfs_get_sysname( expander, &expander_sysname, &expander_sysname_len );
   if( rc != 0 ) {
      
      free( phy_id );
      free( expander );
      return rc;
   }
   
   free( expander );
   
   // get the expander sas device 
   rc = vdev_sysfs_device_path_from_subsystem_sysname( "/sys", "sas_device", expander_sysname, &expander_sasdev, &expander_sasdev_len );
   
   free( expander_sysname );
   
   if( rc == 0 ) {
      
      // has expander device 
      // get its address
      rc = vdev_sysfs_read_attr( expander_sasdev, "sas_address", &sas_address, &sas_address_len );
      free( expander_sasdev );
      
      if( rc != 0 ) {
         
         free( phy_id );
         return rc;
      }
   }
   
   format_lun_number( parent, &lun );
   
   if( sas_address != NULL ) {
      
      path_prepend(path, "sas-exp%s-phy%s-%s", sas_address, phy_id, lun);
      free( sas_address );
   }
   else {
      
      path_prepend(path, "sas-phy%s-%s", phy_id, lun);
   }
   
   if( lun != NULL ) {
      free( lun );   
   }
   
   free( phy_id );
   
   *new_parent = strdup( parent );
   if( *new_parent == NULL ) {
   
      return -ENOMEM;
   }
    
   return 0;
}


// generate persistent path for iscsi devices
// return 0 on success, and update *path with this device's persistent path information, and set *parent to the next parent device to explore 
// return negative on errror
static int handle_scsi_iscsi( char const* parent, char **path, char** new_parent ) {
   
   char* transportdev = NULL;
   
   char* transportdev_sysname = NULL;
   size_t transportdev_sysname_len = 0;
   
   char* sessiondev = NULL;
   size_t sessiondev_len = 0;
   
   char* target = NULL;
   size_t target_len = 0;
   
   char* connname = NULL;
   
   char* conndev = NULL;
   size_t conndev_len = 0;
   
   char* addr = NULL;
   size_t addr_len = 0;
   
   char* port = NULL;
   size_t port_len = 0;
   
   char* lun = NULL;
   
   int rc = 0;
   
   // find iscsi session 
   transportdev = (char*)parent;
   
   while( 1 ) {
      
      char* transport_parent = NULL;
      size_t transport_parent_len = 0;
      
      rc = vdev_sysfs_get_parent_device( transportdev, &transport_parent, &transport_parent_len );
      if( rc != 0 ) {
         
         return rc;
      }
      
      transportdev = transport_parent;
      if( strncmp( transportdev, "session", strlen("session") ) == 0 ) {
         
         break;
      }
      
      free( transportdev );
   }
   
   // find tranport dev sysname 
   rc = vdev_sysfs_get_sysname( transportdev, &transportdev_sysname, &transportdev_sysname_len );
   if( rc != 0 ) {
      
      free( transportdev );
      return rc;
   }
   
   free( transportdev );
   
   // find iscsi session device 
   rc = vdev_sysfs_device_path_from_subsystem_sysname( "/sys", "iscsi_session", transportdev_sysname, &sessiondev, &sessiondev_len );
   if( rc != 0 ) {
      
      free( transportdev_sysname );
      return rc;
   }
   
   // read the target 
   rc = vdev_sysfs_read_attr( sessiondev, "targetname", &target, &target_len );
   
   free( sessiondev );
   
   if( rc != 0 ) {
      
      free( transportdev_sysname );
      return rc;
   }
   
   rc = asprintf( &connname, "connection%s:0", transportdev_sysname );
   
   free( transportdev_sysname );
   
   if( rc < 0 ) {
      
      free( target );
      free( connname );
      return rc;
   }
   
   // find connection device 
   rc = vdev_sysfs_device_path_from_subsystem_sysname( "/sys", "iscsi_connection", connname, &conndev, &conndev_len );
   
   free( connname );
   
   if( rc != 0 ) {
      
      free( target );
      return rc;
   }
   
   // look up address 
   rc = vdev_sysfs_read_attr( conndev, "persistent_address", &addr, &addr_len );
   if( rc != 0 ) {
      
      free( target );
      return rc;
   }
   
   // look up port 
   rc = vdev_sysfs_read_attr( conndev, "persistent_port", &port, &port_len );
   if( rc != 0 ) {
      
      free( addr );
      free( target );
      return rc;
   }
   
   // make the name!
   format_lun_number( parent, &lun );
   path_prepend( path, "ip-%s:%s-iscsi-%s-%s", addr, port, target, lun );
   
   if( lun != NULL ) {
      
      free( lun );
   }
   
   free( port );
   free( target );
   free( addr );
   
   *new_parent = strdup( parent );
   if( *new_parent == NULL ) {
      
      return -ENOMEM;
   }
   
   return 0;
}



// get scsi host information
// return 0 on success
// return negative on failure 
static int scsi_read_host_info( char const* parent, char** hostdev, int* host, int* bus, int* target, int* lun ) {
   
   int rc = 0;
   
   size_t hostdev_len = 0;
   
   char* name = NULL;
   size_t name_len = 0;
   
   // find scsi host parent device 
   rc = vdev_sysfs_device_path_from_subsystem_sysname( parent, "scsi", "scsi_host", hostdev, &hostdev_len );
   if( rc != 0 ) {
      
      return rc;
   }
   
   // get sysname of scsi host
   rc = vdev_sysfs_get_sysname( parent, &name, &name_len );
   if( rc != 0 ) {
      
      free( *hostdev );
      *hostdev = NULL;
      
      return rc;
   }
   
   // read host, bus, target, and lun
   rc = sscanf( name, "%d:%d:%d:%d", host, bus, target, lun );
   if( rc != 4 ) {
      
      // invalid name 
      free( *hostdev );
      *hostdev = NULL;
      
      free( name );
      return -EINVAL;
   }
   
   free( name );
   
   return 0;
}

/*
 * Rebase host offset to get the local relative number
 *
 * Note: This is by definition racy, unreliable and too simple.
 * Please do not copy this model anywhere. It's just a left-over
 * from the time we had no idea how things should look like in
 * the end.
 *
 * Making assumptions about a global in-kernel counter and use
 * that to calculate a local offset is a very broken concept. It
 * can only work as long as things are in strict order.
 *
 * The kernel needs to export the instance/port number of a
 * controller directly, without the need for rebase magic like 
 * this. Manual driver unbind/bind, parallel hotplug/unplug will
 * get into the way of this "I hope it works" logic.
 */
static int scsi_host_offset( char const* hostdev_sysname ) {
   
   DIR* dir = NULL;
   struct dirent* dent = NULL;
   char* base = NULL;
   char* pos = NULL;
   int basenum = -1;
   int rc = 0;
   
   base = strdup( hostdev_sysname );
   if( base == NULL ) {
      return -ENOMEM;
   }
   
   // sanity check 
   pos = strrchr(base, '/');
   if (pos == NULL) {
      
      free( base );
      return -EINVAL;
   }
   
   // make base its dirname
   pos[0] = '\0';
   
   dir = opendir(base);
   
   if (dir == NULL) {
      
      rc = -errno;
      free( base );
      return rc;
   }
   
   for( dent = readdir( dir ); dent != NULL; dent = readdir( dir ) ) {
      
      char* rest = NULL;
      int i = 0;
      
      // skip . and .. 
      if( dent->d_name[0] == '.' ) {
         continue;
      }
      
      // only regular files 
      if( dent->d_type != DT_DIR && dent->d_type != DT_LNK ) {
         continue;
      }
      
      // only files that start with "host" 
      if( strncmp( dent->d_name, "host", strlen("host") ) != 0 ) {
         continue;
      }
      
      // get base number 
      i = strtoul( &dent->d_name[4], &rest, 10 );
      if( rest[0] != '\0' ) {
         continue;
      }
      
      /*
       * find the smallest number; the host really needs to export its
       * own instance number per parent device; relying on the global host
       * enumeration and plainly rebasing the numbers sounds unreliable
       */
      if( basenum == -1 || i < basenum ) {
         basenum = i;
      }
   }
   
   closedir( dir );
   
   if( basenum == -1 ) {
      
      // not found 
      return -ENOENT;
   }
   
   return basenum;
}


// persistent default scsi path 
// return 0 on success, and update *path with the scsi path information, and set *parent to the next parent device to explore 
// return negative on error.
static int handle_scsi_default( char const* parent, char **path, char** new_parent ) {
   
   int rc = 0;
   int attempts = 5;    // arbitrary
   
   while( 1 ) {
      
      int host = 0, bus = 0, target = 0, lun = 0;
      int host2 = 0, bus2 = 0, target2 = 0, lun2 = 0;
      int host_offset = 0;
      char* hostdev = NULL;
      char* hostdev2 = NULL;
      
      char* hostdev_sysname = NULL;
      size_t hostdev_sysname_len = 0;
      
      // get host info 
      rc = scsi_read_host_info( parent, &hostdev, &host, &bus, &target, &lun );
      
      if( rc != 0 ) {
         
         return rc;
      }
      
      // get host sysname 
      rc = vdev_sysfs_get_sysname( hostdev, &hostdev_sysname, &hostdev_sysname_len );
      if( rc != 0 ) {
         
         free( hostdev );
         return rc;
      }
      
      // get host offset 
      host_offset = scsi_host_offset( hostdev_sysname );
      
      free( hostdev_sysname );
      
      if( host_offset < 0 ) {
         
         return -ENODATA;
      }
      
      // get the host info, again, and verify that the kernel didn't race us 
      rc = scsi_read_host_info( parent, &hostdev2, &host2, &bus2, &target2, &lun2 );
      
      if( rc != 0 ) {
         
         free( hostdev );
         return rc;
      }
      
      // verify that nothing changed 
      if( strcmp(hostdev, hostdev2) != 0 || host != host2 || bus != bus2 || target != target2 || lun != lun2 ) {
         
         free( hostdev );
         free( hostdev2 );
         
         // let the driver settle
         sleep(1);
         
         attempts--;
         if( attempts <= 0 ) {
            
            return -ETIMEDOUT;
         }
         
         continue;
      }
      
      // success
      free( hostdev2 );
      
      // success!
      host -= host_offset;
      
      path_prepend(path, "scsi-%u:%u:%u:%u", host, bus, target, lun);
      
      *new_parent = hostdev;
      break;
   }
   
   return 0;
}



// create persistent path for a hyperv scsi device 
// return 0 on success, and append persistent path info to *path and set *new_parent to the next parent device to explore 
// return negative on error 
static int handle_scsi_hyperv( char const* parent, char **path, char** new_parent ) {
   
   char* hostdev = NULL;
   size_t hostdev_len = 0;
   
   char* vmbusdev = NULL;
   size_t vmbusdev_len = 0;
   
   char* guid_str = NULL;
   size_t guid_str_len = 0;
   
   char* lun = NULL;
   
   char guid[38];
   memset( guid, 0, 38 );
   
   size_t i = 0;
   size_t k = 0;
   
   int rc = 0;
   
   // get scsi host parent device 
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( parent, "scsi", "scsi_host", &hostdev, &hostdev_len );
   if( rc != 0 ) {
      
      return rc;
   }
   
   // get vmbus device--parent of scsi host 
   rc = vdev_sysfs_get_parent_device( hostdev, &vmbusdev, &vmbusdev_len );
   if( rc != 0 ) {
      
      free( hostdev );
      return rc;
   }
   
   // get the guid 
   rc = vdev_sysfs_read_attr( vmbusdev, "device_id", &guid_str, &guid_str_len );
   
   free( hostdev );
   free( vmbusdev );
   
   if( rc != 0 ) {
      
      return rc;
   }
   
   // sanity check 
   if( strlen(guid_str) < 37 || guid_str[0] != '{' || guid_str[36] != '}' ) {
      
      return -EINVAL;
   }
   
   // remove '-'
   for( i = 0, k = 0; i < 36; i++ ) {
      
      if( guid_str[i] == '-' ) {
         continue;
      }
      
      guid[k] = guid_str[i];
      k++;
   }
   guid[k] = '\0';
   
   format_lun_number( parent, &lun );
   path_prepend(path, "vmbus-%s-%s", guid, lun);
   
   if( lun != NULL ) {
      free( lun );
   }
   
   *new_parent = strdup( parent );
   if( *new_parent == NULL ) {
      
      return -ENOMEM;
   }
   
   return 0;
}


// generate persistent device name for a scsi device.  dispatch to implementation-specific handlers.
// return 0 on success, and prepend *path with the persistent name for this device.  Set *new_parent to the next parent to explore, and set 
//      *supported_parent to true if this device has a parent we support for persistent names.
// return negative on failure.
static int handle_scsi( char const* parent, char **path, char** new_parent, bool *supported_parent ) {
   
   char* devtype = NULL;
   size_t devtype_len = 0;
   
   char* id = NULL;
   size_t id_len = 0;
   
   int rc = 0;
   
   // verify that this is a scsi_device 
   rc = vdev_sysfs_uevent_read_key( parent, "DEVTYPE", &devtype, &devtype_len );
   if( rc != 0 ) {
      
      return rc;
   }
   
   if( devtype == NULL || strcmp( devtype, "scsi_device" ) != 0 ) {
      
      // not a scsi device 
      *new_parent = strdup( parent );
      if( *new_parent == NULL ) {
         
         return -ENOMEM;
      }
      
      return 0;
   }
   
   // firewrire?
   rc = vdev_sysfs_read_attr( parent, "ieee1394_id", &id, &id_len );
   free( devtype );
   
   if( rc == 0 ) {
      
      // yup!
      rc = skip_subsystem( parent, "scsi", new_parent );
      if( rc != 0 ) {
         
         free( id );
         return rc;
      }
      
      path_prepend(path, "ieee1394-0x%s", id);
      
      free( id );
      
      *supported_parent = true;
      return 0;
   }
   
   free( id );
   
   if( strstr( parent, "/rport-" ) != NULL ) {
      
      // fibrechannel
      rc = handle_scsi_fibre_channel( parent, path, new_parent );
      if( rc != 0 ) {
         
         return rc;
      }
      
      *supported_parent = true;
      return 0;
   }
   
   if( strstr( parent, "/end_device-" ) != NULL ) {
      
      // SCSI SAS 
      rc = handle_scsi_sas( parent, path, new_parent );
      if( rc != 0 ) {
         
         return rc;
      }
      
      *supported_parent = true;
      return 0;
   }
   
   if( strstr( parent, "/session" ) != NULL ) {
      
      // iSCSI 
      rc = handle_scsi_iscsi( parent, path, new_parent );
      if( rc != 0 ) {
         
         return rc;
      }
      
      *supported_parent = true;
      return 0;
   }
   
   /*
   * We do not support the ATA transport class, it uses global counters
   * to name the ata devices which numbers spread across multiple
   * controllers.
   *
   * The real link numbers are not exported. Also, possible chains of ports
   * behind port multipliers cannot be composed that way.
   *
   * Until all that is solved at the kernel level, there are no by-path/
   * links for ATA devices.
   */
   
   if( strstr( parent, "/ata" ) != NULL ) {
      
      // not supported 
      *new_parent = NULL;
      
      return 0;
   }
   
   if( strstr( parent, "/vmbus_" ) != NULL ) {
      
      rc = handle_scsi_hyperv( parent, path, new_parent );
      if( rc != 0 ) {
         
         return rc;
      }
      
      return 0;
   }
   
   // default 
   rc = handle_scsi_default( parent, path, new_parent );
   
   return rc;
}

// add persistent path name information for a cciss device 
// return 0 on success, update *path, and set *new_parent to the next parent device to explore 
// return negative on failure
static int handle_cciss( char const* parent, char **path, char** new_parent ) {
   
   unsigned int controller = 0, disk = 0;
   
   char* parent_sysname = NULL;
   size_t parent_sysname_len = 0;
   
   int rc = 0;
   
   rc = vdev_sysfs_get_sysname( parent, &parent_sysname, &parent_sysname_len );
   if( rc != 0 ) {
      return rc;
   }
   
   rc = sscanf( parent_sysname, "c%ud%u%*s", &controller, &disk );
   
   free( parent_sysname );
   
   if( rc != 2 ) {
      
      return -ENOENT;
   }
   
   path_prepend(path, "cciss-disk%u", disk);
   
   rc = skip_subsystem( parent, "cciss", new_parent );
   
   return rc;
}


// add persistent path name information for a scsi tape device 
// always succeeds
static void handle_scsi_tape( char const* dev, char **path ) {
    
   // must be the last device in the syspath 
   if( *path != NULL ) {
      return;
   }
   
   char* sysname = NULL;
   size_t sysname_len = 0;
   
   int rc = 0;
   
   rc = vdev_sysfs_get_sysname( dev, &sysname, &sysname_len );
   if( rc != 0 ) {
      
      return;
   }
   
   if( strncmp( sysname, "nst", 3 ) == 0 && strchr( "lma", sysname[3] ) != NULL ) {
      
      path_prepend( path, "nst%c", sysname[3] );
   }
   else if( strncmp( sysname, "st", 2 ) == 0 && strchr( "lma", sysname[2] ) != NULL ) {
      
      path_prepend( path, "st%c", sysname[2] );
   }
   
   free( sysname );
   return;
}


// add persistent path name information for a usb device 
// return 0 on success, update *path with the information, and set *new_parent to the next device to explore 
// return negative on failure 
static int handle_usb( char const *parent, char **path, char** new_parent ) {
   
   int rc = 0;
   
   char* devtype = NULL;
   size_t devtype_len = 0;
   
   char* sysname = NULL;
   size_t sysname_len = 0;
   
   char* port = NULL;
   
   rc = vdev_sysfs_uevent_read_key( parent, "DEVTYPE", &devtype, &devtype_len );
   if( rc != 0 ) {
      
      *new_parent = strdup( parent );
      if( *new_parent == NULL ) {
         
         return -ENOMEM;
      }
      
      return 0;
   }
   
   if( strcmp( devtype, "usb_interface" ) != 0 && strcmp( devtype, "usb_device" ) != 0 ) {
      
      free( devtype );
      
      *new_parent = strdup( parent );
      if( *new_parent == NULL ) {
         
         return -ENOMEM;
      }
       
      return 0;
   }
   
   free( devtype );
   
   rc = vdev_sysfs_get_sysname( parent, &sysname, &sysname_len );
   if( rc != 0 ) {
      
      return rc;
   }
   
   port = strchr( sysname, '-' );
   
   if( port == NULL ) {
      
      *new_parent = strdup( parent );
      if( *new_parent == NULL ) {
         
         return -ENOMEM;
      }
      
      free( sysname );
   
      return 0;
   }
   
   port++;
   
   rc = skip_subsystem( parent, "usb", new_parent );
   if( rc != 0 ) {
      
      free( sysname );

      return rc;
   }
   
   path_prepend( path, "usb-0:%s", port );
   
   free( sysname );
   
   return 0;
}

static int handle_bcma( char const* parent, char **path, char** new_parent ) {
   
   unsigned int core = 0;
   int rc = 0;
   
   char* sysname = NULL;
   size_t sysname_len = 0;
   
   rc = vdev_sysfs_get_sysname( parent, &sysname, &sysname_len );
   if( rc != 0 ) {
      
      return rc;
   }
   
   rc = sscanf( sysname, "bcma%*u:%u", &core );
   
   free( sysname );
   
   if( rc != 1 ) {
      
      return -ENOENT;
   }
   
   path_prepend( path, "bcma-%u", core );
   
   *new_parent = strdup( parent );
   if( *new_parent == NULL ) {
      return -ENOMEM;
   }
   
   return 0;
}


static int handle_ccw( char const* parent, char const* dev, char **path, char** new_parent ) {
   
   char* scsi_dev = NULL;
   size_t scsi_dev_len = 0;
   
   bool handled = false;
   
   int rc = 0;
   
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( dev, "scsi", "scsi_device", &scsi_dev, &scsi_dev_len );
   if( rc == 0 ) {
      
      char* wwpn = NULL;
      size_t wwpn_len = 0;
      
      char* lun = NULL;
      size_t lun_len = 0;
      
      char* hba_id = NULL;
      size_t hba_id_len = 0;
      
      vdev_sysfs_read_attr( scsi_dev, "hba_id", &hba_id, &hba_id_len );
      vdev_sysfs_read_attr( scsi_dev, "wwpn", &wwpn, &wwpn_len );
      vdev_sysfs_read_attr( scsi_dev, "fcp_lun", &lun, &lun_len );
      
      if( hba_id != NULL && lun != NULL && wwpn != NULL ) {
         
         path_prepend(path, "ccw-%s-zfcp-%s:%s", hba_id, wwpn, lun);
         handled = true;
      }
      
      if( hba_id != NULL ) {
         free( hba_id );
      }
      if( lun != NULL ) {
         free( lun );
      }
      if( wwpn != NULL ) {
         free( wwpn );
      }
   }
   
   if( !handled ) {
      
      char* sysname = NULL;
      size_t sysname_len = 0;
      
      rc = vdev_sysfs_get_sysname( parent, &sysname, &sysname_len );
      
      if( rc != 0 ) {
         
         free( scsi_dev );
         return rc;
      }
      
      path_prepend( path, "ccw-%s", sysname );
      
      free( sysname );
   }
   
   free( scsi_dev );
   
   rc = skip_subsystem( parent, "ccw", new_parent );
   if( rc != 0 ) {
      return rc;
   }
   
   return 0;
}


void usage( char const* progname ) {
   
   fprintf(stderr, "Usage: %s /sysfs/path/to/device | /path/to/device/node\n", progname );
}


// entry point
int main( int argc, char** argv ) {
   
   char* parent = NULL;
   size_t parent_len = 0;
   
   char* new_parent = NULL;
   
   char* sysname = NULL;
   size_t sysname_len = 0;
   
   char* path = NULL;
   
   char* subsys = NULL;
   size_t subsys_len = 0;
   
   bool supported_transport = false;
   bool supported_parent = false;
   
   struct stat sb;
   
   int rc = 0;
   
   if( argc != 2 ) {
      usage( argv[0] );
      exit(1);
   }
   
   char* dev = NULL;
   
   // if this is a character device, then look up the device path 
   rc = stat( argv[1], &sb );
   if( rc != 0 ) {
      
      usage( argv[0] );
      exit(1);
   }
   
   if( S_ISCHR( sb.st_mode ) ) {
      
      size_t dev_len = 0;
      rc = vdev_sysfs_get_syspath_from_device( "/sys", sb.st_mode, major( sb.st_rdev ), minor( sb.st_rdev ), &dev, &dev_len );
      if( rc != 0 ) {
         
         fprintf(stderr, "vdev_sysfs_get_syspath_from_device rc = %d\n", rc );
         usage(argv[0]);
         exit(1);
      }
   }
   else {
      
      dev = strdup( argv[1] );
   }
   
   // s390 ccw bus?
   rc = vdev_sysfs_get_parent_with_subsystem_devtype( dev, "ccw", NULL, &parent, &parent_len );
   if( rc == 0 ) {
      
      rc = handle_ccw( parent, dev, &path, &new_parent );
      if( rc != 0 ) {
         exit(2);
      }
      
      goto main_finish;
   }
   
   // walk up the device sysfs path and make the persistent path 
   parent = strdup( dev );
   if( parent == NULL ) {
      
      exit(3);
   }
   
   rc = 0;
   
   while( parent != NULL ) {
      
      vdev_sysfs_read_subsystem( parent, &subsys, &subsys_len );
      
      log_debug("'%s': subsystem '%s'", parent, subsys );
      
      if( subsys != NULL ) {
         
         if( strcmp( subsys, "scsi_tape" ) == 0 ) {
            
            handle_scsi_tape( parent, &path );
         }
         else if( strcmp( subsys, "scsi" ) == 0 ) {
            
            rc = handle_scsi( parent, &path, &new_parent, &supported_parent );
            supported_transport = true;
         }
         else if( strcmp( subsys, "cciss" ) == 0 ) {
            
            rc = handle_cciss( parent, &path, &new_parent );
            supported_transport = true;
         }
         else if( strcmp( subsys, "usb" ) == 0 ) {
            
            rc = handle_usb( parent, &path, &new_parent );
            supported_transport = true;
         }
         else if( strcmp( subsys, "bcma" ) == 0 ) {
            
            rc = handle_bcma( parent, &path, &new_parent );
            supported_transport = true;
         }
         else if( strcmp( subsys, "serio" ) == 0 ) {
            
            int sysnum = 0;
            rc = vdev_sysfs_get_sysnum( parent, &sysnum );
            if( rc == 0 ) {
               
               path_prepend( &path, "serio-%d", sysnum );
            
               rc = skip_subsystem( parent, "serio", &new_parent );
            }
         }
         else if( strcmp( subsys, "pci" ) == 0 ) {
            
            rc = vdev_sysfs_get_sysname( parent, &sysname, &sysname_len );
            if( rc == 0 ) {
               
               path_prepend( &path, "pci-%s", sysname );
               free( sysname );
               
               rc = skip_subsystem( parent, "pci", &new_parent );
            }
            
            supported_parent = true;
         }
         else if( strcmp( subsys, "platform" ) == 0 ) {
            
            rc = vdev_sysfs_get_sysname( parent, &sysname, &sysname_len );
            if( rc == 0 ) {
                  
               path_prepend( &path, "platform-%s", sysname );
               free( sysname );
            
               rc = skip_subsystem( parent, "platform", &new_parent );
            }
            
            supported_parent = true;
            supported_transport = true;
         }
         else if( strcmp( subsys, "acpi" ) == 0 ) {
            
            rc = vdev_sysfs_get_sysname( parent, &sysname, &sysname_len );
            if( rc == 0 ) {
               
               path_prepend( &path, "acpi-%s", sysname );
               free( sysname );
            
               rc = skip_subsystem( parent, "acpi", &new_parent );
            }
            
            supported_parent = true;
         }
         else if( strcmp( subsys, "xen" ) == 0 ) {
            
            rc = vdev_sysfs_get_sysname( parent, &sysname, &sysname_len );
            if( rc == 0 ) {
               
               path_prepend( &path, "xen-%s", sysname );
               free( sysname );
            
               rc = skip_subsystem( parent, "xen", &new_parent );
            }
            
            supported_parent = true;
         }
         else if( strcmp( subsys, "scm" ) == 0 ) {
            
            rc = vdev_sysfs_get_sysname( parent, &sysname, &sysname_len );
            if( rc == 0 ) {
                  
               path_prepend( &path, "scm-%s", sysname );
               free( sysname );
               
               rc = skip_subsystem( parent, "scm", &new_parent );
            }
            
            supported_transport = true;
            supported_parent = true;
         }
      }
   
      if( rc != 0 ) {
         
         exit(4);
      }
      
      if( new_parent != NULL ) {
         
         log_debug("new parent is '%s' (original = '%s')", new_parent, parent );
         
         free( parent );
         parent = new_parent;
         
         new_parent = NULL;
      }
      
      char* next = NULL;
      size_t next_len = 0;
      
      // next device 
      rc = vdev_sysfs_get_parent_device( parent, &next, &next_len );
      if( rc != 0 ) {
         
         if( rc == -ENOENT ) {
            
            break;
         }
         
         // terminal error
         exit(15);
      }
      
      
      log_debug("Parent of '%s' is '%s'\n", parent, next );
      
      free( parent );
      parent = next;
      
      free( subsys );
      subsys = NULL;
   }
   
   /*
    * Do not return devices with an unknown parent device type. They
    * might produce conflicting IDs if the parent does not provide a
    * unique and predictable name.
    */
   if( !supported_parent ) {
      
      log_debug("%s", "Unsupported parent");
      
      if( path != NULL ) {
         free( path );
      }
      path = NULL;
   }
   
   /*
    * Do not return block devices without a well-known transport. Some
    * devices do not expose their buses and do not provide a unique
    * and predictable name that way.
    */
   rc = vdev_sysfs_read_subsystem( dev, &subsys, &subsys_len );
   if( rc != 0 ) {
      
      if( path != NULL ) {
         free( path );
      }
      
      exit(16);
   }
   
   if( strcmp( subsys, "block" ) == 0 && !supported_transport ) {
      
      log_debug("'%s': Unsupported transport", dev);
      
      if( path != NULL ) {
         free( path );
      }
      exit(17);
   }
   
main_finish:
   
   free( dev );
   free( subsys );
   
   if( path != NULL ) {
      
      char tag[4097];
      memset( tag, 0, 4097 );
      
      size_t i = 0;
      char const* p = NULL;
      
      
      /* compose valid udev tag name */
      for (p = path, i = 0; *p != '\0'; p++ ) {
         
         // alphanumeric?
         if ((*p >= '0' && *p <= '9') ||
             (*p >= 'A' && *p <= 'Z') ||
             (*p >= 'a' && *p <= 'z') ||
             *p == '-') {
            
               tag[i++] = *p;
               continue;
         }
         
         // skip leading '_'
         if( i == 0 ) {
            continue;
         }
         
         // avoid second '_'
         if( tag[i-1] == '_' ) {
            continue;
         }
         
         tag[i++] = '_';
      }
      
      /* strip trailing '_' */
      while( i > 0 && tag[i-1] == '_' ) {
         i--;
      }
      tag[i] = '\0';

      vdev_property_add( "VDEV_PERSISTENT_PATH", path );
      vdev_property_add( "VDEV_PERSISTENT_PATH_TAG", tag );
      
      free( path );
      
      vdev_property_print();
      vdev_property_free_all();
      
      exit(0);
   }
   else {
      
      exit(255);
   }
}
