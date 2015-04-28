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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <scsi/sg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>

#include "common.h"

// get drive capabilities from an open device node, using the Linux-specific CDROM_GET_CAPABILITY ioctl.
// return 0 on success
// return negative errno on error
static int stat_optical_get_caps( int fd ) {
   
   int caps = 0;
   int rc = 0;
   
   // get the cpability 
   caps = ioctl( fd, CDROM_GET_CAPABILITY, NULL );
   if( caps < 0 ) {
      
      rc = -errno;
      fprintf(stderr, "CDROM_GET_CAPABILITY ioctl failed, rc = %d\n", rc );
      return rc;
   }
   
   return caps;
}


// print out optical capabilities as environment variables 
static int stat_optical_print_caps( int capabilities ) {
   
   vdev_property_add( "VDEV_OPTICAL_CD_R", (capabilities & CDC_CD_R) == 0 ? "0" : "1" );
   vdev_property_add( "VDEV_OPTICAL_CD_RW", (capabilities & CDC_CD_R) == 0 ? "0" : "1" );
   vdev_property_add( "VDEV_OPTICAL_DVD", (capabilities & CDC_DVD) == 0 ? "0" : "1" );
   vdev_property_add( "VDEV_OPTICAL_DVD_R", (capabilities & CDC_DVD_R) == 0 ? "0" : "1" );
   vdev_property_add( "VDEV_OPTICAL_DVD_RAM", (capabilities & CDC_DVD_RAM) == 0 ? "0" : "1" );
   
   vdev_property_print();
   vdev_property_free_all();
   
   return 0;
}

// usage statement
static int usage( char const* prog_name ) {
   
   fprintf(stderr, "Usage: %s /path/to/optical/device\n", prog_name );
   return 0;
}

// entry point 
int main( int argc, char** argv ) {
   
   int rc = 0;
   int fd = 0;
   int capabilities = 0;
   
   if( argc != 2 ) {
      
      usage( argv[0] );
      exit(1);
   }
   
   // get the device node 
   fd = open( argv[1], O_RDONLY | O_NONBLOCK );
   if( fd < 0 ) {
      
      rc = -errno;
      fprintf(stderr, "open('%s') rc = %d\n", argv[1], rc );
      exit(2);
   }
   
   capabilities = stat_optical_get_caps( fd );
   
   close( fd );
   
   if( capabilities < 0 ) {
      
      fprintf(stderr, "stat_optical_get_caps('%s') rc = %d\n", argv[1], rc );
      exit(4);
   }
   
   stat_optical_print_caps( capabilities );
   return 0;
}
