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

#ifndef _VDEV_UTIL_H_
#define _VDEV_UTIL_H_

#include <limits.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <semaphore.h>
#include <signal.h>
#include <openssl/sha.h>
#include <regex.h>
#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <curl/curl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <math.h>
#include <sys/mman.h>

#include <pwd.h>
#include <sys/types.h>
#include <grp.h>

#include "fskit/fskit.h"
#include "pstat/libpstat.h"

#define WHERESTR "%05d:%05d: [%16s:%04u] %s: "
#define WHEREARG (int)getpid(), (int)vdev_os_gettid(), __FILE__, __LINE__, __func__

extern int _DEBUG_MESSAGES;
extern int _ERROR_MESSAGES;

#define vdev_debug( format, ... ) fskit_debug( format, __VA_ARGS__ )
#define vdev_error( format, ... ) fskit_error( format, __VA_ARGS__ )

#define VDEV_CALLOC(type, count) (type*)calloc( sizeof(type) * (count), 1 )
#define VDEV_FREE_LIST(list) do { if( (list) != NULL ) { for(unsigned int __i = 0; (list)[__i] != NULL; ++ __i) { if( (list)[__i] != NULL ) { free( (list)[__i] ); (list)[__i] = NULL; }} free( (list) ); } } while(0)
#define VDEV_SIZE_LIST(sz, list) for( *(sz) = 0; (list)[*(sz)] != NULL; ++ *(sz) );
#define VDEV_COPY_LIST(dst, src, duper) do { for( unsigned int __i = 0; (src)[__i] != NULL; ++ __i ) { (dst)[__i] = duper((src)[__i]); } } while(0)

#define vdev_strdup_or_null( str )  (str) != NULL ? strdup(str) : NULL

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

typedef int (*vdev_dirent_loader_t)( char const*, void* );

extern "C" {

// debug functions
void vdev_set_debug_level( int d );
void vdev_set_error_level( int e );
int vdev_get_debug_level();
int vdev_get_error_level();

// shell functions 
int vdev_subprocess( char const* cmd, char** output, size_t max_output, int* exit_status );

// I/O functions 
ssize_t vdev_read_uninterrupted( int fd, char* buf, size_t len );
ssize_t vdev_write_uninterrupted( int fd, char const* buf, size_t len );

// directory I/O
int vdev_load_all( char const* dir_path, vdev_dirent_loader_t loader, void* cls );

// passwd/group query
int vdev_get_passwd( char const* username, struct passwd* pwd, char** pwd_buf );
int vdev_get_group( char const* groupname, struct group* grp, char** grp_buf );

}

#endif
