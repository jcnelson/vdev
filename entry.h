/*
   License goes here
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