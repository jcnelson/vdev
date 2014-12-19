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

#ifndef _VDEV_WORKQUEUE_H_
#define _VDEV_WORKQUEUE_H_

#include "util.h"

#include <map>
using namespace std;

#define VDEV_NAME_MAX 256

// device request type 
enum vdev_device_request_t {
   VDEV_DEVICE_INVALID = 0,             // invalid request
   VDEV_DEVICE_ADD,
   VDEV_DEVICE_REMOVE,   
   VDEV_DEVICE_ANY                      // only useful for actions
};

typedef map<string, string> vdev_device_params_t;

struct vdev_state;

// device request 
struct vdev_device_request {
   
   // type of request (always initialized)
   vdev_device_request_t type;
   
   // path to the device node 
   char* path;
   
   // device information, for mknod (can't be 0)
   dev_t dev;
   
   // OS-specific driver parameters 
   vdev_device_params_t* params;
   
   // reference to vdev state, so we can call other methods when working
   struct vdev_state* state;
};


extern "C" {

// memory management
int vdev_device_request_init( struct vdev_device_request* req, struct vdev_state* state, vdev_device_request_t type, char const* path );
int vdev_device_request_free( struct vdev_device_request* req );

// setters for device requests (so the OS can build one up)
int vdev_device_request_set_type( struct vdev_device_request* req, vdev_device_request_t req_type );
int vdev_device_request_set_dev( struct vdev_device_request* req, dev_t dev );
int vdev_device_request_set_path( struct vdev_device_request* req, char const* path );
int vdev_device_request_add_param( struct vdev_device_request* req, char const* key, char const* value );

// add a device request
int vdev_device_request_add( struct fskit_wq* wq, struct vdev_device_request* req );

}

#endif