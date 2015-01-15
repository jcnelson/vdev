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

#ifdef _USE_FS

#include "fs.h"
#include "vdev.h"
#include "acl.h"

// post-mount callback: tell the back-end to start processing
static int vdev_postmount_setup( struct fskit_fuse_state* state, void* cls ) {
   
   struct vdev_state* vdev = (struct vdev_state*)cls;
   
   vdev->fs_frontend->running = true;
   
   // start the back-end
   int rc = vdev_send_mount_info( vdev->fs_frontend->pipe_front, vdev->fs_frontend->fs->mountpoint );
   if( rc != 0 ) {
      
      fprintf(stderr, "vdev_start rc = %d\n", rc );
      return rc;
   }
   
   clearenv();
   
   return 0;
}

// initialize the filesystem front-end 
// call after vdev_init
int vdev_frontend_init( struct vdev_fs* fs_frontend, struct vdev_state* vdev ) {
   
   int rc = 0;
   int rh = 0;
   int vdev_pipe[2];
   struct fskit_core* core = NULL;
   struct fskit_fuse_state* fs = VDEV_CALLOC( struct fskit_fuse_state, 1 );
   
   if( fs == NULL ) {
      return -ENOMEM;
   }
   
   fskit_set_debug_level( vdev->debug_level );
   
   // set up fskit
   rc = fskit_fuse_init( fs, vdev );
   if( rc != 0 ) {
      
      vdev_error("fskit_fuse_init rc = %d\n", rc );
      free( fs );
      return rc;
   }
   
   // filesystem: load ACLs 
   rc = vdev_acl_load_all( vdev->config->acls_dir, &fs_frontend->acls, &fs_frontend->num_acls );
   if( rc != 0 ) {
      
      vdev_error("vdev_acl_load_all('%s') rc = %d\n", vdev->config->acls_dir, rc );
      
      fskit_fuse_shutdown( fs, NULL );
      free( fs );
      return rc;
   }
   
   // set up the pipe
   rc = pipe( vdev_pipe );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("pipe rc = %d\n", rc );
      
      fskit_fuse_shutdown( fs, NULL );
      free( fs );
      
      vdev_acl_free_all( fs_frontend->acls, fs_frontend->num_acls );
      
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
   
   fs_frontend->fs = fs;
   fs_frontend->pipe_front = vdev_pipe[1];
   fs_frontend->pipe_back = vdev_pipe[0];
   
   vdev->fs_frontend = fs_frontend;
   
   return 0;
}


// main loop for vdev frontend 
int vdev_frontend_main( struct vdev_fs* fs_frontend, int fuse_argc, char** fuse_argv ) {
   
   int rc = 0;
   
   rc = fskit_fuse_main( fs_frontend->fs, fuse_argc, fuse_argv );
   
   return rc;
}


// stop the front-end of vdev 
int vdev_frontend_stop( struct vdev_fs* fs_frontend ) {
   
   fs_frontend->running = false;
   return 0;
}


// shut down the front-end 
int vdev_frontend_shutdown( struct vdev_fs* fs_frontend ) {
   
   if( fs_frontend->running ) {
      return -EINVAL;
   }
   
   vdev_acl_free_all( fs_frontend->acls, fs_frontend->num_acls );
   
   if( fs_frontend->pipe_front >= 0 ) {
      close( fs_frontend->pipe_front );
      fs_frontend->pipe_front = -1;
   }
   
   if( fs_frontend->pipe_back >= 0 ) {
      close( fs_frontend->pipe_back );
      fs_frontend->pipe_back = -1;
   }
   
   if( fs_frontend->fs != NULL ) {
      
      fskit_fuse_shutdown( fs_frontend->fs, NULL );
      free( fs_frontend->fs );
      fs_frontend->fs = NULL;
   }
   
   memset( fs_frontend, 0, sizeof(struct vdev_fs) );
   
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
   rc = vdev_acl_apply_all( state->config, state->fs_frontend->acls, state->fs_frontend->num_acls, grp->path, &ps, uid, gid, sb );
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
      rc = vdev_acl_apply_all( state->config, state->fs_frontend->acls, state->fs_frontend->num_acls, child_path, &ps, uid, gid, &sb );
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

#endif  // _USE_FS