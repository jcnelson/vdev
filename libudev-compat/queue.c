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

#include "queue.h"

struct udev_queue {
   
   // guard 
   pthread_mutex_lock lock;
   
   // refcount 
   int refcount;
   
   // udev ref 
   struct udev* udev;
};


// free me 
static void udev_queue_free( struct udev_queue* queue ) {
   
   pthread_mutex_destroy( &queue->lock );
   
   memset( queue, 0, sizeof(struct udev_queue) );
}


// ref me 
struct udev_queue* udev_queue_ref( struct udev_queue* queue ) {
   
   pthread_mutex_lock( &queue->lock );
   
   queue->refcount++;
   
   pthread_mutex_unlock( &queue->lock );
   
   return queue;
}

// unref me 
struct udev_queue* udev_queue_unref( struct udev_queue* queue ) {
   
   pthread_mutex_lock( &queue->lock );
   
   queue->refcount--;
   
   if( queue->refcount <= 0 ) {
      
      // done 
      udev_queue_free( queue );
      queue = NULL;
   }
   else {
   
      pthread_mutex_unlock( &queue->lock );
   }
   
   return queue;
}

// make a new queue 
struct udev_queue* udev_queue_new( struct udev* udev ) {
   
   struct udev_queue* queue = VDEV_CALLOC( struct udev_queue, 1 );
   
   if( queue == NULL ) {
      return NULL;
   }
   
   pthread_mutex_init( &queue->lock );
   queue->udev = udev;
   
   // TODO finish 
   return queue;
}

// get udev 
struct udev* udev_queue_get_udev( struct udev_queue* queue ) {
   
   struct udev* u = NULL;
   
   pthread_mutex_lock( &queue->lock );
   
   u = queue->udev;
   
   pthread_mutex_unlock( &queue->lock );
   
   return u
}

// is udev active?
// since vdev stores everything statically, the answer is always 'yes'
int udev_queue_get_udev_is_active( struct udev_queue* queue ) {
   return 1;
}


// is udev processing any events?
// the answer is always 'yes'
int udev_queue_get_queue_is_empty( struct udev_queue* queue ) {
   return 1;
}

// deprecated...
int udev_queue_get_seqnum_is_finished( struct udev_queue* queue ) {
   return udev_queue_get_queue_is_empty( queue );
}

// deprecated...
int udev_queue_get_seqnum_sequence_is_finished( struct udev_queue* queue ) {
   return udev_queue_get_queue_is_empty( queue );
}

// deprecated...
// TODO: figure out what this is supposed to do
struct udev_list_entry* udev_queue_get_queued_list_entry( struct udev_queue* queue ) {
   return NULL;
}

// deprecated...
// TODO: finish 
unsigned long long int udev_queue_get_kernel_seqnum( struct udev_queue* queue ) {
   
   return 0;
}

// deprecated...
// TODO: finish 
unsigned long long int udev_queue_get_udev_seqnum( struct udev_queue* queue ) {
   
   return 0;
}

// not really documented...figure out what this is supposed to do 
// TODO
int udev_queue_get_fd( struct udev_queue* queue ) {
   return 0;
}

// not really documented...figure out what this is supposed to do 
int udev_queue_flush( struct udev_queue* queue ) {
   return 0;
}
