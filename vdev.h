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

#ifndef _VDEV_H_
#define _VDEV_H_

#include "acl.h"
#include "fs.h"
#include "config.h"
#include "util.h"
#include "os/common.h"
#include "device.h"
#include "workqueue.h"

#include <map>
#include <string>
#include <set>

#define VDEV_CONFIG_DEFAULT_PATH                "/etc/vdev.d/vdev.conf"
#define VDEV_CONFIG_DEFAULT_DEBUG_LEVEL         0
#define VDEV_CONFIG_DEFAULT_LOGFILE_PATH        NULL

using namespace std;

// global vdev state 
struct vdev_state {
   
   // configuration 
   struct vdev_config* config;
   
   // arguments 
   int argc;
   char** argv;
   
   // fs front-end
   struct vdev_fs* fs_frontend;
   
   // FUSE arguments (front-end)
   // pretty much unused if _USE_FS is not defined
   int fuse_argc;
   char** fuse_argv;
   
   // mountpoint; where /dev is
   char* mountpoint;
   
   // debug level 
   int debug_level;
   
   // OS context (back-end)
   struct vdev_os_context* os;
   
   // actions (back-end)
   struct vdev_action* acts;
   size_t num_acts;
   
   // pending requests (back-end)
   struct vdev_pending_context* pending;
   
   // device processing workqueue (back-end)
   struct vdev_wq device_wq;
   
   // are we taking events from the OS? (back-end)
   bool running;
};


extern "C" {

int vdev_init( struct vdev_state* vdev, int argc, char** argv );
int vdev_shutdown( struct vdev_state* state );

int vdev_backend_init( struct vdev_state* vdev );
int vdev_backend_start( struct vdev_state* vdev );
int vdev_backend_main( struct vdev_state* state );
int vdev_backend_stop( struct vdev_state* state );

int vdev_send_mount_info( int pipe_front, char const* mountpoint );
int vdev_recv_mount_info( int pipe_back, char** ret_mountpoint );

}

#endif
