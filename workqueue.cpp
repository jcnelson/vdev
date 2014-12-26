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

#include "workqueue.h"
#include "os/methods.h"
#include "action.h"
#include "vdev.h"
#include "fskit/fskit.h"

// create a request 
// return 0 on success
// return -ENOMEM if we can't allocate device parameters
int vdev_device_request_init( struct vdev_device_request* req, struct vdev_state* state, vdev_device_request_t req_type, char const* path ) {
   
   memset( req, 0, sizeof(struct vdev_device_request) );
   
   if( path != NULL ) {
      req->path = vdev_strdup_or_null( path );
      if( req->path == NULL ) {
         return -ENOMEM;
      }
   }
   
   req->state = state;
   req->type = req_type;
   req->params = new (nothrow) vdev_device_params_t();
   
   if( req->params == NULL ) {
      
      if( req->path != NULL ) {
         free( req->path );
      }
      
      return -ENOMEM;
   }
   
   return 0;
}


// free a request 
int vdev_device_request_free( struct vdev_device_request* req ) {
   
   if( req->params != NULL ) {
      
      delete req->params;
      req->params = NULL;
   }
   
   if( req->path != NULL ) {
      
      free( req->path );
      req->path = NULL;
   }
   
   memset( req, 0, sizeof(struct vdev_device_request) );
   
   return 0;
}

// add a device parameter (must be unique)
// return 0 on success
// return -EEXIST if the parameter exists
// return -ENOMEM if OOM
int vdev_device_request_add_param( struct vdev_device_request* req, char const* key, char const* value ) {
   
   try {
      
      vdev_device_params_t::iterator itr = req->params->find( string(key) );
      if( itr == req->params->end() ) {
         
         (*req->params)[ string(key) ] = string(value);
      }
      else {
         
         return -EEXIST;
      }
   }
   catch ( bad_alloc& ba ) {
      return -ENOMEM;
   }
   
   return 0;
}


// set the path 
int vdev_device_request_set_path( struct vdev_device_request* req, char const* path ) {
   
   if( req->path != NULL ) {
      free( req->path );
   }
   
   req->path = vdev_strdup_or_null( path );
   
   return 0;
}

// set the device 
int vdev_device_request_set_dev( struct vdev_device_request* req, dev_t dev ) {
   
   req->dev = dev;
   return 0;
}
   

// device request sanity check 
// return 0 if valid 
// return -EINVAL if not valid
int vdev_device_request_sanity_check( struct vdev_device_request* req ) {
   
   if( req->path == NULL ) {
      
      vdev_error("request %p missing path\n", req );
      return -EINVAL;
   }
   
   if( req->type == VDEV_DEVICE_INVALID ) {
      
      vdev_error("request %p has no request type\n", req );
      return -EINVAL;
   }
   
   if( req->params == NULL ) {
      
      vdev_error("request %p has no params\n", req );
      return -EINVAL;
   }
   
   return 0;
}


// set the request type 
int vdev_device_request_set_type( struct vdev_device_request* req, vdev_device_request_t req_type ) {
   
   req->type = req_type;
   return 0;
}


// set the request device mode 
int vdev_device_request_set_mode( struct vdev_device_request* req, mode_t mode ) {
   
   req->mode = mode;
   return 0;
}

// handler to add a device (mknod)
// need to fork to do this
static int vdev_device_add( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   if( req->path != NULL && req->dev != 0 && req->mode != 0 ) {
      
      vdev_debug("ADD device %p type %s at %s (%d:%d)\n", req, (S_ISBLK(req->mode) ? "'block'" : S_ISCHR(req->mode) ? "'char'" : "'unknown'"), req->path, major(req->dev), minor(req->dev) );
      
      char* fp = fskit_fullpath( req->state->mountpoint, req->path, NULL );
      
      // call into our mknod() via FUSE (waking up inotify/kqueue listeners)
      rc = mknod( fp, req->mode | 0777, req->dev );
      if( rc != 0 ) {
         
         vdev_error("mknod(%s, dev=(%u, %u)) rc = %d\n", fp, major(req->dev), minor(req->dev), rc );
      }
      
      else {
         
         // call all ADD actions
         rc = vdev_action_run_all( req, req->state->acts, req->state->num_acts );
         if( rc != 0 ) {
            
            vdev_error("vdev_action_run_all(ADD %s, dev=(%u, %u)) rc = %d\n", fp, major(req->dev), minor(req->dev), rc );
         }
      }
      
      free( fp );
   }
   
   // done with this request
   vdev_device_request_free( req );
   free( req );
   
   return rc;
}


// handler to remove a device (unlink)
// need to fork to do this
static int vdev_device_remove( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   if( req->path != NULL ) {
      
      vdev_debug("REMOVE device %p type %s at %s (%d:%d)\n", req, (S_ISBLK(req->mode) ? "'block'" : S_ISCHR(req->mode) ? "'char'" : "'unknown'"), req->path, major(req->dev), minor(req->dev) );
      
      // child
      char* fp = fskit_fullpath( req->state->mountpoint, req->path, NULL );
   
      // call into our unlink() via FUSE (waking up inotify/kqueue listeners)
      rc = 0; unlink( fp );
      if( rc != 0 ) {
         
         vdev_error("unlink(%s) rc = %d\n", fp, rc );
      }
      
      else {
         
         // call all REMOVE actions
         rc = vdev_action_run_all( req, req->state->acts, req->state->num_acts );
         if( rc != 0 ) {
            
            vdev_error("vdev_action_run_all(REMOVE %s) rc = %d\n", fp, rc );
         }
      }
      
      free( fp );
   }
   
   // done with this request 
   vdev_device_request_free( req );
   free( req );
   
   return rc;
}


// enqueue a device request
int vdev_device_request_enqueue( struct fskit_wq* wq, struct vdev_device_request* req ) {
   
   int rc = 0;
   struct fskit_wreq wreq;
   
   // sanity check 
   rc = vdev_device_request_sanity_check( req );
   if( rc != 0 ) {
      
      vdev_error("Invalid device request (type %d)\n", req->type );
      return -EINVAL;
   }
   
   // which handler?
   switch( req->type ) {
      
      case VDEV_DEVICE_ADD: {
         
         fskit_wreq_init( &wreq, vdev_device_add, req, 0 );
         break;
      }
      
      case VDEV_DEVICE_REMOVE: {
         
         fskit_wreq_init( &wreq, vdev_device_remove, req, 0 );
         break;
      }
      
      default: {
         
         vdev_error("Invalid device request type %d\n", req->type );
         return -EINVAL;
      }  
   }
   
   rc = fskit_wq_add( wq, &wreq );
   if( rc != 0 ) {
      
      vdev_error("fskit_wq_add('%s') rc = %d\n", req->path, rc );
   }
   
   return rc;
}
