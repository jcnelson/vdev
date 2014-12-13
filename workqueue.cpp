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

// create a request 
// return 0 on success
// return -ENOMEM if we can't allocate device parameters
int vdev_device_request_init( struct vdev_device_request* req, vdev_device_request_t req_type ) {
   
   memset( req, 0, sizeof(struct vdev_device_request) );
   
   req->type = req_type;
   req->params = new (nothrow) vdev_device_params_t();
   
   if( req->params == NULL ) {
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


// set the request type 
int vdev_device_request_set_type( struct vdev_device_request* req, vdev_device_request_t req_type ) {
   
   req->type = req_type;
   return 0;
}


// handler to add a device 
static int vdev_device_add( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   // TODO
   return rc;
}

// handler to remove a device 
static int vdev_device_remove( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   // TODO
   return rc;
}

// handler to move a device 
static int vdev_device_move( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   // TODO
   return rc;
}


// handler to change a device 
static int vdev_device_change( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   // TODO
   return rc;
}

// handler to put a device online
static int vdev_device_online( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   // TODO
   return rc;
}

// handler to put a device offline 
static int vdev_device_offline( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   // TODO
   return rc;
}

// enqueue a device request
int vdev_device_request_add( struct fskit_wq* wq, struct vdev_device_request* req ) {
   
   // TODO
   return 0;
}
