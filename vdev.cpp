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
#include "opts.h"

static char const* vdev_fuse_odev = "-odev";
static char const* vdev_fuse_allow_other = "-oallow_other";

// post-mount callback: tell the back-end to start processing
static int vdev_postmount_setup( struct fskit_fuse_state* state, void* cls ) {
   
   struct vdev_state* vdev = (struct vdev_state*)cls;
   
   // start the back-end
   int rc = vdev_frontend_send_mount_info( vdev );
   if( rc != 0 ) {
      
      fprintf(stderr, "vdev_start rc = %d\n", rc );
      return rc;
   }
   
   clearenv();
   
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
   rc = fskit_wq_init( &vdev->device_wq );
   if( rc != 0 ) {
      
      vdev_error("fskit_wq_init rc = %d\n", rc );
      
      return rc;
   }
   
   return 0;
}


// start up the back-end: ask the front-end for the mountpoint and start taking requests
int vdev_backend_start( struct vdev_state* vdev ) {
   
   int rc = 0;
   char* mountpoint = NULL;
   size_t mountpoint_len = 0;
   
   // get mountpoint len 
   rc = read( vdev->pipe_back, &mountpoint_len, sizeof(size_t) );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("read(%d) rc = %d\n", vdev->pipe_back, rc );
      return rc;
   }
   
   // get mountpoint 
   mountpoint = VDEV_CALLOC( char, mountpoint_len + 1 );
   if( mountpoint == NULL ) {
      
      return -ENOMEM;
   }
   
   rc = read( vdev->pipe_back, mountpoint, mountpoint_len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("read(%d) rc = %d\n", vdev->pipe_back, rc );
      return rc;
   }
   
   vdev->running = true;
   vdev->mountpoint = mountpoint;
   
   vdev_debug("Backend mountpoint: '%s'\n", mountpoint );
   
   // start processing requests 
   rc = fskit_wq_start( &vdev->device_wq );
   if( rc != 0 ) {
      
      vdev_error("fskit_wq_start rc = %d\n", rc );
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


// initialize the filesystem front-end 
// call after vdev_init
int vdev_frontend_init( struct vdev_state* vdev ) {
   
   int rc = 0;
   int rh = 0;
   struct fskit_core* core = NULL;
   struct fskit_fuse_state* fs = VDEV_CALLOC( struct fskit_fuse_state, 1 );
   
   if( fs == NULL ) {
      return -ENOMEM;
   }
   
   // set up fskit
   rc = fskit_fuse_init( fs, vdev );
   if( rc != 0 ) {
      
      vdev_error("fskit_fuse_init rc = %d\n", rc );
      free( fs );
      return rc;
   }
   
   // filesystem: load ACLs 
   rc = vdev_acl_load_all( vdev->config->acls_dir, &vdev->acls, &vdev->num_acls );
   if( rc != 0 ) {
      
      vdev_error("vdev_acl_load_all('%s') rc = %d\n", vdev->config->acls_dir, rc );
      
      fskit_fuse_shutdown( fs, NULL );
      free( fs );
      return rc;
   }
   
   // make sure the fs can access its methods through the VFS
   fskit_fuse_setting_enable( fs, FSKIT_FUSE_SET_FS_ACCESS );
   
   core = fskit_fuse_get_core( fs );
   
   // add handlers.
   rh = fskit_route_readdir( core, FSKIT_ROUTE_ANY, vdev_readdir, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_readdir(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      
      fskit_fuse_shutdown( fs, NULL );
      free( fs );
      
      return rh;
   }
   
   rh = fskit_route_stat( core, FSKIT_ROUTE_ANY, vdev_stat, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_stat(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      
      fskit_fuse_shutdown( fs, NULL );
      free( fs );
      
      return rh;
   }
   
   // set the root to be owned by the effective UID and GID of user
   fskit_chown( core, "/", 0, 0, geteuid(), getegid() );
   
   // call our post-mount callback to finish initializing vdev 
   fskit_fuse_postmount_callback( fs, vdev_postmount_setup, vdev );
   
   vdev->fs = fs;
   
   return 0;
}


// global vdev initialization 
int vdev_init( struct vdev_state* vdev, int argc, char** argv ) {
   
   int rc = 0;
   struct vdev_opts opts;
   int fuse_argc = 0;
   int vdev_pipe[2];
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
   
   fskit_set_debug_level( opts.debug_level );
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
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      free( fuse_argv );
      return rc;
   }
   
   rc = vdev_config_load( opts.config_file_path, vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_load('%s') rc = %d\n", opts.config_file_path, rc );
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      free( fuse_argv );
      return rc;
   }
   
   rc = pipe( vdev_pipe );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("pipe rc = %d\n", rc );
      
      vdev_free( vdev );
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
   
   vdev_opts_free( &opts );
   
   vdev->argc = argc;
   vdev->argv = argv;
   vdev->fuse_argc = fuse_argc;
   vdev->fuse_argv = fuse_argv;
   vdev->pipe_front = vdev_pipe[1];
   vdev->pipe_back = vdev_pipe[0];
   
   return 0;
}

// start up the front-end of vdev.
// this is called *after* the filesystem is mounted--FUSE will buffer and execute subsequent I/O requests.
// basically, tell the back-end that we're ready for it to start issuing requests.
int vdev_frontend_send_mount_info( struct vdev_state* state ) {
   
   int rc = 0;
   size_t mountpoint_len = strlen(state->fs->mountpoint);
   
   state->running = true;
   
   // send mountpoint length
   rc = write( state->pipe_front, &mountpoint_len, sizeof(mountpoint_len) );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("write(%d, %zu) rc = %d\n", state->pipe_front, sizeof(mountpoint_len), rc );
      return rc;
   }
   
   // send mountpoint 
   rc = write( state->pipe_front, state->fs->mountpoint, sizeof(char) * mountpoint_len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("write(%d, %zu) rc = %d\n", state->pipe_front, sizeof(char) * mountpoint_len, rc );
      return rc;
   }
   
   return 0;
}


// main loop for vdev frontend 
int vdev_frontend_main( struct vdev_state* state ) {
   
   int rc = 0;
   
   rc = fskit_fuse_main( state->fs, state->fuse_argc, state->fuse_argv );
   
   return rc;
}

// main loop for the back-end 
int vdev_backend_main( struct vdev_state* state ) {
   
   int rc = 0;
   rc = vdev_os_main( state->os );
   
   return rc;
}

// stop the front-end of vdev 
int vdev_frontend_stop( struct vdev_state* state ) {
   
   state->running = false;
   return 0;
}

// back-end: stop vdev 
// NOTE: if this fails, there's not really a way to recover
int vdev_backend_stop( struct vdev_state* state ) {
   
   int rc = 0;
   int umount_rc = 0;
   char umount_cmd[PATH_MAX + 100];
   memset( umount_cmd, 0, PATH_MAX + 100 );
   
   if( !state->running ) {
      return -EINVAL;
   }
   
   state->running = false;
   
   // stop processing requests 
   rc = fskit_wq_stop( &state->device_wq );
   if( rc != 0 ) {
      
      vdev_error("fskit_wq_stop rc = %d\n", rc );
      return rc;
   }
   
   if( state->mountpoint != NULL ) {
         
      // unmount the front-end 
      sprintf( umount_cmd, "/sbin/fusermount -u %s", state->mountpoint );
      
      vdev_debug("Unmount %s\n", state->mountpoint );
      
      rc = vdev_subprocess( umount_cmd, NULL, NULL, 0, &umount_rc );
      
      if( rc != 0 ) {
         
         vdev_error("vdev_subprocess('%s') rc = %d\n", umount_cmd, rc );
      }
      if( umount_rc != 0 ) {
         
         vdev_error("fusermount -u '%s' rc = %d\n", state->mountpoint, umount_rc );
         if( rc == 0 ) {
            rc = -abs(umount_rc);
         }
      }
      
      vdev_debug("Unmounted %s\n", state->mountpoint );
   }
   
   return rc;
}

// free up vdev 
int vdev_free( struct vdev_state* state ) {
   
   if( state->running ) {
      return -EINVAL;
   }
   
   vdev_acl_free_all( state->acls, state->num_acls );
   vdev_action_free_all( state->acts, state->num_acts );
   
   state->acls = NULL;
   state->acts = NULL;
   state->num_acls = 0;
   state->num_acts = 0;
   
   if( state->pipe_front >= 0 ) {
      close( state->pipe_front );
      state->pipe_front = -1;
   }
   
   if( state->pipe_back >= 0 ) {
      close( state->pipe_back );
      state->pipe_back = -1;
   }
   
   if( state->os != NULL ) {
      vdev_os_context_free( state->os );
      free( state->os );
      state->os = NULL;
   }
   
   if( state->config != NULL ) {
      vdev_config_free( state->config );
      free( state->config );
      state->config = NULL;
   }
   
   if( state->fs != NULL ) {
      
      fskit_fuse_shutdown( state->fs, NULL );
      free( state->fs );
      state->fs = NULL;
   }
   
   fskit_wq_free( &state->device_wq );
   
   if( state->fuse_argv != NULL ) {
      free( state->fuse_argv );
      state->fuse_argv = NULL;
      state->fuse_argc = 0;
   }
   
   if( state->mountpoint != NULL ) {
      free( state->mountpoint );
      state->mountpoint = NULL;
   }
   
   return 0;
}

// stat: equvocate about which devices exist, depending on who's asking
int vdev_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb ) {
   
   int rc = 0;
   struct pstat ps;
   pid_t pid = 0;
   uid_t uid = 0;
   gid_t gid = 0;
   struct vdev_state* state = (struct vdev_state*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   
   // stat the calling process
   pid = fskit_fuse_get_pid();
   uid = fskit_fuse_get_uid( fs_state );
   gid = fskit_fuse_get_gid( fs_state );
   
   vdev_debug("vdev_stat('%s') from user %d group %d task %d\n", grp->path, uid, gid, pid );
   
   // see who's asking 
   rc = pstat( pid, &ps, state->config->pstat_discipline );
   if( rc != 0 ) {
      
      vdev_error("pstat(%d) rc = %d\n", pid, rc );
      return -EIO;
   }
   
   // apply the ACLs on the stat buffer
   rc = vdev_acl_apply_all( state->acls, state->num_acls, grp->path, &ps, uid, gid, sb );
   if( rc < 0 ) {
      
      vdev_error("vdev_acl_apply_all(%s, uid=%d, gid=%d, pid=%d) rc = %d\n", grp->path, uid, gid, pid, rc );
      return -EIO;
   }
   
   // omit entirely?
   if( rc == 0 || (sb->st_mode & 0777) == 0 ) {
      
      // filter
      vdev_debug("Filter '%s'\n", grp->path );
      return -ENOENT;
   }
   else {
      
      // accept!
      return 0;
   }
}

// readdir: equivocate about which devices exist, depending on who's asking
int vdev_readdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents ) {
   
   int rc = 0;
   struct fskit_entry* child = NULL;
   
   // entries to omit in the listing
   vector<int> omitted_idx;
   
   pid_t pid = 0;
   uid_t uid = 0;
   gid_t gid = 0;
   
   struct vdev_state* state = (struct vdev_state*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   
   struct stat sb;
   struct pstat ps;
   char* child_path = NULL;
   
   pid = fskit_fuse_get_pid();
   uid = fskit_fuse_get_uid( fs_state );
   gid = fskit_fuse_get_gid( fs_state );
   
   vdev_debug("vdev_readdir(%s, %zu) from user %d group %d task %d\n", grp->path, num_dirents, uid, gid, pid );
   
   // see who's asking
   rc = pstat( pid, &ps, state->config->pstat_discipline );
   if( rc != 0 ) { 
      
      vdev_error("pstat(%d) rc = %d\n", pid, rc );
      return -EIO;
   }
   
   for( unsigned int i = 0; i < num_dirents; i++ ) {
      
      // skip . and ..
      if( strcmp(dirents[i]->name, ".") == 0 || strcmp(dirents[i]->name, "..") == 0 ) {
         continue;
      }
      
      // find the associated fskit_entry
      child = fskit_dir_find_by_name( fent, dirents[i]->name );
      
      if( child == NULL ) {
         // strange, shouldn't happen...
         continue;
      }
      
      fskit_entry_rlock( child );
      
      // construct a stat buffer from what we actually need 
      memset( &sb, 0, sizeof(struct stat) );
      
      sb.st_uid = child->owner;
      sb.st_gid = child->group;
      sb.st_mode = fskit_fullmode( child->type, child->mode );
      
      child_path = fskit_fullpath( grp->path, child->name, NULL );
      if( child_path == NULL ) {
         
         // can't continue; OOM
         fskit_entry_unlock( child );
         rc = -ENOMEM;
         break;
      }
      
      // filter it 
      rc = vdev_acl_apply_all( state->acls, state->num_acls, child_path, &ps, uid, gid, &sb );
      if( rc < 0 ) {
         
         vdev_error("vdev_acl_apply_all('%s', uid=%d, gid=%d, pid=%d) rc = %d\n", child_path, uid, gid, pid, rc );
         rc = -EIO;
      }
      else if( rc == 0 || (sb.st_mode & 0777) == 0 ) {
         
         // omit this one 
         vdev_debug("Filter '%s'\n", child->name );
         omitted_idx.push_back( i );
         
         rc = 0;
      }
      else {
         
         // success; matched
         rc = 0;
      }
      
      fskit_entry_unlock( child );
      
      free( child_path );
      
      // error?
      if( rc != 0 ) {
         break;
      }
   }
   
   // skip ACL'ed entries
   for( unsigned int i = 0; i < omitted_idx.size(); i++ ) {
      
      fskit_readdir_omit( dirents, omitted_idx[i] );
   }
   
   return rc;
}

