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

#ifndef _VDEV_CONFIG_H_
#define _VDEV_CONFIG_H_

#include "fskit/fskit.h"
#include "util.h"

#define VDEV_CONFIG_NAME        "config"
#define VDEV_OS_CONFIG_NAME     "OS"

#define VDEV_CONFIG_FIRMWARE_DIR  "firmware"
#define VDEV_CONFIG_PSTAT         "proc_check"
#define VDEV_CONFIG_ACLS          "acls"
#define VDEV_CONFIG_ACTIONS       "actions"

#define VDEV_CONFIG_NAME_PSTAT_HASH "hash"

typedef map<string, string> vdev_config_map_t;

struct vdev_config {
   
   // firmware directory 
   char* firmware_dir;
   
   // ACLs directory 
   char* acls_dir;
   
   // actions directory 
   char* acts_dir;
   
   // process stat discipline 
   int pstat_discipline;
   
   // OS-specific configuration (for keys under "OS")
   vdev_config_map_t* os_config;
};

extern "C" {

int vdev_config_init( struct vdev_config* conf );
int vdev_config_load( char const* path, struct vdev_config* conf );
int vdev_config_load_file( FILE* file, struct vdev_config* conf );
int vdev_config_free( struct vdev_config* conf );

};

#endif