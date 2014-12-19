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

#include "vdev.h"
#include "action.h"
#include "opts.h"

// global vdev initialization 
int vdev_init( struct vdev_state* vdev, struct fskit_fuse_state* fs, int argc, char** argv ) {
   
   int rc = 0;
   struct vdev_opts opts;
   
   // parse opts 
   rc = vdev_opts_parse( &opts, argc, argv );
   if( rc != 0 ) {
      
      vdev_error("vdev_opts_parse rc = %d\n", rc );
      vdev_usage( argv[0] );
      return rc;
   }
   
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
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      return rc;
   }
   
   rc = vdev_config_load( opts.config_file_path, vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_load('%s') rc = %d\n", opts.config_file_path, rc );
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      return rc;
   }
   
   vdev_debug("vdev ACLs dir:    %s\n", vdev->config->acls_dir );
   vdev_debug("vdev actions dir: %s\n", vdev->config->acts_dir );
   vdev_debug("firmware dir:     %s\n", vdev->config->firmware_dir );
   
   // load ACLs 
   rc = vdev_acl_load_all( vdev->config->acls_dir, &vdev->acls, &vdev->num_acls );
   if( rc != 0 ) {
      
      vdev_error("vdev_acl_load_all('%s') rc = %d\n", vdev->config->acls_dir, rc );
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      return rc;
   }
   
   // load actions 
   rc = vdev_action_load_all( vdev->config->acts_dir, &vdev->acts, &vdev->num_acts );
   if( rc != 0) {
      
      vdev_error("vdev_action_load_all('%s') rc = %d\n", vdev->config->acts_dir, rc );
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      return rc;
   }
   
   // initialize OS-specific state 
   vdev->os = VDEV_CALLOC( struct vdev_os_context, 1 );
   
   if( vdev->os == NULL ) {
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      return -ENOMEM;
   }
   
   rc = vdev_os_context_init( vdev->os, vdev );
   
   if( rc != 0 ) {
   
      vdev_error("vdev_os_context_init rc = %d\n", rc );
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      return rc;
   }
   
   // initialize request work queue 
   rc = fskit_wq_init( &vdev->device_wq );
   if( rc != 0 ) {
      
      vdev_error("fskit_wq_init rc = %d\n", rc );
      
      vdev_free( vdev );
      vdev_opts_free( &opts );
      return rc;
   }
   
   vdev_opts_free( &opts );
   
   vdev->argc = argc;
   vdev->argv = argv;
   
   return 0;
}

// start up vdev
// NOTE: if this fails, there's not really a way to recover.
int vdev_start( struct vdev_state* state ) {
   
   int rc = 0;
   
   if( state->running ) {
      return -EINVAL;
   }
   
   state->running = true;
   
   // start taking requests 
   rc = fskit_wq_start( &state->device_wq );
   if( rc != 0 ) {
      
      vdev_error("fskit_wq_start rc = %d\n", rc );
      return rc;
   }
   
   // start generating and listening for device events 
   rc = vdev_os_context_start( state->os );
   if( rc != 0 ) {
      
      vdev_error("vdev_os_context_start rc = %d\n", rc );
      return rc;
   }
   
   return 0;
}


// stop vdev 
// NOTE: if this fails, there's not really a way to recover
int vdev_stop( struct vdev_state* state ) {
   
   int rc = 0;
   
   if( !state->running ) {
      return -EINVAL;
   }
   
   state->running = false;
   
   // stop listening to the OS 
   rc = vdev_os_context_stop( state->os );
   if( rc != 0 ) {
      
      vdev_error("vdev_os_context_stop rc = %d\n", rc );
      return rc;
   }
   
   // stop processing requests 
   rc = fskit_wq_stop( &state->device_wq );
   if( rc != 0 ) {
      
      vdev_error("fskit_wq_stop rc = %d\n", rc );
      return rc;
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
   
   vdev_os_context_free( state->os, NULL );
   free( state->os );
   
   vdev_config_free( state->config );
   free( state->config );
   
   fskit_wq_free( &state->device_wq );
   
   return 0;
}


// stat: equvocate about which devices exist, depending on who's asking
int vdev_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb ) {
   
   vdev_debug("vdev_stat(%s)\n", grp->path );
   
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
   rc = pstat( pid, &ps, state->config->pstat_discipline );
   
   if( rc != 0 ) {
      
      vdev_error("pstat(%d) rc = %d\n", pid, rc );
      return rc;
   }
   
   // apply the ACLs on the stat buffer
   rc = vdev_acl_apply_all( state->acls, state->num_acls, grp->path, &ps, uid, gid, sb );
   if( rc < 0 ) {
      
      vdev_error("vdev_acl_apply_all(%s) rc = %d\n", grp->path, rc );
      return -EIO;
   }
   
   // omit entirely?
   if( rc == 0 ) {
      
      // filter
      return -ENOENT;
   }
   else {
      
      // accept!
      return 0;
   }
}


// readdir: equivocate about which devices exist, depending on who's asking
int vdev_readdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents ) {
   
   pid_t pid = 0;
   uid_t uid = 0;
   gid_t gid = 0;
   
   struct vdev_state* state = (struct vdev_state*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   
   pid = fskit_fuse_get_pid();
   uid = fskit_fuse_get_uid( fs_state );
   gid = fskit_fuse_get_gid( fs_state );
   
   int rc = 0;
   struct fskit_entry* child = NULL;
   
   vdev_debug("vdev_readdir(%s, %zu) from %d\n", grp->path, num_dirents, pid );
   
   // entries to omit in the listing
   vector<int> omitted_idx;
   
   for( unsigned int i = 0; i < num_dirents; i++ ) {
      
      // skip . and ..
      if( strcmp(dirents[i]->name, ".") == 0 || strcmp(dirents[i]->name, "..") == 0 ) {
         continue;
      }
      
      // find the associated fskit_entry
      child = fskit_entry_set_find_name( fent->children, dirents[i]->name );
      
      if( child == NULL ) {
         // strange, shouldn't happen...
         continue;
      }
      
      fskit_entry_rlock( child );
      
      // rewrite stat buffers
      // TODO
      
      fskit_entry_unlock( child );
   }
   
   // skip ACL'ed entries
   for( unsigned int i = 0; i < omitted_idx.size(); i++ ) {
      
      fskit_readdir_omit( dirents, omitted_idx[i] );
   }
   
   return rc;
}

