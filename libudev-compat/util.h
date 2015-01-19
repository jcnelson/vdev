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

#ifndef _LIBUDEV_COMPAT_UTIL_H_
#define _LIBUDEV_COMPAT_UTIL_H_

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

#define VDEV_WHERESTR "%05d:%05d: [%16s:%04u] %s: "
#define VDEV_WHEREARG (int)getpid(), (int)pthread_self(), __FILE__, __LINE__, __func__

extern int _VDEV_DEBUG_MESSAGES;
extern int _VDEV_ERROR_MESSAGES;

#define vdev_debug( format, ... ) \
   do { \
      if( _VDEV_DEBUG_MESSAGES ) { \
         printf( VDEV_WHERESTR format, VDEV_WHEREARG, __VA_ARGS__ ); \
         fflush(stdout); \
      } \
   } while(0)


#define vdev_error( format, ... ) \
   do { \
      if( _VDEV_ERROR_MESSAGES ) { \
         fprintf(stderr, VDEV_WHERESTR format, VDEV_WHEREARG, __VA_ARGS__); \
         fflush(stderr); \
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

C_LINKAGE_BEGIN 

int udev_util_encode_string( char const* str, char* str_enc, size_t len );

C_LINKAGE_END 

#endif