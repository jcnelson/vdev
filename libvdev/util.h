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

#ifndef _VDEV_UTIL_H_
#define _VDEV_UTIL_H_

#define _POSIX_C_SOURCE 1

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
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <attr/xattr.h>

#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <grp.h>
#include <syslog.h>
#include <sys/resource.h>

#include "pstat/libpstat.h"

#define VDEV_WHERESTR "%05d:%05d: [%16s:%04u] %s: "
#define VDEV_WHEREARG (int)getpid(), (int)pthread_self(), __FILE__, __LINE__, __func__

extern int _VDEV_DEBUG_MESSAGES;
extern int _VDEV_INFO_MESSAGES;
extern int _VDEV_WARN_MESSAGES;
extern int _VDEV_ERROR_MESSAGES;
extern int _VDEV_SYSLOG;

#define VDEV_SYSLOG_IDENT "vdev"

#define vdev_debug( format, ... ) \
   do { \
      if( _VDEV_DEBUG_MESSAGES ) { \
         if( _VDEV_SYSLOG ) { \
            syslog( LOG_DAEMON | LOG_DEBUG, format, __VA_ARGS__ ); \
         } \
         else { \
            printf( VDEV_WHERESTR "DEBUG: " format, VDEV_WHEREARG, __VA_ARGS__ ); \
            fflush(stdout); \
         } \
      } \
   } while(0)

   
#define vdev_info( format, ... ) \
   do { \
      if( _VDEV_INFO_MESSAGES ) { \
         if( _VDEV_SYSLOG ) { \
            syslog( LOG_DAEMON | LOG_INFO, format, __VA_ARGS__ ); \
         } \
         else { \
            printf( VDEV_WHERESTR "INFO: " format, VDEV_WHEREARG, __VA_ARGS__ ); \
            fflush(stdout); \
         } \
      } \
   } while(0)

   
#define vdev_warn( format, ... ) \
   do { \
      if( _VDEV_WARN_MESSAGES ) { \
         if( _VDEV_SYSLOG ) { \
            syslog( LOG_DAEMON | LOG_WARNING, format, __VA_ARGS__ ); \
         } \
         else { \
            fprintf(stderr, VDEV_WHERESTR "WARN: " format, VDEV_WHEREARG, __VA_ARGS__); \
            fflush(stderr); \
         } \
      } \
   } while(0)   

   
#define vdev_error( format, ... ) \
   do { \
      if( _VDEV_ERROR_MESSAGES ) { \
         if( _VDEV_SYSLOG ) { \
            syslog( LOG_DAEMON | LOG_ERR, format, __VA_ARGS__ ); \
         } \
         else { \
            fprintf(stderr, VDEV_WHERESTR "ERROR: " format, VDEV_WHEREARG, __VA_ARGS__); \
            fflush(stderr); \
         } \
      } \
   } while(0)
   
#define VDEV_CALLOC(type, count) (type*)calloc( sizeof(type) * (count), 1 )
#define VDEV_FREE_LIST(list) do { if( (list) != NULL ) { for(unsigned int __i = 0; (list)[__i] != NULL; ++ __i) { if( (list)[__i] != NULL ) { free( (list)[__i] ); (list)[__i] = NULL; }} free( (list) ); } } while(0)
#define VDEV_SIZE_LIST(sz, list) for( *(sz) = 0; (list)[*(sz)] != NULL; ++ *(sz) );
#define VDEV_COPY_LIST(dst, src, duper) do { for( unsigned int __i = 0; (src)[__i] != NULL; ++ __i ) { (dst)[__i] = duper((src)[__i]); } } while(0)

#define vdev_strdup_or_null( str )  (str) != NULL ? strdup(str) : NULL

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#ifdef __cplusplus
#define C_LINKAGE_BEGIN extern "C" {
#define C_LINKAGE_END }
#else
#define C_LINKAGE_BEGIN 
#define C_LINKAGE_END
#endif 

typedef int (*vdev_dirent_loader_t)( char const*, void* );

C_LINKAGE_BEGIN

// debug functions
void vdev_set_debug_level( int d );
void vdev_set_error_level( int e );
int vdev_get_debug_level();
int vdev_get_error_level();
int vdev_enable_syslog();

// system functions 
int vdev_daemonize( int* fd_keep, int num_fds );
int vdev_subprocess( char const* cmd, char* const env[], char** output, size_t max_output, int* exit_status );
int vdev_log_redirect( char const* logfile_path );
int vdev_pidfile_write( char const* pidfile_path );

// parser functions 
int vdev_keyvalue_next( char* keyvalue, char** key, char** value );
uint64_t vdev_parse_uint64( char const* uint64_str, bool* success );

// I/O functions 
ssize_t vdev_read_uninterrupted( int fd, char* buf, size_t len );
ssize_t vdev_write_uninterrupted( int fd, char const* buf, size_t len );
int vdev_read_file( char const* path, char* buf, size_t len );
int vdev_write_file( char const* path, char const* buf, size_t len, int flags, mode_t mode );

// directory I/O
int vdev_load_all( char const* dir_path, vdev_dirent_loader_t loader, void* cls );
int vdev_mkdirs( char const* dirp, int start, mode_t mode );
int vdev_rmdirs( char const* dirp );

// passwd/group query
int vdev_get_passwd( char const* username, struct passwd* pwd, char** pwd_buf );
int vdev_get_group( char const* groupname, struct group* grp, char** grp_buf );
int vdev_parse_uid( char const* uid_str, uid_t* uid );
int vdev_parse_gid( char const* gid_str, gid_t* gid );
int vdev_validate_uid( uid_t uid );
int vdev_validate_gid( gid_t gid );

// misc 
char* vdev_fullpath( char const* root, char const* path, char* dest );
char* vdev_dirname( char const* path, char* dest );
size_t vdev_basename_len( char const* path );
char* vdev_basename( char const* path, char* dest );

C_LINKAGE_END

#endif
