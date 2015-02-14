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

#include "vdev.h"
#include "action.h"
#include "libvdev/opts.h"

// start up the back-end
// return 0 on success 
// return -ENOMEM on OOM 
// return negative if the OS-specific back-end fails to initialize
int vdev_start( struct vdev_state* vdev ) {
   
   int rc = 0;
   
   // otherwise, it's already given
   vdev->running = true;
   
   // initialize OS-specific state, and start feeding requests
   vdev->os = VDEV_CALLOC( struct vdev_os_context, 1 );
   
   if( vdev->os == NULL ) {
      
      return -ENOMEM;
   }
   
   // start processing requests 
   rc = vdev_wq_start( &vdev->device_wq );
   if( rc != 0 ) {
      
      vdev_error("vdev_wq_start rc = %d\n", rc );
      
      free( vdev->os );
      vdev->os = NULL;
      return rc;
   }
   
   
   rc = vdev_os_context_init( vdev->os, vdev );
   
   if( rc != 0 ) {
   
      vdev_error("vdev_os_context_init rc = %d\n", rc );
      
      int wqrc = vdev_wq_stop( &vdev->device_wq, false );
      if( wqrc != 0 ) {
         
         vdev_error("vdev_wq_stop rc = %d\n", wqrc);
      }
      
      free( vdev->os );
      vdev->os = NULL;
      return rc;
   }
   
   return 0;
}


// global vdev initialization 
int vdev_init( struct vdev_state* vdev, int argc, char** argv ) {
   
   int rc = 0;
   struct vdev_opts opts;
   
   int fuse_argc = 0;
   char** fuse_argv = VDEV_CALLOC( char*, argc + 1 );
   
   if( fuse_argv == NULL ) {
      
      return -ENOMEM;
   }
   
   // parse opts 
   rc = vdev_opts_parse( &opts, argc, argv, &fuse_argc, fuse_argv );
   
   // not needed
   free( fuse_argv );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_opts_parse rc = %d\n", rc );
      
      vdev_usage( argv[0] );
      
      return rc;
   }
   
   vdev_set_debug_level( opts.debug_level );
   
   // load config 
   vdev->config = VDEV_CALLOC( struct vdev_config, 1 );
   if( vdev->config == NULL ) {
      
      vdev_opts_free( &opts );
      return -ENOMEM;
   }
   
   vdev_debug("Config file: %s\n", opts.config_file_path );
   
   rc = vdev_config_init( vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_init rc = %d\n", rc );
      
      vdev_shutdown( vdev );
      vdev_opts_free( &opts );
      return rc;
   }
   
   rc = vdev_config_load( opts.config_file_path, vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_load('%s') rc = %d\n", opts.config_file_path, rc );
      
      vdev_shutdown( vdev );
      vdev_opts_free( &opts );
      
      return rc;
   }
   
   vdev_debug("vdev actions dir: %s\n", vdev->config->acts_dir );
   vdev_debug("firmware dir:     %s\n", vdev->config->firmware_dir );
   vdev_debug("helpers dir:      %s\n", vdev->config->helpers_dir );
   
   vdev->mountpoint = vdev_strdup_or_null( opts.mountpoint );
   vdev->debug_level = opts.debug_level;
   vdev->once = opts.once;
   
   if( vdev->mountpoint == NULL ) {
      
      vdev_error("Failed to set mountpoint, opts.mountpount = '%s'\n", opts.mountpoint );
      
      vdev_shutdown( vdev );
      vdev_opts_free( &opts );
      
      return -EINVAL;
   }
   else {
      
      vdev_debug("mountpoint:       %s\n", vdev->mountpoint );
   }
   
   vdev_opts_free( &opts );
   
   vdev->argc = argc;
   vdev->argv = argv;
   
   // load actions 
   rc = vdev_action_load_all( vdev->config->acts_dir, &vdev->acts, &vdev->num_acts );
   if( rc != 0) {
      
      vdev_error("vdev_action_load_all('%s') rc = %d\n", vdev->config->acts_dir, rc );
      
      vdev_shutdown( vdev );
      return rc;
   }
   
   // initialize request work queue 
   rc = vdev_wq_init( &vdev->device_wq );
   if( rc != 0 ) {
      
      vdev_error("vdev_wq_init rc = %d\n", rc );
      
      vdev_shutdown( vdev );
      return rc;
   }
   
   
   return 0;
}

// main loop for the back-end 
int vdev_main( struct vdev_state* vdev ) {
   
   int rc = 0;
   rc = vdev_os_main( vdev->os );
   
   return rc;
}

// stop vdev 
// NOTE: if this fails, there's not really a way to recover
int vdev_stop( struct vdev_state* vdev ) {
   
   int rc = 0;
   bool wait_for_empty = false;
   
   if( !vdev->running ) {
      return -EINVAL;
   }
   
   vdev->running = false;
   wait_for_empty = vdev->once;         // wait for the queue to drain if running once
   
   // stop processing requests 
   rc = vdev_wq_stop( &vdev->device_wq, wait_for_empty );
   if( rc != 0 ) {
      
      vdev_error("vdev_wq_stop rc = %d\n", rc );
      return rc;
   }
   
   return rc;
}

// free up vdev 
int vdev_shutdown( struct vdev_state* vdev ) {
   
   if( vdev->running ) {
      return -EINVAL;
   }
   
   vdev_action_free_all( vdev->acts, vdev->num_acts );
   
   vdev->acts = NULL;
   vdev->num_acts = 0;
   
   if( vdev->os != NULL ) {
      vdev_os_context_free( vdev->os );
      free( vdev->os );
      vdev->os = NULL;
   }
   
   if( vdev->config != NULL ) {
      vdev_config_free( vdev->config );
      free( vdev->config );
      vdev->config = NULL;
   }
   
   vdev_wq_free( &vdev->device_wq );
   
   if( vdev->mountpoint != NULL ) {
      free( vdev->mountpoint );
      vdev->mountpoint = NULL;
   }
   
   return 0;
}
