/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

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


#ifndef _VDEV_LINUX_HELPERS_COMMON_H_
#define _VDEV_LINUX_HELPERS_COMMON_H_

#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory.h>
#include <strings.h>
#include <time.h>
#include <dirent.h>

#define DEBUG 1

#define VDEV_WHERESTR "%05d: [%16s:%04u] %s: "
#define VDEV_WHEREARG (int)getpid(), __FILE__, __LINE__, __func__

#define log_debug(x, ...) if( DEBUG ) { fprintf(stderr, VDEV_WHERESTR x "\n", VDEV_WHEREARG, __VA_ARGS__); }
#define log_error(x, ...) if( 1 ) { fprintf(stderr,VDEV_WHERESTR x "\n", VDEV_WHEREARG, __VA_ARGS__); }

#ifndef memzero 
#define memzero(b, z) memset( b, 0, z )
#endif

#ifndef streq
#define streq(a, b) (strcmp(a, b) == 0)
#endif

#ifndef strneq 
#define strneq(a, b, len) (strncmp(a, b, len) == 0)
#endif

#ifndef strcaseeq
#define strcaseeq(a, b) (strcasecmp(a, b) == 0)
#endif

#ifndef strscpy
#define strscpy(dest, destsize, src) strncpy(dest, src, destsize)
#endif

#ifndef initialize_srand 
#define initialize_srand() do { struct timespec ts; clock_gettime( CLOCK_MONOTONIC, &ts ); srand( ts.tv_sec + ts.tv_nsec ); } while(0)
#endif



// expanding list of properties 
struct vdev_property {
   
   char* name;
   char* value;
   
   struct vdev_property* next;
};

// string methods
int vdev_util_replace_whitespace(const char *str, char *to, size_t len);
int vdev_whitelisted_char_for_devnode(char c, const char *white);
int vdev_utf8_encoded_to_unichar(const char *str);
int vdev_utf8_encoded_valid_unichar(const char *str);
int vdev_util_replace_chars(char *str, const char *white);
int vdev_util_encode_string(const char *str, char *str_enc, size_t len);
int vdev_util_rstrip( char* str );

// device properties
int vdev_property_add( char const* name, char const* value );
int vdev_property_print( void );
int vdev_property_free_all( void );

// sysfs methods 
int vdev_sysfs_read_attr( char const* sysfs_device_path, char const* attr_name, char** value, size_t* value_len );
int vdev_sysfs_uevent_get_key( char const* uevent_buf, size_t uevent_buflen, char const* key, char** value, size_t* value_len );
int vdev_sysfs_get_parent_with_subsystem_devtype( char const* sysfs_device_path, char const* subsystem_name, char const* devtype_name, char** devpath, size_t* devpath_len );
int vdev_sysfs_read_device_path( char const* sysfs_dir, char** devpath, size_t* devpath_len );
int vdev_sysfs_device_path_from_subsystem_sysname( char const* sysfs_mount, char const* subsystem, char const* sysname, char** devpath, size_t* devpath_len );
int vdev_sysfs_get_parent_device( char const* dev_path, char** ret_parent_device, size_t* ret_parent_device_len );
int vdev_sysfs_read_subsystem( char const* devpath, char** ret_subsystem, size_t* ret_subsystem_len );
int vdev_sysfs_get_sysname( char const* devpath, char** sysname, size_t* sysname_len );

// file operations 
int vdev_read_file( char const* path, char** file_buf, size_t* file_buf_len );

#endif