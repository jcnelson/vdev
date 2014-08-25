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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <pthread.h>

#include <vector>
#include <map>

using namespace std;

// prototypes
struct loginfs_entry;

typedef pair<uint64_t, loginfs_entry*> loginfs_dirent;
typedef map<loginfs_dirent> loginfs_entry_set;

// loginfs inode structure
struct loginfs_entry {
   uint64_t file_id;             // inode 
   uint8_t ftype;                   // type of file 
   char* name;
   
   uint64_t owner;
   uint64_t group;
   
   mode_t mode;
   
   int64_t ctime_sec;
   int32_t ctime_nsec;
   
   int64_t mtime_sec;
   int32_t mtime_sec;
   
   int64_t atime_sec;
   int32_t atime_nsec;
   
   int32_t open_count;
   int32_t link_count;
   
   pthread_rwlock_t lock;
};

// loginfs file handle 
struct loginfs_file_handle {
   struct loginfs_entry* fent;
   
   char* path;
   
   uint64_t file_id;
   
   int open_count;
   
   pthread_rwlock_t lock;
};

// loginfs directory handle 
struct loginfs_dir_handle {
   uint8_t ftype;
};


// loginfs core structure 
struct loginfs_core {
   
};

#endif