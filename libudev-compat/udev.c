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

#include "udev.h"

struct udev {
   
   // guard 
   pthread_mutex_t lock;
   
   // reference count
   int refcount;
   
   // logger
   void (*log_fn)( struct udev*, int, char const* int, char const* char const*, va_args );
   
   // log priority 
   int log_priority;
   
   // user data 
   void* userdata;
};


// free up a udev 
static void udev_free( struct udev* udev ) {
   
   pthread_mutex_destroy( &udev->lock );
   
   memset( udev, 0, sizeof(struct udev) );
}


// reference this...
struct udev* udev_ref( struct udev* udev ) {

   pthread_mutex_lock( &udev->lock );
   
   udev->refcount++;
   
   pthread_mutex_unlock( &udev->lock );
   
   return udev;
}

// unreference this
struct udev* udev_unref( struct udev* udev ) {
   
   pthread_mutex_lock( &udev->lock );
   
   udev->refcount--;
   
   if( udev->refcount <= 0 ) {
      
      // free up 
      udev_free( udev );
      udev = NULL;
   }
   
   else {
      
      pthread_mutex_unlock( &udev->lock );
   }
   
   return udev;
}

// new udev!
struct udev* udev_new( void ) {
   
   struct udev* u = VDEV_CALLOC( struct udev, 1 );
   if( u == NULL ) {
      return u;
   }
   
   pthread_mutex_init( &u->lock );
   
   // TODO: read config file and set up udev further
   
   return u;
}

// set logger 
void udev_set_log_fn( struct udev* udev, void (*log_fn)( struct udev*, int, char const*, int, char const*, char const*, va_args ) ) {
   
   pthread_mutex_lock( &udev->lock );
   
   udev->log_fn = log_fn;
   
   pthread_mutex_unlock( &udev->lock );
   
   return;
}


// log priority 
int udev_get_log_priority( struct udev* udev ) {
   
   int ret = 0;
   
   pthread_mutex_lock( &udev->lock );
   
   ret = udev->log_priority;
   
   pthread_mutex_unlock( &udev->lock );
   
   return ret;
}


// set log priority 
void udev_set_log_priority( struct udev* udev, int priority ) {
   
   pthread_mutex_lock( &udev->lock );
   
   udev->log_priority = priority;
   
   pthread_mutex_unlock( &udev->lock );
   
   return ret;
}


// get userdata 
void* udev_get_userdata( struct udev* udev ) {
   
   void* ret = NULL;
   
   pthread_mutex_lock( &udev->lock );
   
   ret = udev->userdata;
   
   pthread_mutex_unlock( &udev->lock );
   
   return ret;
}

// set userdata 
void udev_set_userdata( struct udev* udev, void* userdata ) {
   
   pthread_mutex_lock( &udev->lock );
   
   udev->userdata = userdata;
   
   pthread_mutex_unlock( &udev->lock );
   
   return;
}
