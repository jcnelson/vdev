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
int vdev_os_main( struct vdev_os_context* vos ) {
   
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
      
      vdev_debug("Next device: type=%d path=%s dev=%u mode=%o\n", vreq->type, vreq->path, vreq->dev, vreq->mode );
      
      // post the event to the device work queue
      rc = vdev_device_request_enqueue( &vos->state->device_wq, vreq );
      
      if( rc != 0 ) {
         
         vdev_device_request_free( vreq );
         free( vreq );
         
         vdev_error("vdev_device_request_add rc = %d\n", rc );
         
         continue;
      }
   }
   
   return 0;
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
   
   vos->running = true;
   
   return 0;
}


// free memory 
int vdev_os_context_free( struct vdev_os_context* vos ) {

   if( vos != NULL ) {
      vdev_os_shutdown( vos->os_cls );
      vos->os_cls = NULL;
      
      memset( vos, 0, sizeof(struct vdev_os_context) );
   }
   
   return 0;
}
