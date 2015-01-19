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

#include "hwdb.h"

struct udev_hwdb {
   
   // guard 
   pthread_mutex_t lock;
   
   // ref count 
   int refcount;
   
   // ref to udev parent 
   struct udev* udev;
};


// free this db
static void udev_hwdb_free( struct udev_hwdb* hwdb ) {
   
   pthread_mutex_destroy( hwdb->lock );
   
   memset( hwdb, 0, sizeof( struct udev_hwdb ) );
}

// ref this db 
struct udev_hwdb* udev_hwdb_ref( struct udev_hwdb* hwdb ) {
   
   pthread_mutex_lock( &hwdb->lock );
   
   hwdb->refcount++;
   
   pthread_mutex_unlock( &hwdb->lock );
   
   return hwdb;
}

// unref this db
struct udev_hwdb* udev_hwdb_unref( struct udev_hwdb* hwdb ) {
   
   pthread_mutex_lock( &hwdb->lock );
   
   hwdb->refcount--;
   
   if( hwdb->refcount <= 0 ) {
      
      udev_hwdb_free( hwdb );
      hwdb = NULL;
   }
   else {
   
      pthread_mutex_unlock( &hwdb->lock );
   }
   
   return hwdb;
}

// allocate this db
struct udev_hwdb* udev_hwdb_new( struct udev* udev ) {
   
   struct udev_hwdb* hwdb = VDEV_CALLOC( struct udev_hwdb, 1 );
   
   if( udev_hwdb == NULL ) {
      return NULL;
   }
   
   // populate...
   pthread_mutex_init( &hwdb->lock, NULL );
   hwdb->udev = udev;
   
   // TODO: fill in anything else we may need....
   
   return hwdb;
}

// get hardware properties 
// TODO: not sure what this is supposed to do; research examples....
struct udev_list_entry* udev_hwdb_get_properties_list_entry( struct udev_hwdb* hwdb, char const* modalias, unsigned int flags ) {
   
   return NULL;
}
