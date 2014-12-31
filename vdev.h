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

#include "fskit/fskit.h"
#include "fskit/fuse/fskit_fuse.h"

#include "acl.h"
#include "config.h"
#include "util.h"
#include "os/common.h"
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
   
   // mountpoint; replicated from fs->mountpoint for the back-end
   char* mountpoint;
   
   // pipe between the front-end and back-end, for sending over the back-end's copy of the mountpoint
   int pipe_front;
   int pipe_back;
   
   // fskit state (front-end)
   struct fskit_fuse_state* fs;
   
   // acls (front-end)
   struct vdev_acl* acls;
   size_t num_acls;
   
   // FUSE arguments (front-end)
   int fuse_argc;
   char** fuse_argv;
   
   // OS context (back-end)
   struct vdev_os_context* os;
   
   // actions (back-end)
   struct vdev_action* acts;
   size_t num_acts;
   
   // pending requests (back-end)
   struct vdev_pending_context* pending;
   
   // device processing workqueue (back-end)
   struct fskit_wq device_wq;
   
   // are we taking events from the OS? (back-end)
   bool running;
};


extern "C" {

int vdev_init( struct vdev_state* vdev, int argc, char** argv );
int vdev_backend_init( struct vdev_state* vdev );
int vdev_frontend_init( struct vdev_state* vdev );

int vdev_backend_start( struct vdev_state* vdev );
int vdev_frontend_send_mount_info( struct vdev_state* state );

int vdev_frontend_main( struct vdev_state* state );
int vdev_backend_main( struct vdev_state* state );

int vdev_frontend_stop( struct vdev_state* state );
int vdev_backend_stop( struct vdev_state* state );

int vdev_free( struct vdev_state* state );

int vdev_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb );
int vdev_readdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents );

}

#endif
