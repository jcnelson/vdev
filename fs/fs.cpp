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


#include "fs.h"
#include "acl.h"

static char const* vdev_fuse_odev = "-odev";
static char const* vdev_fuse_allow_other = "-oallow_other";

// get the mountpoint option, by parsing the FUSE command line 
static int vdev_get_mountpoint( int fuse_argc, char** fuse_argv, char** ret_mountpoint ) {
   
   struct fuse_args fargs = FUSE_ARGS_INIT(fuse_argc, fuse_argv);
   char* mountpoint = NULL;
   int unused_1;
   int unused_2;
   int rc = 0;
   
   // parse command-line...
   rc = fuse_parse_cmdline( &fargs, &mountpoint, &unused_1, &unused_2 );
   if( rc < 0 ) {

      fskit_error("fuse_parse_cmdline rc = %d\n", rc );
      fuse_opt_free_args(&fargs);

      return rc;
   }
   
   else {
      
      if( mountpoint != NULL ) {
         
         *ret_mountpoint = strdup( mountpoint );
         free( mountpoint );
         
         rc = 0;
      }
      else {
         rc = -ENOMEM;
      }
      
      fuse_opt_free_args(&fargs);
   }
   
   return 0;
}     

// initialize the filesystem front-end 
// call after vdev_init
int vdevfs_init( struct vdevfs* vdev, int argc, char** argv ) {
   
   int rc = 0;
   int rh = 0;
   struct fskit_core* core = NULL;
   int fuse_argc = 0;
   char** fuse_argv = NULL;
   
   struct fskit_fuse_state* fs = VDEV_CALLOC( struct fskit_fuse_state, 1 );
   
   if( fs == NULL ) {
      return -ENOMEM;
   }
   
   fuse_argv = VDEV_CALLOC( char*, argc + 4 );
   
   if( fuse_argv == NULL ) {
      
      free( fs );
      return -ENOMEM;
   }
   
   // load config 
   vdev->config = VDEV_CALLOC( struct vdev_config, 1 );
   if( vdev->config == NULL ) {
      
      free( fs );
      free( fuse_argv );
      return -ENOMEM;
   }
   
   // init config 
   rc = vdev_config_init( vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_init rc = %d\n", rc );
      
      vdevfs_shutdown( vdev );
      free( fs );
      free( fuse_argv );
      return rc;
   }
   
   // parse opts 
   rc = vdev_config_load_from_args( vdev->config, argc, argv, &fuse_argc, fuse_argv );
   if( rc != 0 ) {
      
      vdev_error("vdev_opts_parse rc = %d\n", rc );
      
      vdev_config_usage( argv[0] );
      
      free( fs );
      free( fuse_argv );
      vdevfs_shutdown( vdev );
      return rc;
   }
   
   // get the mountpoint, but from FUSE 
   if( vdev->config->mountpoint != NULL ) {
      free( vdev->config->mountpoint );
   }
   
   rc = vdev_get_mountpoint( fuse_argc, fuse_argv, &vdev->config->mountpoint );
   if( rc != 0 ) {
      
      vdev_error("vdev_get_mountpoint rc = %d\n", rc );
      
      vdev_config_usage( argv[0] );
      
      free( fs );
      free( fuse_argv );
      return rc;
   }
   
   vdev_set_debug_level( vdev->config->debug_level );
   
   
   vdev_debug("Config file: %s\n", vdev->config->config_path );
   
   rc = vdev_config_load( vdev->config->config_path, vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_load('%s') rc = %d\n", vdev->config->config_path, rc );
      
      vdevfs_shutdown( vdev );
      free( fs );
      free( fuse_argv );
      return rc;
   }
   
   vdev_debug("vdev ACLs dir:    %s\n", vdev->config->acls_dir );
   
   // force -odev, since we'll create device nodes 
   fuse_argv[fuse_argc] = (char*)vdev_fuse_odev;
   fuse_argc++;
   
   // force -oallow_other, since we'll want to expose this to everyone 
   fuse_argv[fuse_argc] = (char*)vdev_fuse_allow_other;
   fuse_argc++;
   
   vdev->mountpoint = vdev_strdup_or_null( vdev->config->mountpoint );
   vdev->debug_level = vdev->config->debug_level;
   
   if( vdev->mountpoint == NULL ) {
      
      vdev_error("Failed to set mountpoint, config.mountpount = '%s'\n", vdev->config->mountpoint );
      
      vdevfs_shutdown( vdev );
      free( fuse_argv );
      free( fs );
      return -EINVAL;
   }
   else {
      
      vdev_debug("mountpoint:       %s\n", vdev->mountpoint );
   }
   
   vdev->argc = argc;
   vdev->argv = argv;
   vdev->fuse_argc = fuse_argc;
   vdev->fuse_argv = fuse_argv;
   
   fskit_set_debug_level( vdev->debug_level );
   
   // set up fskit
   rc = fskit_fuse_init( fs, vdev );
   if( rc != 0 ) {
      
      vdev_error("fskit_fuse_init rc = %d\n", rc );
      free( fs );
      vdevfs_shutdown( vdev );
      return rc;
   }
   
   // load ACLs 
   rc = vdev_acl_load_all( vdev->config->acls_dir, &vdev->acls, &vdev->num_acls );
   if( rc != 0 ) {
      
      vdev_error("vdev_acl_load_all('%s') rc = %d\n", vdev->config->acls_dir, rc );
      
      fskit_fuse_shutdown( fs, NULL );
      free( fs );
      vdevfs_shutdown( vdev );
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
      vdevfs_shutdown( vdev );
      return rh;
   }
   
   rh = fskit_route_stat( core, FSKIT_ROUTE_ANY, vdev_stat, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_stat(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      
      fskit_fuse_shutdown( fs, NULL );
      free( fs );
      vdevfs_shutdown( vdev );
      return rh;
   }
   
   vdev->fs = fs;
   
   // set the root to be owned by the effective UID and GID of user
   fskit_chown( core, "/", 0, 0, geteuid(), getegid() );
   
   return 0;
}


// main loop for vdev frontend 
int vdevfs_main( struct vdevfs* vdev, int fuse_argc, char** fuse_argv ) {
   
   int rc = 0;
   
   rc = fskit_fuse_main( vdev->fs, fuse_argc, fuse_argv );
   
   return rc;
}


// shut down the front-end 
int vdevfs_shutdown( struct vdevfs* vdev ) {
   
   if( vdev->acls != NULL ) {
      vdev_acl_free_all( vdev->acls, vdev->num_acls );
   }
   
   if( vdev->config != NULL ) {
      vdev_config_free( vdev->config );
      free( vdev->config );
      vdev->config = NULL;
   }
   
   if( vdev->fs != NULL ) {
      
      fskit_fuse_shutdown( vdev->fs, NULL );
      free( vdev->fs );
      vdev->fs = NULL;
   }
   
   if( vdev->mountpoint != NULL ) {
      
      free( vdev->mountpoint );
      vdev->mountpoint = NULL;
   }
   
   if( vdev->fuse_argv != NULL ) {
      
      free( vdev->fuse_argv );
      vdev->fuse_argc = 0;
      vdev->fuse_argv  = NULL;
   }
   
   memset( vdev, 0, sizeof(struct vdevfs) );
   
   return 0;
}


// stat: equvocate about which devices exist, depending on who's asking
int vdev_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb ) {
   
   int rc = 0;
   struct pstat ps;
   pid_t pid = 0;
   uid_t uid = 0;
   gid_t gid = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   
   // stat the calling process
   pid = fskit_fuse_get_pid();
   uid = fskit_fuse_get_uid( fs_state );
   gid = fskit_fuse_get_gid( fs_state );
   
   vdev_debug("vdev_stat('%s') from user %d group %d task %d\n", grp->path, uid, gid, pid );
   
   // see who's asking 
   rc = pstat( pid, &ps, 0 );
   if( rc != 0 ) {
      
      vdev_error("pstat(%d) rc = %d\n", pid, rc );
      return -EIO;
   }
   
   // apply the ACLs on the stat buffer
   rc = vdev_acl_apply_all( vdev->config, vdev->acls, vdev->num_acls, grp->path, &ps, uid, gid, sb );
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
   
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   
   struct stat sb;
   struct pstat ps;
   char* child_path = NULL;
   
   pid = fskit_fuse_get_pid();
   uid = fskit_fuse_get_uid( fs_state );
   gid = fskit_fuse_get_gid( fs_state );
   
   vdev_debug("vdev_readdir(%s, %zu) from user %d group %d task %d\n", grp->path, num_dirents, uid, gid, pid );
   
   // see who's asking
   rc = pstat( pid, &ps, 0 );
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
      rc = vdev_acl_apply_all( vdev->config, vdev->acls, vdev->num_acls, child_path, &ps, uid, gid, &sb );
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
