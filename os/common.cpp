/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common.h"
#include "workqueue.h"
#include "methods.h"

// yield new devices 
void* vdev_os_yield_devices( void* cls ) {
   
   struct vdev_os_context* vos = (struct vdev_os_context*)cls;
   int rc = 0;
   
   while( vos->running ) {
      
      // make a device request
      struct vdev_device_request* vreq = VDEV_CALLOC( struct vdev_device_request, 1 );
      
      if( vreq == NULL ) {
         // OOM
         break;
      }
      
      // next device request
      rc = vdev_device_request_init( vreq, vos->state, VDEV_DEVICE_INVALID, NULL );
      if( rc != 0 ) {
         
         free( vreq );
         
         vdev_error("vdev_device_request_init rc = %d\n", rc );
         break;
      }
      
      // yield the next device 
      rc = vdev_os_next_device( vreq, vos->os_cls );
      if( rc != 0 ) {
         
         vdev_device_request_free( vreq );
         free( vreq );
         
         vdev_error("vdev_os_next_device rc = %d\n", rc );
         continue;
      }
      
      // post the event to the device work queue
      rc = vdev_device_request_add( &vos->state->device_wq, vreq );
      
      if( rc != 0 ) {
         
         vdev_device_request_free( vreq );
         free( vreq );
         
         vdev_error("vdev_device_request_add rc = %d\n", rc );
         
         continue;
      }
   }
   
   return NULL;
}

// set up a vdev os context
int vdev_os_context_init( struct vdev_os_context* vos, struct vdev_state* state ) {
   
   int rc = 0;
   
   memset( vos, 0, sizeof(struct vdev_os_context) );
   
   vos->state = state;
   
   // set up OS state 
   rc = vdev_os_init( vos, &vos->os_cls );
   if( rc != 0 ) {
      
      vdev_error("vdev_os_init rc = %d\n", rc );
      return rc;
   }
   
   return 0;
}


// initialize OS-specific state, and start the listening thread 
int vdev_os_context_start( struct vdev_os_context* vos ) {
   
   int rc = 0;
   pthread_attr_t attrs;
   
   // sanity check 
   if( vos->running ) {
      return -EINVAL;
   }
   
   // start processing requests
   memset( &attrs, 0, sizeof(pthread_attr_t) );
   
   vos->running = true;
   rc = pthread_create( &vos->thread, &attrs, vdev_os_yield_devices, vos );
   
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("pthread_create rc = %d\n", rc );
      
      return rc;
   }
   else {
      
      // start up OS-specific event feeding 
      rc = vdev_os_start( vos->os_cls );
      if( rc != 0 ) {
         
         vdev_error("vdev_os_start rc = %d\n", rc );
         return rc;
      }
      
      return 0;
   }
}

// stop the listening thread 
int vdev_os_context_stop( struct vdev_os_context* vos ) {
   
   int rc = 0;
   
   if( !vos->running ) {
      return -EINVAL;
   }
   
   // stop getting work requests 
   rc = vdev_os_stop( vos->os_cls );
   if( rc != 0 ) {
      
      vdev_error("vdev_os_stop rc = %d\n", rc );
      return rc;
   }
   
   // cancel and join with the listener
   vos->running = false;
   pthread_cancel( vos->thread );
   pthread_join( vos->thread, NULL );
   
   return 0;
}

// free memory 
int vdev_os_context_free( struct vdev_os_context* vos, void** cls ) {
   
   // nothing to do yet 
   if( cls != NULL ) {
      *cls = vos->os_cls;
   }
   
   return 0;
}
