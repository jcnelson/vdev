/*
   loginfs: a FUSE filesystem to allow unprivileged users to access privileged files on UNIX-like systems.
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

#ifndef _LOGINFS_ENTRY_H_
#define _LOGINFS_ENTRY_H_

#include "debug.h"

#include <map>
#include <locale>

using namespace std;

// prototypes
struct loginfs_entry;

// types
typedef pair<long, loginfs_entry*> loginfs_dirent;
typedef vector<loginfs_dirent> loginfs_entry_set;

#define LOGINFS_ENTRY_TYPE_DEAD         0
#define LOGINFS_ENTRY_TYPE_FILE         1
#define LOGINFS_ENTRY_TYPE_DIR          2

// root's UID
#define LOGINFS_SYS_USER                0

// permissions checks
#define LOGINFS_ENTRY_IS_READABLE( mode, node_user, node_group, user, group ) ((user) == LOGINFS_SYS_USER || ((mode) & S_IROTH) || ((node_group) == (group) && ((mode) & S_IRGRP)) || ((node_user) == (user) && ((mode) & S_IRUSR)))
#define LOGINFS_ENTRY_IS_DIR_READABLE( mode, node_user, node_group, user, group ) ((user) == LOGINFS_SYS_USER || ((mode) & S_IXOTH) || ((node_group) == (group) && ((mode) & S_IXGRP)) || ((node_user) == (user) && ((mode) & S_IXUSR)))
#define LOGINFS_ENTRY_IS_WRITEABLE( mode, node_user, node_group, user, group ) (((user) == LOGINFS_SYS_USER || (mode) & S_IWOTH) || ((node_group) == (group) && ((mode) & S_IWGRP)) || ((node_user) == (user) && ((mode) & S_IWUSR)))
#define LOGINFS_ENTRY_IS_EXECUTABLE( mode, node_user, node_group, user, group ) IS_DIR_READABLE( mode, node_user, node_group, user, group )


// loginfs inode structure
struct loginfs_entry {
   uint64_t file_id;             // inode number
   uint8_t type;                 // type of inode
   char* name;
   
   uint64_t owner;
   uint64_t group;
   
   mode_t mode;
   
   int64_t ctime_sec;
   int32_t ctime_nsec;
   
   int64_t mtime_sec;
   int32_t mtime_nsec;
   
   int64_t atime_sec;
   int32_t atime_nsec;
   
   int32_t open_count;
   int32_t link_count;
   
   bool deletion_in_progress;   // set to true if this node is in the process of being unlinked 
   
   // if this is a directory, this is allocated and points to a loginfs_entry_set 
   loginfs_entry_set* children;
   
   // lock governing access to this structure
   pthread_rwlock_t lock;
};

// loginfs file handle 
struct loginfs_file_handle {
   struct loginfs_entry* fent;
   
   char* path;
   
   uint64_t file_id;
   
   int open_count;

   // lock governing access to this structure
   pthread_rwlock_t lock;
};

// loginfs directory handle 
struct loginfs_dir_handle {
   struct loginfs_entry* dent;
   
   // lock governing access to this structure
   pthread_rwlock_t lock;
};


// loginfs core structure 
struct loginfs_core {
   
   struct loginfs_entry* root;
   
   // lock governing access to this structure
   pthread_rwlock_t lock;
};

// path utilities 
long loginfs_entry_name_hash( char const* name );
char* loginfs_fullpath( char const* parent, char const* child, char* output );

// entry sets
void loginfs_entry_set_insert( loginfs_entry_set* set, char const* name, struct loginfs_entry* child );
void loginfs_entry_set_insert_hash( loginfs_entry_set* set, long hash, struct loginfs_entry* child );
struct loginfs_entry* loginfs_entry_set_find_name( loginfs_entry_set* set, char const* name );
struct loginfs_entry* loginfs_entry_set_find_hash( loginfs_entry_set* set, long nh );
bool loginfs_entry_set_remove( loginfs_entry_set* set, char const* name );
bool loginfs_entry_set_remove_hash( loginfs_entry_set* set, long nh );
bool loginfs_entry_set_replace( loginfs_entry_set* set, char const* name, struct loginfs_entry* replacement );
unsigned int loginfs_entry_set_count( loginfs_entry_set* set );
struct loginfs_entry* loginfs_entry_set_get( loginfs_entry_set::iterator* itr );
long loginfs_entry_set_get_name_hash( loginfs_entry_set::iterator* itr );

// locking 
int loginfs_entry_rlock2( struct loginfs_entry* fent, char const* from_str, int line_no );
int loginfs_entry_wlock2( struct loginfs_entry* fent, char const* from_str, int line_no );
int loginfs_entry_unlock2( struct loginfs_entry* fent, char const* from_str, int line_no );

#define loginfs_entry_rlock( fent ) loginfs_entry_rlock2( fent, __FILE__, __LINE__ )
#define loginfs_entry_wlock( fent ) loginfs_entry_wlock2( fent, __FILE__, __LINE__ )
#define loginfs_entry_unlock( fent ) loginfs_entry_unlock2( fent, __FILE__, __LINE__ )

int loginfs_file_handle_rlock( struct loginfs_file_handle* fh );
int loginfs_file_handle_wlock( struct loginfs_file_handle* fh );
int loginfs_file_handle_unlock( struct loginfs_file_handle* fh );

int loginfs_dir_handle_rlock( struct loginfs_dir_handle* dh );
int loginfs_dir_handle_wlock( struct loginfs_dir_handle* dh );
int loginfs_dir_handle_unlock( struct loginfs_dir_handle* dh );

int loginfs_core_rlock( struct loginfs_core* core );
int loginfs_core_wlock( struct loginfs_core* core );
int loginfs_core_unlock( struct loginfs_core* core );

// path resolution 
struct loginfs_entry* loginfs_entry_resolve_path_cls( struct loginfs_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err, int (*ent_eval)( struct loginfs_entry*, void* ), void* cls );
struct loginfs_entry* loginfs_entry_resolve_path( struct loginfs_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err );

#endif