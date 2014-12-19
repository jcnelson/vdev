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
#include "os/common.h"
#include "workqueue.h"

#include <map>
#include <string>
#include <set>

#define VDEV_CONFIG_DEFAULT_PATH                "/etc/vdev.conf"
#define VDEV_CONFIG_DEFAULT_DEBUG_LEVEL         2
#define VDEV_CONFIG_DEFAULT_LOGFILE_PATH        NULL

using namespace std;

// global vdev state 
struct vdev_state {
   
   // device processing workqueue
   struct fskit_wq device_wq;
   
   // configuration 
   struct vdev_config* config;
   
   // OS context 
   struct vdev_os_context* os;
   
   // fskit state 
   struct fskit_fuse_state* fs;
   
   // acls 
   struct vdev_acl* acls;
   size_t num_acls;
   
   // actions 
   struct vdev_action* acts;
   size_t num_acts;
   
   // pending requests 
   struct vdev_pending_context* pending;
   
   // are we taking events from the OS?
   bool running;
   
   // arguments 
   int argc;
   char** argv;
};


extern "C" {

int vdev_init( struct vdev_state* vdev, struct fskit_fuse_state* fs, int argc, char** argv );
int vdev_start( struct vdev_state* state );
int vdev_stop( struct vdev_state* state );
int vdev_free( struct vdev_state* state );

int vdev_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb );
int vdev_readdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents );

}

#endif
