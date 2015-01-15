/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

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

#ifndef _VDEV_FS_H_
#define _VDEV_FS_H_

#ifdef _USE_FS

#include "fskit/fskit.h"
#include "fskit/fuse/fskit_fuse.h"

#include "util.h"

struct vdev_state;

struct vdev_fs {
   
   // pipe between the fs and the back-end event daemon, for sending over the back-end's copy of the mountpoint
   int pipe_front;
   int pipe_back;
   
   // fskit state (front-end)
   struct fskit_fuse_state* fs;
   
   // acls (front-end)
   struct vdev_acl* acls;
   size_t num_acls;
   
   // is the filesystem ready?
   bool running;
};

extern "C" {

   
int vdev_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb );
int vdev_readdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents );

int vdev_frontend_init( struct vdev_fs* fs_frontend, struct vdev_state* vdev );
int vdev_frontend_main( struct vdev_fs* fs_frontend, int fuse_argc, char** fuse_argv );
int vdev_frontend_stop( struct vdev_fs* fs_frontend );
int vdev_frontend_shutdown( struct vdev_fs* fs_frontend );

int vdev_frontend_send_mount_info( struct vdev_state* state );
int vdev_frontend_recv_mount_info( int pipe_back, char** ret_mountpoint, size_t* ret_mountpoint_len );

};

#else  // _USE_FS

// no-op structure
struct vdev_fs {
   
   // keep GCC happy
   int pipe_front;
   int pipe_back;
};

#endif

#endif  // _VDEV_FS_H_
