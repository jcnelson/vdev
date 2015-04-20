/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 or later as 
   published by the Free Software Foundation. For the terms of this 
   license, see LICENSE.GPLv3+ or <http://www.gnu.org/licenses/>.

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

#ifndef _VDEV_DEVICE_H_
#define _VDEV_DEVICE_H_

#include "libvdev/util.h"
#include "libvdev/param.h"
#include "workqueue.h"

#define VDEV_NAME_MAX 256

#define VDEV_METADATA_PREFIX            "metadata/"

#define VDEV_DEVICE_PATH_UNKNOWN        "UNKNOWN"

#define VDEV_METADATA_PARAM_INSTANCE    "vdev_instance"

// device request type 
typedef enum {
   VDEV_DEVICE_INVALID = 0,             // invalid request
   VDEV_DEVICE_ADD,
   VDEV_DEVICE_REMOVE,
   VDEV_DEVICE_CHANGE,
   VDEV_DEVICE_ANY                      // only useful for actions
} vdev_device_request_t;

struct vdev_state;

// device request 
struct vdev_device_request {
   
   // type of request (always initialized)
   vdev_device_request_t type;
   
   // path to the device node to create (if we're making one at all)
   // If we're creating a network interface, this is the interface name.
   char* path;
   
   // renamed path (used internally)
   char* renamed_path;
   
   // device numbers, for mknod
   dev_t dev;
   
   // device mode: character or block device
   mode_t mode;
   
   // OS-specific driver parameters 
   vdev_params* params;
   
   // reference to vdev state, so we can call other methods when working
   struct vdev_state* state;
   
   // reference to the next item, since this structure often gets used for linked lists 
   struct vdev_device_request* next;
};


C_LINKAGE_BEGIN

// memory management
int vdev_device_request_init( struct vdev_device_request* req, struct vdev_state* state, vdev_device_request_t type, char const* path );
int vdev_device_request_free( struct vdev_device_request* req );

// setters for device requests (so the OS can build one up)
int vdev_device_request_set_type( struct vdev_device_request* req, vdev_device_request_t req_type );
int vdev_device_request_set_dev( struct vdev_device_request* req, dev_t dev );
int vdev_device_request_set_mode( struct vdev_device_request* req, mode_t mode );
int vdev_device_request_set_path( struct vdev_device_request* req, char const* path );
int vdev_device_request_add_param( struct vdev_device_request* req, char const* key, char const* value );

// environment variables 
int vdev_device_request_to_env( struct vdev_device_request* req, char*** env, size_t* num_env );

// add a device request to the work queue
int vdev_device_request_enqueue( struct vdev_wq* wq, struct vdev_device_request* req );

// sanity check structure 
int vdev_device_request_sanity_check( struct vdev_device_request* req );

// device metadata 
char* vdev_device_metadata_fullpath( char const* mountpoint, char const* device_path );

// add/remove devices 
int vdev_device_add( struct vdev_device_request* req );
int vdev_device_remove( struct vdev_device_request* req );

C_LINKAGE_END

#endif