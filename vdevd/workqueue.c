/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License version 3 or later as
   published by the Free Software Foundation. For the terms of this
   license, see LICENSE.GPLv3+ or <http://www.gnu.org/licenses/>.

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
#include "os/common.h"
#include "vdev.h"

// wait for the queue to be drained
static void vdev_wq_wait_for_empty( struct vdev_wq* wq ) {
   
   pthread_mutex_lock( &wq->waiter_lock );
   
   wq->num_waiters++;
   
   pthread_mutex_unlock( &wq->waiter_lock );
   
   sem_wait( &wq->end_sem );
}


// signal any waiting threds that the queue is empty 
static void vdev_wq_signal_empty( struct vdev_wq* wq ) {
   
   volatile int num_waiters = 0;
   
   pthread_mutex_lock( &wq->waiter_lock );
   
   num_waiters = wq->num_waiters;
   wq->num_waiters = 0;
   
   pthread_mutex_unlock( &wq->waiter_lock );
   
   for( int i = 0; i < num_waiters; i++ ) {
      
      sem_post( &wq->end_sem );
   }
}


// work queue main method
// always succeeds
static void* vdev_wq_main( void* cls ) {

   struct vdev_wq* wq = (struct vdev_wq*)cls;
   
   struct vdev_wreq* work_itr = NULL;
   struct vdev_wreq* next = NULL;
   
   struct vdev_state* state = wq->state;
   
   int rc = 0;

   while( wq->running ) {

      // is there work?
      rc = sem_trywait( &wq->work_sem );
      if( rc != 0 ) {
         
         rc = -errno;
         if( rc == -EAGAIN ) {
            
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
         
         if( vdev_os_context_is_flushed( state->os ) ) {
         
            // no work; wake up anyone waiting for us to have finished the initial devices
            vdev_signal_wq_flushed( state );
         }
         
         vdev_wq_signal_empty( wq );
            
         continue;
      }

      // safe to use work buffer (we'll clear it out in the process)
      while( work_itr != NULL ) {

         // carry out work
         rc = (*work_itr->work)( work_itr, work_itr->work_data );
         
         if( rc != 0 ) {
            
            vdev_warn("work %p rc = %d\n", work_itr->work, rc );
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
int vdev_wq_init( struct vdev_wq* wq, struct vdev_state* state ) {

   int rc = 0;

   memset( wq, 0, sizeof(struct vdev_wq) );
   
   rc = pthread_mutex_init( &wq->work_lock, NULL );
   if( rc != 0 ) {
      
      return -abs(rc);
   }
   
   rc = pthread_mutex_init( &wq->waiter_lock, NULL );
   if( rc != 0 ) {
      
      pthread_mutex_destroy( &wq->work_lock );
      return -abs(rc);
   }
   
   sem_init( &wq->work_sem, 0, 0 );
   sem_init( &wq->end_sem, 0, 0 );
   
   wq->state = state;
   
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
// always succeeds
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
   pthread_mutex_destroy( &wq->waiter_lock );
   
   sem_destroy( &wq->work_sem );
   sem_destroy( &wq->end_sem );

   memset( wq, 0, sizeof(struct vdev_wq) );

   return 0;
}

// create a work request
// always succeeds
int vdev_wreq_init( struct vdev_wreq* wreq, vdev_wq_func_t work, void* work_data ) {

   memset( wreq, 0, sizeof(struct vdev_wreq) );

   wreq->work = work;
   wreq->work_data = work_data;

   return 0;
}

// free a work request
// always succeeds
int vdev_wreq_free( struct vdev_wreq* wreq ) {

   memset( wreq, 0, sizeof(struct vdev_wreq) );
   return 0;
}

// enqueue work
// return 0 on success
// return -EINVAL if the work queue thread isn't running
// return -ENOMEM if OOM
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
