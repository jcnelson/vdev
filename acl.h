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

#ifndef _VDEV_ACL_H_
#define _VDEV_ACL_H_

#include "util.h"

#include <pwd.h>
#include <sys/types.h>
#include <grp.h>
#include <regex.h>
#include <dirent.h>

// acl fields
#define VDEV_ACL_NAME                   "acl"

#define VDEV_ACL_NAME_UID               "uid"
#define VDEV_ACL_NAME_GID               "gid"
#define VDEV_ACL_NAME_PROC_PATH         "bin"
#define VDEV_ACL_NAME_PROC_PIDLIST      "pidlist"
#define VDEV_ACL_NAME_PROC_SHA256       "sha256"
#define VDEV_ACL_NAME_PROC_INODE        "inode"

#define VDEV_ACL_DEVICE_REGEX           "devices"

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
   unsigned char* proc_sha256;  // sha256 of the allowed process binary
   char* proc_pidlist_cmd;      // command string to run to get the list of PIDs
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

extern "C" {

int vdev_acl_init( struct vdev_acl* acl );
int vdev_acl_load_all( char const* dir_path, struct vdev_acl** ret_acls, size_t* ret_num_acls );
int vdev_acl_free( struct vdev_acl* acl );
int vdev_acl_free_all( struct vdev_acl* acl_list, size_t num_acls );

int vdev_acl_apply_all( struct vdev_acl* acls, size_t num_acls, char const* path, struct pstat* caller_proc, uid_t caller_uid, gid_t caller_gid, struct stat* sb );

}

#endif