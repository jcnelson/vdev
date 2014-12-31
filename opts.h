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

#ifndef _VDEV_OPTS_H_
#define _VDEV_OPTS_H_

#include "util.h"
#include <getopt.h>

struct vdev_opts {
   
   // path to config file 
   char* config_file_path;
   
   // debug level 
   int debug_level;
   
   // logfile 
   char* logfile_path;
};


extern "C" {
   
int vdev_opts_parse( struct vdev_opts* opts, int argc, char** argv, int* fuse_argc, char** fuse_argv );
int vdev_opts_free( struct vdev_opts* opts );

int vdev_usage( char const* progname );

}

#endif 