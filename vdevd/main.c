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
      
      vdev_shutdown( &vdev );
      exit(1);
   }
   
   // do we need to daemonize?
   if( !vdev.config->foreground ) {
      
      // start logging 
      if( vdev.config->logfile_path == NULL ) {
         
         fprintf(stderr, "No logfile specified\n");
         
         vdev_shutdown( &vdev );
         exit(2);
      }
      
      rc = vdev_log_redirect( vdev.config->logfile_path );
      if( rc != 0 ) {
         
         vdev_error("vdev_log_redirect('%s') rc = %d\n", vdev.config->logfile_path, rc );
         
         vdev_shutdown( &vdev );
         exit(2);
      }
       
      // become a daemon
      int keep_open[2] = {
         STDOUT_FILENO,
         STDERR_FILENO
      };
      
      rc = vdev_daemonize( keep_open, 2 );
      if( rc != 0 ) {
         
         vdev_error("vdev_daemonize rc = %d\n", rc );
         vdev_shutdown( &vdev );
         
         exit(3);
      }
      
      // write a pidfile 
      if( vdev.config->pidfile_path != NULL ) {
            
         rc = vdev_pidfile_write( vdev.config->pidfile_path );
         if( rc != 0 ) {
            
            vdev_error("vdev_pidfile_write('%s') rc = %d\n", vdev.config->pidfile_path, rc );
            
            vdev_shutdown( &vdev );
            exit(4);
         }
      }
   }
   
   // do we need to connect to syslog?
   if( vdev.config->logfile_path != NULL && strcmp( vdev.config->logfile_path, "syslog" ) == 0 ) {
      
      vdev_debug("%s", "Switching to syslog for messages\n");
      vdev_enable_syslog();
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
   
   // clean up
   unlink( vdev.config->pidfile_path );

   vdev_stop( &vdev );
   vdev_shutdown( &vdev );
   
   return rc;
}

