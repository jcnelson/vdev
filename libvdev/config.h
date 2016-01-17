/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 or later as 
   published by the Free Software Foundation. For the terms of this 
   license, see LICENSE.GPLv3+ or <http://www.gnu.org/licenses/>.

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

#ifndef _VDEV_CONFIG_H_
#define _VDEV_CONFIG_H_

#include "util.h"
#include "param.h"

#include <getopt.h>

#define VDEV_CONFIG_NAME        "vdev-config"
#define VDEV_OS_CONFIG_NAME     "vdev-OS"

#define VDEV_CONFIG_ACLS          "acls"
#define VDEV_CONFIG_ACTIONS       "actions"
#define VDEV_CONFIG_HELPERS       "helpers"
#define VDEV_CONFIG_DEFAULT_MODE  "default_permissions"
#define VDEV_CONFIG_DEFAULT_POLICY "default_policy"     // ACL allow or deny
#define VDEV_CONFIG_PIDFILE_PATH  "pidfile"
#define VDEV_CONFIG_LOGFILE_PATH  "logfile"
#define VDEV_CONFIG_LOG_LEVEL     "loglevel"
#define VDEV_CONFIG_MOUNTPOINT    "mountpoint"
#define VDEV_CONFIG_COLDPLUG_ONLY "coldplug_only"
#define VDEV_CONFIG_FOREGROUND    "foreground"
#define VDEV_CONFIG_PRESEED       "preseed"

#define VDEV_CONFIG_INSTANCE_NONCE_LEN 32
#define VDEV_CONFIG_INSTANCE_NONCE_STRLEN (2*VDEV_CONFIG_INSTANCE_NONCE_LEN + 1)

#define VDEV_OS_QUIRK_DEVICE_EXISTS      0x1       // set this bit if the OS already has the device file--i.e. vdevd is not expected to create it

#define vdev_config_has_OS_quirk( quirk_field, quirk ) (((quirk_field) & (quirk)) != 0)
#define vdev_config_set_OS_quirk( quirk_field, quirk ) quirk_field |= (quirk)

// structure for both file configuration and command-line options
struct vdev_config {
   
   // config file path (used by opts)
   char* config_path;
   
   // preseed script 
   char* preseed_path;
   
   // ACLs directory 
   char* acls_dir;
   
   // actions directory 
   char* acts_dir;
   
   // helpers directory 
   char* helpers_dir;
   
   // default policy (0 for deny, 1 for allow)
   int default_policy;
   
   // debug level
   int debug_level;
   
   // error level 
   int error_level;
   
   // PID file path 
   char* pidfile_path;
   
   // logfile path (set to "syslog" to send directly to syslog)
   char* logfile_path;
   
   // path to where /dev lives 
   char* mountpoint;
   
   // coldplug only?
   bool coldplug_only;
   
   // run in the foreground 
   bool foreground;
   
   // OS-specific configuration (for keys under "OS")
   vdev_params* os_config;
   
   // default permission bits for mknod 
   mode_t default_mode;
   
   // printable 256-bit instance nonce--randomly generated and unique per execution 
   char instance_str[VDEV_CONFIG_INSTANCE_NONCE_STRLEN];
   
   // bitfield of OS-specific quirks 
   uint64_t OS_quirks;
};

C_LINKAGE_BEGIN

int vdev_config_init( struct vdev_config* conf );
int vdev_config_load( char const* path, struct vdev_config* conf );
int vdev_config_load_file( FILE* file, struct vdev_config* conf );
int vdev_config_free( struct vdev_config* conf );

int vdev_config_usage( char const* progname );
int vdev_config_load_from_args( struct vdev_config* config, int argc, char** argv, int* fuse_argc, char** fuse_argv );
int vdev_config_fullpaths( struct vdev_config* config );

C_LINKAGE_END

#endif
