/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 or later as 
   published by the Free Software Foundation. For the terms of this 
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU General
   Public License, but WITHOUT ANY WARRANTY; without even the implied 
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   Alternatively, you are free to use this program under the terms of the 
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or 
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#include "device.h"

struct udev_device {
   
   // guard 
   pthread_mutex_lock lock;
   
   // ref count 
   int refcount;
   
   // udev ref 
   struct udev* udev;
   
   // TODO
};

// free me!
static void udev_device_free( struct udev_device* dev ) {
   
   pthread_mutex_destroy( &dev->lock );
   
   memset( dev, 0, sizeof(struct udev_device) );
}

// ref me!
struct udev_device* udev_device_ref( struct udev_device* dev ) {
   
   pthread_mutex_lock( &dev->lock );
   
   dev->refcount ++;
   
   pthread_mutex_unlock( &dev->lock );
   return dev;
}

// unref me!
struct udev_device* udev_device_unref( struct udev_device* dev ) {
   
   pthread_mutex_lock( &dev->lock );
   
   dev->refcount --;
   
   if( dev->refcount <= 0 ) {
      
      udev_device_free( dev );
      dev = NULL;
   }
   else {
   
      pthread_mutex_unlock( &dev->lock );
   }
   
   return dev;
}

// get udev context
struct udev* udev_device_get_udev( struct udev_device* dev ) {
   
   struct udev* ret = NULL;
   
   pthread_mutex_lock( &dev->lock );
   
   ret = dev->udev;
   
   pthread_mutex_unlock( &dev->lock );
   
   return ret;
}

// make device from sysfs path 
struct udev_device* udev_device_new_from_syspath( struct udev* udev, char const* syspath ) {
   
   // TODO 
   return NULL;
}

// make a device from device number and type 
struct udev_device* udev_device_new_from_devnum( struct udev* udev, char type, dev_t devnum ) {
   
   // TODO 
   return NULL;
}

// make a device from subsystem and system name 
struct udev_device* udev_device_new_from_subsystem_sysname( struct udev* udev, char const* subsystem, char const* sysname ) {

   // TODO 
   return NULL;
}

// make a device from a device id 
// TODO: research device id 
struct udev_device* udev_device_new_from_device_id( struct udev* udev, char const* id ) {
   
   // TODO 
   return NULL;
}

// make a device from the environment variables 
// TODO: research this 
struct udev_device* udev_device_new_from_environment( struct udev* udev ) {
   
   // TODO 
   return NULL;
}

// get parent device 
struct udev_device* udev_device_get_parent( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}


// get a parent with a particular device type and subsystem 
struct udev_device* udev_device_get_parent_with_subsystem_devtype( struct udev_device* dev, char const* subsystem, char const* devtype ) {

   // TODO 
   return NULL;
}

// get device devpath 
char const* udev_device_get_devpath( struct udev_device* dev ) {
   
   // TODO
   return NULL;
}


// get device subsystem 
char const* udev_device_get_subsystem( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}
 
// get devtype string 
char const* udev_device_get_devtype( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get sysfs path 
char const* udev_device_get_syspath( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}


// get system name 
char const* udev_device_get_sysname( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get instance number of the device 
char const* udev_device_get_sysnum( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get device node name
char const* udev_device_get_devnode( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// is the device initialized?
int udev_device_get_is_initialized( struct udev_device* dev ) {
   
   // TODO 
   return 0;
}

// get links pointing to the device file 
struct udev_device* udev_device_get_devlinks_list_entry( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get device properties 
struct udev_device* udev_device_get_properties_list_entry( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get device tags 
struct udev_device* udev_device_get_tags_list_entry( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get property value 
char const* udev_device_get_property_value( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get kernel driver name    
char const* udev_device_get_driver( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get device major/minor 
dev_t udev_device_get_devnum( struct udev_device* dev ) {
   
   // TODO 
   return 0;
}

// get device action (add, remove, etc.)
char const* udev_device_get_action( struct udev_device* dev ) {
   
   // TODO 
   return 0;
}

// get sysfs attribute 
char const* udev_device_get_sysattr_value( struct udev_device* dev, char const* sysattr ) {
   
   // TODO 
   return 0;
}

// set sysfs attribute 
int udev_device_set_sysattr_value( struct udev_device* dev, char const* sysattr, char* value ) {
   
   // TODO 
   return 0;
}

// get list of sysfs attributes 
struct udev_device* udev_device_get_sysattr_list_entry( struct udev_device* dev ) {
   
   // TODO 
   return NULL;
}

// get device sequence number 
unsigned long long int udev_device_get_seqnum( struct udev_device* dev ) {
   
   // TODO 
   return 0;
}

// get microseconds since the device was initialized  
unsigned long long int udev_device_get_usec_since_initialized( struct udev_device* dev ) {
   
   // TODO 
   return 0;
}

// does a device have a tag?
int udev_device_has_tag( struct udev_device* dev, char const* tag ) {
   
   // TODO 
   return 0;
}