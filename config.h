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

typedef map<string, string> vdev_config_map_t;

struct vdev_config {
   
   // firmware directory 
   char* firmware_dir;
   
   // OS-specific configuration (for keys that start with OS_
   vdev_config_map_t* os_config;
};

extern "C" {
 
int vdev_config_load_file( struct vdev_config* conf, char const* path );

};

#endif