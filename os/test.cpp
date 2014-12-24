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

#ifdef _VDEV_OS_TEST

#include "test.h"

#define INI_MAX_LINE 4096 
#define INI_STOP_ON_FIRST_ERROR 1

#include "ini.h"


// parse a synthetic action 
vdev_device_request_t vdev_test_parse_device_request_type( char const* type ) {
   
   if( strcmp(type, "add") == 0 ) {
      return VDEV_DEVICE_ADD;
   }
   else if( strcmp(type, "remove") == 0 ) {
      return VDEV_DEVICE_REMOVE;
   }
   
   return VDEV_DEVICE_INVALID;
}


// parse a synthetic device event 
static int vdev_test_event_parse( void* userdata, char const* section, char const* name, char const* value ) {
   
   struct vdev_device_request* vreq = (struct vdev_device_request*)userdata;
   
   if( strcmp( section, VDEV_TEST_SECTION ) != 0 ) {
      
      vdev_error("Invalid section '%s'\n", section);
      return 0;
   }
   
   if( strcmp(name, VDEV_TEST_NAME_EVENT) == 0 ) {
      
      vdev_device_request_t type = vdev_test_parse_device_request_type( value );
      vdev_device_request_set_type( vreq, type );
      return 1;
   }
   
   if( strcmp(name, VDEV_TEST_NAME_PATH) == 0 ) {
      
      vdev_device_request_set_path( vreq, value );
      return 1;
   }
   
   if( strcmp(name, VDEV_TEST_NAME_SUBSYSTEM == 0 ) {
      
      if( strcmp(value, "block") == 0 ) {
         
         vdev_device_request_set_mode( vreq, S_IFBLK );
      }
      
      else if( strcmp(value, "char") == 0 ) {
         
         vdev_device_request_set_mode( vreq, S_IFCHR );
      }
      
      return 1;
   }
   
   if( strcmp(name, VDEV_TEST_NAME_MAJOR) == 0 ) {
      
      char* tmp = NULL;
      unsigned int minor_num = minor( vreq->dev );
      unsigned int major_num = (unsigned int)strtol( value, &tmp, 10 );
      
      if( tmp == NULL ) {
         
         vdev_error("Invalid major device number '%s'\n", value );
         return 0;
      }
      
      vdev_device_request_set_dev( vreq, makedev( major_num, minor_num ) );
      
      return 1;
   }
   
   if( strcmp(name, VDEV_TEST_NAME_MINOR) == 0 ) {
      
      char* tmp = NULL;
      unsigned int minor_num = (unsigned int)strtol( value, &tmp, 10 );
      unsigned int major_num = major( vreq->dev );
      
      if( tmp == NULL ) {
         
         vdev_error("Invalid minor device number '%s'\n", value );
         return 0;
      }
      
      vdev_device_request_set_dev( vreq, makedev( major_num, minor_num ) );
      
      return 1;
   }
   
   vdev_error("Unrecognized name '%s'\n", name );
   return 0;
}


// load a synthetic device from a file 
static int vdev_test_event_load_file( FILE* f, struct vdev_device_request* vreq ) {
   
   int rc = 0;
   
   rc = ini_parse_file( f, vdev_test_event_parse, vreq );
   if( rc != 0 ) {
      
      vdev_error("ini_parse_file rc = %d\n", rc );
   }
   else {
      
      rc = vdev_device_request_sanity_check( vreq );
   }
   
   return rc;
}


// load a synthetic device from a path 
static int vdev_test_event_load( char const* path, struct vdev_device_request* vreq ) {
   
   int rc = 0;
   FILE* f = NULL;
   
   f = fopen( path, "r" );
   if( f == NULL ) {
      
      rc = -errno;
      vdev_error("fopen(%s) rc = %d\n", path, rc );
      return rc;
   }
   
   rc = vdev_test_event_load_file( f, vreq );
   
   fclose( f );
   
   return rc;
}


// load a synthetic device event into the test context 
static int vdev_test_event_loader( char const* fp, void* cls ) {
   
   struct vdev_test_context* ctx = (struct vdev_test_context*)cls;
   struct vdev_device_request* vreq = VDEV_CALLOC( struct vdev_device_request, 1 );
   int rc = 0;
   struct stat sb;
   
   if( vreq == NULL ) {
      
      return -ENOMEM;
   }
   
   // skip if not a regular file 
   rc = stat( fp, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("stat(%s) rc = %d\n", fp, rc );
      return rc;
   }
   
   if( !S_ISREG( sb.st_mode ) ) {
      
      return 0;
   }
   
   vdev_debug("Load Synthetic Event %s\n", fp );
   
   rc = vdev_test_event_load( fp, vreq );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_test_event_load(%s) rc = %d\n", fp, rc );
      return rc;
   }
   
   pthread_mutex_lock( &ctx->lock );
   
   try {
      
      ctx->devlist->push_back( vreq );
      sem_post( &ctx->devlist_sem );
   }
   catch( bad_alloc& ba ) {
      
      vdev_device_request_free( vreq );
      free( vreq );
      
      rc = -ENOMEM;
   }
   
   pthread_mutex_unlock( &ctx->lock );

   return rc;
}


// yield devices from the queue 
int vdev_os_next_device( struct vdev_device_request* request, void* cls ) {
   
   struct vdev_test_context* ctx = (struct vdev_test_context*)cls;
   struct vdev_device_request* vreq = NULL;
   
   // next device...
   sem_wait( &ctx->devlist_sem );
   
   pthread_mutex_lock( &ctx->lock );
   
   if( ctx->devlist->size() > 0 ) {
      vreq = ctx->devlist->front();
      ctx->devlist->pop_front();
   }
   
   pthread_mutex_unlock( &ctx->lock );
   
   if( vreq == NULL ) {
      // shouldn't happen, but you never know...
      return 0;
   }
   
   // populate the request, and get rid of ours
   memcpy( request, vreq, sizeof(struct vdev_device_request) );
   
   free( vreq );
   
   return 0;
}


// base initialization
int vdev_os_init( struct vdev_os_context* ctx, void** cls ) {
   
   struct vdev_test_context* ctx = VDEV_CALLOC( struct vdev_test_context, 1 );
   if( ctx == NULL ) {
      return -ENOMEM;
   }
   
   // set up the context 
   ctx->devlist = new (nothrow) vdev_test_device_list_t();
   if( ctx->devlist == NULL ) {
      
      return -ENOMEM;
   }
   
   sem_init( &ctx->devlist_sem, 0, 0 );
   pthread_mutex_init( &ctx->devlist_lock, NULL );
   
   *cls = ctx;
   
   return 0;
}


// seed all events 
int vdev_os_start( void* cls ) {
   
   struct vdev_test_context* ctx = (struct vdev_test_context*)cls;
   int rc = 0;
   
   rc = vdev_load_all( ctx->events_dir, vdev_test_event_loader, ctx );
   if( rc != 0 ) {
      
      vdev_error("vdev_load_all('%s') rc = %d\n", ctx->events_dir, rc );
      return rc;
   }
   
   return 0;
}


// stop all events 
int vdev_os_stop( void* cls ) {
   
}

// shut down test events 
int vdev_os_shutdown( void* cls ) {
   
}


#endif