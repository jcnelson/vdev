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

#ifndef _VDEV_H_
#define _VDEV_H_

#include "fskit/fskit.h"
#include "fskit/fuse/fskit_fuse.h"

#include "acl.h"
#include "config.h"
#include "util.h"

#include <map>
#include <string>
#include <set>

using namespace std;

// global vdev state 
struct vdev_state {
   
   // device processing workqueue
   struct fskit_wq* device_wq;
   
   // configuration 
   struct vdev_config* config;
   
   // mountpoint 
   char* mountpoint;
};

struct vdev_inode {
   
   // ACLs that apply to this device node
   vdev_acl_list_t* acls;
};

extern "C" {

int vdev_mknod( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_data );
int vdev_mkdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* dent, mode_t mode, void** inode_data );
int vdev_detach( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, void* inode_data );
int vdev_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb );
int vdev_readdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents );

}

#endif
