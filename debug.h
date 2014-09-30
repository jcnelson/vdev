/*
   vdev: a FUSE filesystem to allow unprivileged users to access privileged files on UNIX-like systems.
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

#ifndef _VDEV_DEBUG_H_
#define _VDEV_DEBUG_H_

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
#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/syscall.h> 
#include <inttypes.h>

pid_t gettid(void);

#define WHERESTR "%05d:%05d: [%16s:%04u] %s: "
#define WHEREARG (int)getpid(), (int)gettid(), __FILE__, __LINE__, __func__

extern int _DEBUG_MESSAGES;
extern int _ERROR_MESSAGES;

#ifdef dbprintf
#undef dbprintf
#endif

#ifdef errorf
#undef errorf
#endif

#define dbprintf( format, ... ) do { if( _DEBUG_MESSAGES ) { printf( WHERESTR format, WHEREARG, __VA_ARGS__ ); fflush(stdout); } } while(0)
#define errorf( format, ... ) do { if( _ERROR_MESSAGES ) { fprintf(stderr, WHERESTR format, WHEREARG, __VA_ARGS__); fflush(stderr); } } while(0)

#define CALLOC_LIST(type, count) (type*)calloc( sizeof(type) * (count), 1 )
#define FREE_LIST(list) do { for(unsigned int __i = 0; (list)[__i] != NULL; ++ __i) { if( (list)[__i] != NULL ) { free( (list)[__i] ); (list)[__i] = NULL; }} free( (list) ); } while(0)
#define SIZE_LIST(sz, list) for( *(sz) = 0; (list)[*(sz)] != NULL; ++ *(sz) );
#define VECTOR_TO_LIST(ret, vec, type) do { ret = CALLOC_LIST(type, ((vec).size() + 1)); for( vector<type>::size_type __i = 0; __i < (vec).size(); ++ __i ) ret[__i] = (vec).at(__i); } while(0)
#define COPY_LIST(dst, src, duper) do { for( unsigned int __i = 0; (src)[__i] != NULL; ++ __i ) { (dst)[__i] = duper((src)[__i]); } } while(0)
#define DUP_LIST(type, dst, src, duper) do { unsigned int sz = 0; SIZE_LIST( &sz, src ); dst = CALLOC_LIST( type, sz + 1 ); COPY_LIST( dst, src, duper ); } while(0)

#define strdup_or_null( str )  (str) != NULL ? strdup(str) : NULL

extern int _debug_locks;

#endif 
