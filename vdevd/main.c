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

#include "main.h"

// run! 
int main( int argc, char** argv ) {
   
   int rc = 0;
   pid_t pid = 0;
   struct vdev_state vdev;
   
   memset( &vdev, 0, sizeof(struct vdev_state) );
   
   // set up global vdev state
   rc = vdev_init( &vdev, argc, argv );
   if( rc != 0 ) {
      
      vdev_error("vdev_init rc = %d\n", rc );
      
      exit(1);
   }

   
   // start up
   rc = vdev_start( &vdev );
   if( rc != 0 ) {
      
      vdev_error("vdev_backend_init rc = %d\n", rc );
      
      vdev_stop( &vdev );
      vdev_shutdown( &vdev );
      exit(1);
   }
   
   // run 
   rc = vdev_main( &vdev );
   if( rc != 0 ) {
      
      vdev_error("vdev_backend_main rc = %d\n", rc );
      
   }

   vdev_stop( &vdev );
   vdev_shutdown( &vdev );
   
   return rc;
}

