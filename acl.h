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

#ifdef _USE_FS

#ifndef _VDEV_ACL_H_
#define _VDEV_ACL_H_

#include "util.h"

#include <regex.h>
#include <dirent.h>

// acl fields
#define VDEV_ACL_NAME                   "vdev-acl"

#define VDEV_ACL_NAME_UID               "uid"
#define VDEV_ACL_NAME_GID               "gid"
#define VDEV_ACL_NAME_PROC_PATH         "bin"
#define VDEV_ACL_NAME_PROC_PREDICATE    "predicate"
#define VDEV_ACL_NAME_PROC_INODE        "inode"

#define VDEV_ACL_DEVICE_REGEX           "paths"

#define VDEV_ACL_NAME_SETUID            "setuid"
#define VDEV_ACL_NAME_SETGID            "setgid"
#define VDEV_ACL_NAME_SETMODE           "setmode"

#define VDEV_ACL_PROC_BUFLEN            65536

// vdev access control list.
struct vdev_acl {
   
   // user to match
   bool has_uid;
   uid_t uid;
   
   // group to match
   bool has_gid;
   gid_t gid;
   
   // process info to match (set at least one; all must match for the ACL to apply)
   bool has_proc;               // if true, at least one of the following is filled in (and the ACL will only apply if the request is from one of the indicated processes)
   char* proc_path;             // path to the allowed process
   char* proc_predicate_cmd;    // command string to run to see if this ACL applies to the calling process (based on the exit code:  0 indicates 'yes, this applies'; nonzero indicates 'no, does not apply')
   bool has_proc_inode;         // whether or not the ACL has an inode check
   ino_t proc_inode;            // process binary's inode
   
   // UID to set on match
   bool has_setuid;
   uid_t setuid;
   
   // GID to set on match
   bool has_setgid;
   gid_t setgid;
   
   // mode to set on match
   bool has_setmode;
   mode_t setmode;
   
   // device node path regexes over which this ACL applies (NULL-terminated)
   char** paths;
   regex_t* regexes;
   size_t num_paths;
};

typedef struct vdev_acl vdev_acl;

// prototype...
struct vdev_config;

C_LINKAGE_BEGIN

int vdev_acl_init( struct vdev_acl* acl );
int vdev_acl_load_all( char const* dir_path, struct vdev_acl** ret_acls, size_t* ret_num_acls );
int vdev_acl_free( struct vdev_acl* acl );
int vdev_acl_free_all( struct vdev_acl* acl_list, size_t num_acls );

int vdev_acl_apply_all( struct vdev_config* conf, struct vdev_acl* acls, size_t num_acls, char const* path, struct pstat* caller_proc, uid_t caller_uid, gid_t caller_gid, struct stat* sb );

C_LINKAGE_END

#endif

#endif  // _USE_FS