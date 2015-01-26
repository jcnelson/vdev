/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License version 3 or later as
   published by the Free Software Foundation. For the terms of this
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU Lesser General
   Public License, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU Lesser General Public License for more details.

   Alternatively, you are free to use this program under the terms of the
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#include "libvdev/util.h"
#include "workqueue.h"


// wait for the queue to be empty 
static int vdev_wq_wait_for_empty( struct vdev_wq* wq ) {
   
   pthread_mutex_lock( &wq->waiter_lock );
   
   wq->waiter_count++;
   
   pthread_mutex_unlock( &wq->waiter_lock );
   
   sem_wait( &wq->end_sem );
   
   return 0;
}

// wake up all threads waiting for the queue to be empty 
static int vdev_wq_wakeup_waiters_on_empty( struct vdev_wq* wq ) {
   
   pthread_mutex_lock( &wq->waiter_lock );
   
   int waiter_count = wq->waiter_count;
   wq->waiter_count = 0;
   
   pthread_mutex_unlock( &wq->waiter_lock );
   
   for( int i = 0; i < waiter_count; i++ ) {
      
      sem_post( &wq->end_sem );
   }
   
   return 0;
}

// work queue main method
static void* vdev_wq_main( void* cls ) {

   struct vdev_wq* wq = (struct vdev_wq*)cls;
   
   struct vdev_wreq* work_itr = NULL;
   struct vdev_wreq* next = NULL;
   
   int rc = 0;

   while( wq->running ) {

      // is there work?
      rc = sem_trywait( &wq->work_sem );
      if( rc != 0 ) {
         
         rc = -errno;
         if( rc == -EAGAIN ) {
            
            // no work; wake up anyone waiting for us to be empty
            vdev_wq_wakeup_waiters_on_empty( wq );
            
            // wait for work
            sem_wait( &wq->work_sem );
         }
         else {
            
            // some other fatal error 
            vdev_error("FATAL: sem_trywait rc = %d\n", rc );
            break;
         }
      }
      
      // cancelled?
      if( !wq->running ) {
         break;
      }

      // exchange buffers--we have work to do
      pthread_mutex_lock( &wq->work_lock );

      work_itr = wq->work;
      wq->work = NULL;
      wq->tail = NULL;
      
      pthread_mutex_unlock( &wq->work_lock );
      
      if( work_itr == NULL ) {
         // nothing to do
         continue;
      }

      // safe to use work buffer (we'll clear it out in the process)
      while( work_itr != NULL ) {

         // carry out work
         rc = (*work_itr->work)( work_itr, work_itr->work_data );
         
         if( rc != 0 ) {
            
            vdev_error("WARN: work %p rc = %d\n", work_itr->work, rc );
         }
         
         next = work_itr->next;
         
         vdev_wreq_free( work_itr );
         free( work_itr );
         
         work_itr = next;
      }
   }

   return NULL;
}

// set up a work queue, but don't start it.
// return 0 on success
// return negative on failure:
// * -ENOMEM if OOM
int vdev_wq_init( struct vdev_wq* wq ) {

   int rc = 0;

   memset( wq, 0, sizeof(struct vdev_wq) );
   
   pthread_mutex_init( &wq->work_lock, NULL );
   sem_init( &wq->work_sem, 0, 0 );
   sem_init( &wq->end_sem, 0, 0 );
   
   return rc;
}


// start a work queue
// return 0 on success
// return negative on error:
// * -EINVAL if already started
// * whatever pthread_create errors on
int vdev_wq_start( struct vdev_wq* wq ) {

   if( wq->running ) {
      return -EINVAL;
   }

   int rc = 0;
   pthread_attr_t attrs;

   memset( &attrs, 0, sizeof(pthread_attr_t) );

   wq->running = true;

   rc = pthread_create( &wq->thread, &attrs, vdev_wq_main, wq );
   if( rc != 0 ) {

      wq->running = false;

      rc = -errno;
      vdev_error("pthread_create errno = %d\n", rc );

      return rc;
   }

   return 0;
}

// stop a work queue
// return 0 on success
// return negative on error:
// * -EINVAL if not running
int vdev_wq_stop( struct vdev_wq* wq, bool wait ) {

   if( !wq->running ) {
      return -EINVAL;
   }
   
   if( wait ) {
      
      // wait for the queue to be empty 
      vdev_wq_wait_for_empty( wq );
   }

   wq->running = false;

   // wake up the work queue so it cancels
   sem_post( &wq->work_sem );
   pthread_cancel( wq->thread );

   pthread_join( wq->thread, NULL );

   return 0;
}


// free a work request queue
static int vdev_wq_queue_free( struct vdev_wreq* wqueue ) {

   struct vdev_wreq* next = NULL;
   
   while( wqueue != NULL ) {
      
      next = wqueue->next;
      
      vdev_wreq_free( wqueue );
      free( wqueue );
      
      wqueue = next;
   }
   
   return 0;
}

// free up a work queue
// return 0 on success
// return negative on error:
// * -EINVAL if running
int vdev_wq_free( struct vdev_wq* wq ) {

   if( wq->running ) {
      return -EINVAL;
   }

   // free all
   vdev_wq_queue_free( wq->work );
   
   pthread_mutex_destroy( &wq->work_lock );
   sem_destroy( &wq->work_sem );

   memset( wq, 0, sizeof(struct vdev_wq) );

   return 0;
}

// create a work request
int vdev_wreq_init( struct vdev_wreq* wreq, vdev_wq_func_t work, void* work_data ) {

   memset( wreq, 0, sizeof(struct vdev_wreq) );

   wreq->work = work;
   wreq->work_data = work_data;

   return 0;
}

// free a work request
int vdev_wreq_free( struct vdev_wreq* wreq ) {

   memset( wreq, 0, sizeof(struct vdev_wreq) );
   return 0;
}

// enqueue work
// return 0 on success
// return -EINVAL if the work queue thread isn't running
int vdev_wq_add( struct vdev_wq* wq, struct vdev_wreq* wreq ) {

   int rc = 0;
   struct vdev_wreq* next = NULL;
   
   if( !wq->running ) {
      return -EINVAL;
   }

   // duplicate this work item 
   next = VDEV_CALLOC( struct vdev_wreq, 1 );
   if( next == NULL ) {
      return -ENOMEM;
   }
   
   next->work = wreq->work;
   next->work_data = wreq->work_data;
   next->next = NULL;
   
   pthread_mutex_lock( &wq->work_lock );
   
   if( wq->work == NULL ) {
      // head
      wq->work = next;
      wq->tail = next;
   }
   else {
      // append 
      wq->tail->next = next;
      wq->tail = next;
   }
   
   pthread_mutex_unlock( &wq->work_lock );

   if( rc == 0 ) {
      // have work
      sem_post( &wq->work_sem );
   }

   return rc;
}
