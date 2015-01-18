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
#include "fs.h"
#include "action.h"
#include "opts.h"

static char const* vdev_fuse_odev = "-odev";
static char const* vdev_fuse_allow_other = "-oallow_other";


// send the back-end the FUSE mountpoint
// this is called *after* the filesystem is mounted--FUSE will buffer and execute subsequent I/O requests.
// basically, tell the back-end that we're ready for it to start issuing requests.
int vdev_send_mount_info( int pipe_front, char const* mountpoint ) {
   
   int rc = 0;
   size_t mountpoint_len = strlen(mountpoint);
   
   // send mountpoint length
   rc = write( pipe_front, &mountpoint_len, sizeof(mountpoint_len) );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("write(%d, %zu) rc = %d\n", pipe_front, sizeof(mountpoint_len), rc );
      return rc;
   }
   
   // send mountpoint 
   rc = write( pipe_front, mountpoint, sizeof(char) * mountpoint_len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("write(%d, %zu) rc = %d\n", pipe_front, sizeof(char) * mountpoint_len, rc );
      return rc;
   }
   
   return 0;
}


// receive the mountpoint from the front-end 
int vdev_recv_mount_info( int pipe_back, char** ret_mountpoint ) {
   
   char* mountpoint = NULL;
   size_t mountpoint_len = 0;
   int rc = 0;
   
   // get mountpoint len 
   rc = read( pipe_back, &mountpoint_len, sizeof(size_t) );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("read(%d) rc = %d\n", pipe_back, rc );
      return rc;
   }
   
   // get mountpoint 
   mountpoint = VDEV_CALLOC( char, mountpoint_len + 1 );
   if( mountpoint == NULL ) {
      
      return -ENOMEM;
   }
   
   rc = read( pipe_back, mountpoint, mountpoint_len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("read(%d) rc = %d\n", pipe_back, rc );
      return rc;
   }
   
   *ret_mountpoint = mountpoint;
   
   return 0;
}


// initialize the back-end link to the OS.
// call after vdev_init
int vdev_backend_init( struct vdev_state* vdev ) {
   
   int rc = 0;
   
   // load actions 
   rc = vdev_action_load_all( vdev->config->acts_dir, &vdev->acts, &vdev->num_acts );
   if( rc != 0) {
      
      vdev_error("vdev_action_load_all('%s') rc = %d\n", vdev->config->acts_dir, rc );
      
      return rc;
   }
   
   // initialize request work queue 
   rc = vdev_wq_init( &vdev->device_wq );
   if( rc != 0 ) {
      
      vdev_error("vdev_wq_init rc = %d\n", rc );
      
      return rc;
   }
   
   return 0;
}


// start up the back-end: ask the front-end for the mountpoint and start taking requests
int vdev_backend_start( struct vdev_state* vdev ) {
   
   int rc = 0;
   char* mountpoint = NULL;
   
   if( vdev->fs_frontend != NULL ) {
      
      // mountpoint is specified by 
      rc = vdev_recv_mount_info( vdev->fs_frontend->pipe_back, &mountpoint );
      
      if( rc != 0 ) {
         
         vdev_error("vdev_recv_mount_info(%d) rc = %d\n", vdev->fs_frontend->pipe_back, rc );
         return rc;
      }
   }
   
   // otherwise, it's already given
   vdev->running = true;
   
   if( vdev->mountpoint != NULL && mountpoint != NULL ) {
      
      free( vdev->mountpoint );
      vdev->mountpoint = mountpoint;
   }
   
   vdev_debug("Mountpoint: '%s'\n", mountpoint );
   
   // start processing requests 
   rc = vdev_wq_start( &vdev->device_wq );
   if( rc != 0 ) {
      
      vdev_error("vdev_wq_start rc = %d\n", rc );
      return rc;
   }
   
   // initialize OS-specific state, and start feeding requests
   vdev->os = VDEV_CALLOC( struct vdev_os_context, 1 );
   
   if( vdev->os == NULL ) {
      
      return -ENOMEM;
   }
   
   rc = vdev_os_context_init( vdev->os, vdev );
   
   if( rc != 0 ) {
   
      vdev_error("vdev_os_context_init rc = %d\n", rc );
      
      return rc;
   }
   
   return 0;
}


// global vdev initialization 
int vdev_init( struct vdev_state* vdev, int argc, char** argv ) {
   
   int rc = 0;
   struct vdev_opts opts;
   int fuse_argc = 0;
   char** fuse_argv = VDEV_CALLOC( char*, argc + 4 );
   
   if( fuse_argv == NULL ) {
      return -ENOMEM;
   }
   
   // parse opts 
   rc = vdev_opts_parse( &opts, argc, argv, &fuse_argc, fuse_argv );
   if( rc != 0 ) {
      
      vdev_error("vdev_opts_parse rc = %d\n", rc );
      
      vdev_usage( argv[0] );
      
      free( fuse_argv );
      return rc;
   }
   
   vdev_set_debug_level( opts.debug_level );
   
   // load config 
   vdev->config = VDEV_CALLOC( struct vdev_config, 1 );
   if( vdev->config == NULL ) {
      
      vdev_opts_free( &opts );
      
      free( fuse_argv );
      return -ENOMEM;
   }
   
   vdev_debug("Config file: %s\n", opts.config_file_path );
   
   rc = vdev_config_init( vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_init rc = %d\n", rc );
      
      vdev_shutdown( vdev );
      vdev_opts_free( &opts );
      free( fuse_argv );
      return rc;
   }
   
   rc = vdev_config_load( opts.config_file_path, vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_load('%s') rc = %d\n", opts.config_file_path, rc );
      
      vdev_shutdown( vdev );
      vdev_opts_free( &opts );
      free( fuse_argv );
      return rc;
   }
   
   vdev_debug("vdev ACLs dir:    %s\n", vdev->config->acls_dir );
   vdev_debug("vdev actions dir: %s\n", vdev->config->acts_dir );
   vdev_debug("firmware dir:     %s\n", vdev->config->firmware_dir );
   vdev_debug("helpers dir:      %s\n", vdev->config->helpers_dir );
   
   // force -odev, since we'll create device nodes 
   fuse_argv[fuse_argc] = (char*)vdev_fuse_odev;
   fuse_argc++;
   
   // force -oallow_other, since we'll want to expose this to everyone 
   fuse_argv[fuse_argc] = (char*)vdev_fuse_allow_other;
   fuse_argc++;
   
   vdev->mountpoint = vdev_strdup_or_null( opts.mountpoint );
   vdev->debug_level = opts.debug_level;
   
   if( vdev->mountpoint == NULL ) {
      
      vdev_error("Failed to set mountpoint, opts.mountpount = '%s'\n", opts.mountpoint );
      
      vdev_shutdown( vdev );
      vdev_opts_free( &opts );
      free( fuse_argv );
      
      return -EINVAL;
   }
   else {
      
      vdev_debug("mountpoint:       %s\n", vdev->mountpoint );
   }
   
   vdev_opts_free( &opts );
   
   vdev->argc = argc;
   vdev->argv = argv;
   vdev->fuse_argc = fuse_argc;
   vdev->fuse_argv = fuse_argv;
   
   return 0;
}

// main loop for the back-end 
int vdev_backend_main( struct vdev_state* vdev ) {
   
   int rc = 0;
   rc = vdev_os_main( vdev->os );
   
   return rc;
}

// back-end: stop vdev 
// NOTE: if this fails, there's not really a way to recover
int vdev_backend_stop( struct vdev_state* vdev ) {
   
   int rc = 0;
   int umount_rc = 0;
   char umount_cmd[PATH_MAX + 100];
   memset( umount_cmd, 0, PATH_MAX + 100 );
   
   if( !vdev->running ) {
      return -EINVAL;
   }
   
   vdev->running = false;
   
   // stop processing requests 
   rc = vdev_wq_stop( &vdev->device_wq );
   if( rc != 0 ) {
      
      vdev_error("vdev_wq_stop rc = %d\n", rc );
      return rc;
   }
   
   if( vdev->mountpoint != NULL && vdev->fs_frontend != NULL ) {
         
      // unmount the front-end 
      sprintf( umount_cmd, "/sbin/fusermount -u %s", vdev->mountpoint );
      
      vdev_debug("Unmount %s\n", vdev->mountpoint );
      
      rc = vdev_subprocess( umount_cmd, NULL, NULL, 0, &umount_rc );
      
      if( rc != 0 ) {
         
         vdev_error("vdev_subprocess('%s') rc = %d\n", umount_cmd, rc );
      }
      if( umount_rc != 0 ) {
         
         vdev_error("fusermount -u '%s' rc = %d\n", vdev->mountpoint, umount_rc );
         if( rc == 0 ) {
            rc = -abs(umount_rc);
         }
      }
      
      vdev_debug("Unmounted %s\n", vdev->mountpoint );
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
   
   if( vdev->fuse_argv != NULL ) {
      free( vdev->fuse_argv );
      vdev->fuse_argv = NULL;
      vdev->fuse_argc = 0;
   }
   
   if( vdev->mountpoint != NULL ) {
      free( vdev->mountpoint );
      vdev->mountpoint = NULL;
   }
   
   return 0;
}
