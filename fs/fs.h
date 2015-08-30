/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

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

#ifndef _VDEVFS_H_
#define _VDEVFS_H_

#include <fcntl.h>

#include <fskit/fskit.h>
#include <fskit/fuse/fskit_fuse.h>

#include <attr/xattr.h>

#include "libvdev/util.h"
#include "libvdev/config.h"


struct vdevfs {
   
   // configuration 
   struct vdev_config* config;
   
   // arguments 
   int argc;
   char** argv;
   
   // FUSE-filtered arguments
   int fuse_argc;
   char** fuse_argv;
   
   // mountpoint; where /dev is
   char* mountpoint;
   
   // mountpoint dir handle, underneath /dev
   int mountpoint_dirfd;
   
   // fskit core
   struct fskit_fuse_state* fs;
   
   // acls
   struct vdev_acl* acls;
   size_t num_acls; 
   
   // close route handler id
   int close_rh;
};


C_LINKAGE_BEGIN
   
int vdevfs_mkdir( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, mode_t mode, void** inode_cls );
int vdevfs_mknod( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_cls );
int vdevfs_create( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, mode_t mode, void** inode_cls, void** handle_cls );
int vdevfs_open( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, int flags, void** handle_cls );
int vdevfs_read( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, char* buf, size_t len, off_t offset, void* handle_cls );
int vdevfs_write( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, char* buf, size_t len, off_t offset, void* handle_cls );
int vdevfs_close( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, void* handle_cls );
int vdevfs_sync( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent );
int vdevfs_detach( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, void* inode_cls );
int vdevfs_stat( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, struct stat* sb );
int vdevfs_readdir( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents );

int vdevfs_init( struct vdevfs* vdev, int argc, char** argv );
int vdevfs_main( struct vdevfs* vdev, int fuse_argc, char** fuse_argv );
int vdevfs_shutdown( struct vdevfs* vdev );

C_LINKAGE_END

#endif  // _VDEV_FS_H_
