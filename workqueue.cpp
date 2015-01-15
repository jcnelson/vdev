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

#include "workqueue.h"
#include "util.h"

// work queue main method
static void* vdev_wq_main( void* cls ) {

   struct vdev_wq* wq = (struct vdev_wq*)cls;
   vdev_wq_queue_t* work = NULL;
   struct vdev_wreq wreq;
   int rc = 0;

   while( wq->running ) {

      // wait for work
      sem_wait( &wq->work_sem );

      // cancelled?
      if( !wq->running ) {
         break;
      }

      // exchange buffers--we have work
      pthread_mutex_lock( &wq->work_lock );

      work = wq->work;

      if( wq->work == wq->work_1 ) {
         wq->work = wq->work_2;
      }
      else {
         wq->work = wq->work_1;
      }

      pthread_mutex_unlock( &wq->work_lock );

      // safe to use work buffer (we'll clear it out)
      while( work->size() > 0 ) {

         // next work
         wreq = work->front();
         work->pop();

         // carry out work
         rc = (*wreq.work)( &wreq, wreq.work_data );
         
         if( rc != 0 ) {
            
            vdev_error("WARN: work %p rc = %d\n", wreq.work, rc );
         }
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

   wq->work_1 = new (nothrow) vdev_wq_queue_t;
   wq->work_2 = new (nothrow) vdev_wq_queue_t;

   if( wq->work_1 == NULL || wq->work_2 == NULL ) {

      if( wq->work_1 != NULL ) {
         delete wq->work_1;
      }
      
      if( wq->work_2 != NULL ) {
         delete wq->work_2;
      }
      
      return -ENOMEM;
   }

   wq->work = wq->work_1;

   pthread_mutex_init( &wq->work_lock, NULL );
   sem_init( &wq->work_sem, 0, 0 );

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
int vdev_wq_stop( struct vdev_wq* wq ) {

   if( !wq->running ) {
      return -EINVAL;
   }

   wq->running = false;

   // wake up the work queue so it cancels
   sem_post( &wq->work_sem );
   pthread_cancel( wq->thread );

   pthread_join( wq->thread, NULL );

   return 0;
}


// free a work request queue
static int vdev_wq_queue_free( vdev_wq_queue_t* wqueue ) {

   if( wqueue != NULL ) {
      while( wqueue->size() > 0 ) {

         struct vdev_wreq wreq = wqueue->front();
         wqueue->pop();

         vdev_wreq_free( &wreq );
      }
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
   vdev_wq_queue_free( wq->work_1 );
   vdev_wq_queue_free( wq->work_2 );

   if( wq->work_1 != NULL ) {
      delete wq->work_1;
   }
   
   if( wq->work_2 != NULL ) {
      delete wq->work_2;
   }
   
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

   if( !wq->running ) {
      return -EINVAL;
   }

   pthread_mutex_lock( &wq->work_lock );

   try {
      wq->work->push( *wreq );
   }
   catch( bad_alloc& ba ) {
      rc = -ENOMEM;
   }

   pthread_mutex_unlock( &wq->work_lock );

   if( rc == 0 ) {
      // have work
      sem_post( &wq->work_sem );
   }

   return rc;
}
