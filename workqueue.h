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

#define VDEV_NAME_MAX 256

// device request type 
enum vdev_device_request_t {
   VDEV_DEVICE_INVALID = 0,             // invalid request
   VDEV_DEVICE_ADD,
   VDEV_DEVICE_CHANGE,
   VDEV_DEVICE_REMOVE,   
   VDEV_DEVICE_MOVE,
   VDEV_DEVICE_ONLINE,
   VDEV_DEVICE_OFFLINE
};

typedef map<string, string> vdev_device_params_t;

// device request 
struct vdev_device_request {
   
   // type of request (always initialized)
   vdev_device_request_t type;
   
   // OS-specific driver parameters 
   vdev_device_params_t* params;
};

extern "C" {
 
int vdev_device_request_init( struct vdev_device_request* req, vdev_device_request_t type );
int vdev_device_request_free( struct vdev_device_request* req );

int vdev_device_request_set_type( struct vdev_device_request* req, vdev_device_request_t req_type ) ;
int vdev_device_request_add_param( struct vdev_device_request* req, char const* key, char const* value );

int vdev_device_request_add( struct fskit_wq* wq, struct vdev_device_request* req );

}

#endif