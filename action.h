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

#ifndef _VDEV_ACTION_H_
#define _VDEV_ACTION_H_

#include "util.h"
#include "workqueue.h"

// action fields
#define VDEV_ACTION_NAME                "vdev-action"

#define VDEV_ACTION_NAME_EVENT          "event"
#define VDEV_ACTION_NAME_PATH           "path"
#define VDEV_ACTION_NAME_RENAME         "rename_command"
#define VDEV_ACTION_NAME_SHELL          "command"
#define VDEV_ACTION_NAME_ASYNC          "async"

#define VDEV_ACTION_EVENT_ADD           "add"
#define VDEV_ACTION_EVENT_REMOVE        "remove"
#define VDEV_ACTION_EVENT_ANY           "any"

// vdev action to take on an event 
struct vdev_action {
   
   // what kind of request triggers this?
   vdev_device_request_t trigger;
   
   // device path to match on 
   char* path;
   regex_t path_regex;
   
   // command to run to rename the matched path, if needed
   char* rename_command;
   
   // command to run once the device state change is processed
   char* command;
   
   // OS-specific fields to match on
   vdev_device_params_t* dev_params;
   
   // synchronous or asynchronous 
   bool async;
};

extern "C" {

int vdev_action_init( struct vdev_action* act, vdev_device_request_t trigger, char* path, char* command, bool async );
int vdev_action_add_param( struct vdev_action* act, char const* name, char const* value );
int vdev_action_free( struct vdev_action* act );
int vdev_action_free_all( struct vdev_action* act_list, size_t num_acts );

int vdev_action_load( char const* path, struct vdev_action* act );
int vdev_action_load_file( FILE* f, struct vdev_action* act );

int vdev_action_load_all( char const* dir, struct vdev_action** acts, size_t* num_acts );

int vdev_action_create_path( struct vdev_device_request* vreq, struct vdev_action* acts, size_t num_acts, char** path );
int vdev_action_run_commands( struct vdev_device_request* vreq, struct vdev_action* acts, size_t num_acts );

};

#endif