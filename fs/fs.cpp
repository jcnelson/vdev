/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 or later as 
   published by the Free Software Foundation. For the terms of this 
   license, see LICENSE.GPLv3+ or <http://www.gnu.org/licenses/>.

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
static char const* vdev_fuse_ononempty = "-ononempty";
static char const* vdev_fuse_allow_other = "-oallow_other";

// scanning context, for vdevfs_dev_import 
struct vdevfs_scandirat_context {

   struct fskit_core* core;          // fskit core context
   struct fskit_entry* parent_dir;   // corresponding fskit directory being scanned
   char* parent_path;
   
   queue<int>* dir_queue;            // points to dirfds
   queue<char*>* dir_paths;          // points to dirpaths
};


// set up a vdevfs_scandirat_context 
// always succeeds
static void vdevfs_scandirat_context_init( struct vdevfs_scandirat_context* ctx, struct fskit_core* core, struct fskit_entry* parent_dir, char* parent_path, queue<int>* dir_queue, queue<char*>* dir_paths ) {
   
   ctx->core = core;
   ctx->parent_dir = parent_dir;
   ctx->parent_path = parent_path;
   ctx->dir_queue = dir_queue;
   ctx->dir_paths = dir_paths;
}


// callback to be fed int vdev_load_all_at.
// builds up the children listing of vdevfs_scandirat_context.parent_dir.
// if we find a directory, open it and enqueue its file descriptor to dir_queue.
// return 0 on success
// return -ENOMEM on OOM
static int vdevfs_scandirat_context_callback( int dirfd, struct dirent* dent, void* cls ) {
   
   struct vdevfs_scandirat_context* ctx = (struct vdevfs_scandirat_context*)cls;
   
   int rc = 0;
   int fd = 0;
   struct stat sb;
   struct fskit_entry* child;
   char linkbuf[8193];            // for resolving an underlying symlink
   char const* method_name;       // for logging
   char* joined_path = NULL;
   
   // skip . and ..
   if( strcmp( dent->d_name, "." ) == 0 || strcmp( dent->d_name, ".." ) == 0 ) {
      return 0;
   }
   
   // learn more...
   rc = fstatat( dirfd, dent->d_name, &sb, AT_SYMLINK_NOFOLLOW );
   if( rc != 0 ) {
      
      rc = -errno;
      
      // mask errors; just log the serious ones
      if( rc != -ENOENT && rc != -EACCES ) {
         
         vdev_error("fstatat(%d, '%s') rc = %d\n", dirfd, dent->d_name, rc );
      }
      
      return 0;
   }
   
   // directory?  get an fd to it if so, so we can keep scanning
   if( S_ISDIR( sb.st_mode ) ) {
      
      // try to get at it 
      fd = openat( dirfd, dent->d_name, O_RDONLY );
      if( fd < 0 ) {
         
         rc = -errno;
         
         // mask errors; just log the serious ones 
         if( rc != -ENOENT && rc != -EACCES ) {
            
            vdev_error("openat(%d, '%s') rc = %d\n", dirfd, dent->d_name, rc );
         }
         
         return 0;
      }
      
      // woo! save it 
      joined_path = vdev_fullpath( ctx->parent_path, dent->d_name, NULL );
      if( joined_path == NULL ) {
         
         close( fd );
         return -ENOMEM;
      }
      
      try {
         
         ctx->dir_paths->push( joined_path );
      }
      catch( bad_alloc& ba ) {
         
         // OOM
         free( joined_path );
         return -ENOMEM;
      }
      
      try {
         
         ctx->dir_queue->push( fd );
      }
      catch( bad_alloc& ba ) {
         
         // OOM 
         close( fd );
         return -ENOMEM;
      }
   }
   
   // construct an inode for this entry 
   child = VDEV_CALLOC( struct fskit_entry, 1 );
   if( child == NULL ) {
      
      return -ENOMEM;
   }
   
   // regular file?
   if( S_ISREG( sb.st_mode ) ) {
      
      method_name = "fskit_entry_init_file";
      rc = fskit_entry_init_file( child, sb.st_ino, dent->d_name, sb.st_uid, sb.st_gid, sb.st_mode & 0777 );
   }
   
   // directory?
   else if( S_ISDIR( sb.st_mode ) ) {
      
      method_name = "fskit_entry_init_dir";
      rc = fskit_entry_init_dir( child, ctx->parent_dir, sb.st_ino, dent->d_name, sb.st_uid, sb.st_gid, sb.st_mode & 0777 );
   }
   
   // named pipe?
   else if( S_ISFIFO( sb.st_mode ) ) {
      
      method_name = "fskit_entry_init_fifo";
      rc = fskit_entry_init_fifo( child, sb.st_ino, dent->d_name, sb.st_uid, sb.st_gid, sb.st_mode & 0777 );
   }
   
   // unix domain socket?
   else if( S_ISSOCK( sb.st_mode ) ) {
      
      method_name = "fskit_entry_init_sock";
      rc = fskit_entry_init_sock( child, sb.st_ino, dent->d_name, sb.st_uid, sb.st_gid, sb.st_mode & 0777 );
   }
   
   // character device?
   else if( S_ISCHR( sb.st_mode ) ) {
      
      method_name = "fskit_entry_init_chr";
      rc = fskit_entry_init_chr( child, sb.st_ino, dent->d_name, sb.st_uid, sb.st_gid, sb.st_mode, sb.st_rdev );
   }
   
   // block device?
   else if( S_ISBLK( sb.st_mode ) ) {
      
      method_name = "fskit_entry_init_blk";
      rc = fskit_entry_init_blk( child, sb.st_ino, dent->d_name, sb.st_uid, sb.st_gid, sb.st_mode, sb.st_rdev );
   }
   
   // symbolic link?
   else if( S_ISLNK( sb.st_mode ) ) {
      
      // read the link first...
      memset( linkbuf, 0, 8193 );
      
      rc = readlinkat( dirfd, dent->d_name, linkbuf, 8192 );
      if( rc < 0 ) {
         
         rc = -errno;
         
         // mask error, but log serious ones.  this link will not appear in the listing 
         if( rc != -ENOENT && rc != -EACCES ) {
            
            vdev_error("readlinkat(%d, '%s') rc = %d\n", dirfd, dent->d_name, rc );
         }
         
         free( child );
         child = NULL;
         
         return 0;
      }
      
      method_name = "fskit_entry_init_symlink";
      rc = fskit_entry_init_symlink( child, sb.st_ino, dent->d_name, linkbuf );
   }
   
   // success?
   if( rc != 0 ) {
      
      vdev_error("%s( on %d, '%s' ) rc = %d\n", method_name, dirfd, dent->d_name, rc );
      
      free( child );
      child = NULL;
      
      return rc;
   }
   
   // insert into parent 
   rc = fskit_entry_attach_lowlevel( ctx->parent_dir, child );
   if( rc != 0 ) {
      
      // OOM 
      fskit_entry_destroy( ctx->core, child, false );
      
      free( child );
      child = NULL;
      
      return rc;
   }
   
   // success!
   return rc;
}


// load the filesystem with metadata from under the mountpoint
// return 0 on success 
// return -ENOMEM on OOM
static int vdevfs_dev_import( struct fskit_fuse_state* fs, void* arg ) {
   
   struct vdevfs* vdev = (struct vdevfs*)arg;
   int rc = 0;
   queue<int> dirfds;           // directory descriptors
   queue<char*> dirpaths;       // relative paths to the mountpoint 
   struct fskit_entry* dir_ent = NULL;
   
   struct vdevfs_scandirat_context scan_context;
   
   char* root = vdev_strdup_or_null("/");
   if( root == NULL ) {
      return -ENOMEM;
   }
   
   int root_fd = dup( vdev->mountpoint_dirfd );
   if( root_fd < 0 ) {
      
      vdev_error("dup(%d) rc = %d\n", vdev->mountpoint_dirfd, rc );
      rc = -errno;
      free( root );
      return rc;
   }
   
   // start at the mountpoint
   try {
      
      dirfds.push( root_fd );
      dirpaths.push( root );
   }
   catch( bad_alloc& ba ) {
      
      free( root );
      return -ENOMEM;
   }
   
   while( dirfds.size() > 0 ) {
      
      // next directory 
      int dirfd = dirfds.front();
      char* dirpath = dirpaths.front();
      
      dirfds.pop();
      dirpaths.pop();
      
      // look up this entry 
      dir_ent = fskit_entry_resolve_path( fskit_fuse_get_core( vdev->fs ), dirpath, 0, 0, true, &rc );
      if( rc != 0 ) {
         
         // shouldn't happen--we're going breadth-first 
         vdev_error("fskit_entry_resolve_path('%s') rc = %d\n", dirpath, rc );
         
         close( dirfd );
         free( dirpath );
         dirpath = NULL;
        
         break;
      }
      
      // make a scan context 
      vdevfs_scandirat_context_init( &scan_context, fskit_fuse_get_core( vdev->fs ), dir_ent, dirpath, &dirfds, &dirpaths );
      
      // scan this directory 
      rc = vdev_load_all_at( dirfd, vdevfs_scandirat_context_callback, &scan_context );
      
      fskit_entry_unlock( dir_ent );
      
      if( rc != 0 ) {
         
         // failed
         vdev_error("vdev_load_all_at(%d, '%s') rc = %d\n", dirfd, dirpath, rc );
      }
      
      close( dirfd );
      free( dirpath );
      dirpath = NULL;
      
      if( rc != 0 ) {
         break;
      }
   }
   
   // free any remaining directory state 
   size_t num_dirfds = dirfds.size();
   for( unsigned int i = 0; i < num_dirfds; i++ ) {
      
      int next_dirfd = dirfds.front();
      dirfds.pop();
      
      close( next_dirfd );
   }
   
   size_t num_dirpaths = dirpaths.size();
   for( unsigned int i = 0; i < num_dirpaths; i++ ) {
      
      char* path = dirpaths.front();
      dirpaths.pop();
      
      free( path );
   }
   
   return rc;
}


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

      vdev_error("fuse_parse_cmdline rc = %d\n", rc );
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
// return 0 on success
// return -ENOMEM on OOM
// return negative on error
int vdevfs_init( struct vdevfs* vdev, int argc, char** argv ) {
   
   int rc = 0;
   int rh = 0;
   struct fskit_core* core = NULL;
   int fuse_argc = 0;
   char** fuse_argv = NULL;
   int dirfd = 0;
   
   // library setup 
   vdev_setup_global();
   
   struct fskit_fuse_state* fs = VDEV_CALLOC( struct fskit_fuse_state, 1 );
   
   if( fs == NULL ) {
      return -ENOMEM;
   }
   
   fuse_argv = VDEV_CALLOC( char*, argc + 5 );
   
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
   vdev_set_error_level( vdev->config->error_level );
   
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
   
   // force -ononempty, since we'll want to import the underlying filesystem
   fuse_argv[fuse_argc] = (char*)vdev_fuse_ononempty;
   fuse_argc++;
   
   vdev->mountpoint = vdev_strdup_or_null( vdev->config->mountpoint );
   
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
   
   fskit_set_debug_level( vdev->config->debug_level );
   fskit_set_error_level( vdev->config->error_level );
   
   // get mountpoint directory 
   dirfd = open( vdev->mountpoint, O_DIRECTORY );
   if( dirfd < 0 ) {
      
      rc = -errno;
      vdev_error("open('%s') rc = %d\n", vdev->mountpoint, rc );
      
      free( fs );
      vdevfs_shutdown( vdev );
      return rc;
   }
   
   vdev->mountpoint_dirfd = dirfd;
   
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
   rh = fskit_route_readdir( core, FSKIT_ROUTE_ANY, vdevfs_readdir, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_readdir(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_stat( core, FSKIT_ROUTE_ANY, vdevfs_stat, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_stat(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_mknod( core, FSKIT_ROUTE_ANY, vdevfs_mknod, FSKIT_CONCURRENT );
   if( rc < 0 ) {
      
      vdev_error("fskit_route_mknod(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_mkdir( core, FSKIT_ROUTE_ANY, vdevfs_mkdir, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_mkdir(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_create( core, FSKIT_ROUTE_ANY, vdevfs_create, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_create(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_open( core, FSKIT_ROUTE_ANY, vdevfs_open, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_open(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_read( core, FSKIT_ROUTE_ANY, vdevfs_read, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_read(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_write( core, FSKIT_ROUTE_ANY, vdevfs_write, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_write(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_close( core, FSKIT_ROUTE_ANY, vdevfs_close, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_close(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_sync( core, FSKIT_ROUTE_ANY, vdevfs_sync, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_sync(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   rh = fskit_route_detach( core, FSKIT_ROUTE_ANY, vdevfs_detach, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      
      vdev_error("fskit_route_detach(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      goto vdev_route_fail;
   }
   
   vdev->fs = fs;
   vdev->close_rh = rh;
   
   // set the root to be owned by the effective UID and GID of user
   fskit_chown( core, "/", 0, 0, geteuid(), getegid() );
   
   // import the underlying filesystem once we're mounted, but before taking requests.
   rc = fskit_fuse_postmount_callback( fs, vdevfs_dev_import, vdev );
   if( rc != 0 ) {
      
      vdev_error("fskit_fuse_postmount_callback() rc = %d\n", rc );
      
      vdev->fs = NULL;
      goto vdev_route_fail;
   }
   
   return 0;

vdev_route_fail:

   fskit_fuse_shutdown( fs, NULL );
   free( fs );
   vdevfs_shutdown( vdev );
   return rh;
}


// main loop for vdev frontend 
int vdevfs_main( struct vdevfs* vdev, int fuse_argc, char** fuse_argv ) {
   
   int rc = 0;
   
   rc = fskit_fuse_main( vdev->fs, fuse_argc, fuse_argv );
   
   return rc;
}


// shut down the front-end 
int vdevfs_shutdown( struct vdevfs* vdev ) {
   
   if( vdev->fs != NULL ) {
      
      // stop processing unlink() requests, since the filesystem itself will unlink all files when it frees itself up.
      fskit_unroute_detach( fskit_fuse_get_core( vdev->fs ), vdev->close_rh );
      
      fskit_fuse_shutdown( vdev->fs, NULL );
      free( vdev->fs );
      vdev->fs = NULL;
   }
   
   if( vdev->acls != NULL ) {
      vdev_acl_free_all( vdev->acls, vdev->num_acls );
   }
   
   if( vdev->config != NULL ) {
      vdev_config_free( vdev->config );
      free( vdev->config );
      vdev->config = NULL;
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
   
   if( vdev->mountpoint_dirfd >= 0 ) {
      
      close( vdev->mountpoint_dirfd );
      vdev->mountpoint_dirfd = -1;
   }
   
   memset( vdev, 0, sizeof(struct vdevfs) );
   
   return 0;
}


// for creating, opening, or stating files, verify that the caller is permitted according to our ACLs 
// return 0 on success 
// return -EPERM if denied 
// return other -errno on error 
static int vdevfs_access_check( struct vdevfs* vdev, struct fskit_fuse_state* fs_state, char const* method_name, char const* path ) {
   
   int rc = 0;
   pid_t pid = 0;
   uid_t uid = 0;
   gid_t gid = 0;
   struct stat sb;
   struct pstat ps;
   
   memset( &sb, 0, sizeof(struct stat) );
   sb.st_mode = 0777;
   
   memset( &ps, 0, sizeof(struct pstat) );
   
   // stat the calling process
   pid = fskit_fuse_get_pid();
   uid = fskit_fuse_get_uid( fs_state );
   gid = fskit_fuse_get_gid( fs_state );
   
   vdev_debug("%s('%s') from user %d group %d task %d\n", method_name, path, uid, gid, pid );
   
   // see who's asking 
   rc = pstat( pid, &ps, 0 );
   if( rc != 0 ) {
      
      vdev_error("pstat(%d) rc = %d\n", pid, rc );
      return -EIO;
   }
   
   // apply the ACLs on the stat buffer
   rc = vdev_acl_apply_all( vdev->config, vdev->acls, vdev->num_acls, path, &ps, uid, gid, &sb );
   if( rc < 0 ) {
      
      vdev_error("vdev_acl_apply_all(%s, uid=%d, gid=%d, pid=%d) rc = %d\n", path, uid, gid, pid, rc );
      return -EIO;
   }
   
   // omit entirely?
   if( rc == 0 || (sb.st_mode & 0777) == 0 ) {
      
      // filter
      vdev_debug("DENY '%s'\n", path );
      return -EPERM;
   }
   else {
      
      // accept!
      return 0;
   }
}
   

// mknod: create the device node as normal, but also write to the underlying filesystem as an emergency counter-measure
int vdevfs_mknod( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_cls ) {
   
   int rc = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   char const* path = NULL;
   
   rc = vdevfs_access_check( vdev, fs_state, "mknod", grp->path );
   if( rc < 0 ) {
      
      // denied!
      return -EACCES;
   }
   
   // must be relative path 
   path = grp->path;
   while( *path == '/' && *path != '\0' ) {
      path++;
   }
   
   if( *path == '\0' ) {
      path = ".";
   }
   
   rc = mknodat( vdev->mountpoint_dirfd, path, dev, mode );
   
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("mknodat('%s', '%s') rc = %d\n", vdev->mountpoint, path, rc );
      
      return rc;
   }
   
   return 0;
}


// mkdir: create the directory as normal, but also write to the underlying filesystem as an emergency counter-measure
int vdevfs_mkdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, void** inode_cls ) {
   
   int rc = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   char const* path = NULL;
   
   rc = vdevfs_access_check( vdev, fs_state, "mkdir", grp->path );
   if( rc < 0 ) {
      
      // denied!
      return -EACCES;
   }
   
   // must be relative path 
   path = grp->path;
   while( *path == '/' && *path != '\0' ) {
      path++;
   }
   
   if( *path == '\0' ) {
      path = ".";
   }
   
   rc = mkdirat( vdev->mountpoint_dirfd, path, mode );
   
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("mkdirat('%s', '%s') rc = %d\n", vdev->mountpoint, path, rc );
      
      return rc;
   }
   
   return 0;
}


// creat: create the file as usual, but also write to the underlying filesystem as an emergency counter-measure
// NOTE: since this is backed by FUSE, this handler will only be called for regular files
int vdevfs_create( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, void** inode_cls, void** handle_cls ) {
   
   int fd = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   char const* path = NULL;
   int rc = 0;
   
   rc = vdevfs_access_check( vdev, fs_state, "create", grp->path );
   if( rc < 0 ) {
      
      // denied!
      return -EACCES;
   }
   
   // must be relative path 
   path = grp->path;
   while( *path == '/' && *path != '\0' ) {
      path++;
   }
   
   if( *path == '\0' ) {
      path = ".";
   }
   
   // success!
   fd = openat( vdev->mountpoint_dirfd, path, O_CREAT | O_WRONLY | O_TRUNC, mode );
   if( fd < 0 ) {
      
      fd = -errno;
      vdev_error("openat('%s', '%s') rc = %d\n", vdev->mountpoint, path, fd );
      
      return fd;
   }
   
   // careful...
   void* handle_data = NULL;
   memcpy( &handle_data, &fd, 4 );
   
   *handle_cls = handle_data;
   
   return 0;
}


// open: open the file as usual, but from the underlying filesystem 
// NOTE: since this is backed by FUSE, this handler will only be called for regular files
int vdevfs_open( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, int flags, void** handle_cls ) {
   
   int fd = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   char const* path = NULL;
   
   int rc = 0;
   
   // dir or file?
   char const* method = NULL;
   
   if( fskit_entry_get_type( fent ) == FSKIT_ENTRY_TYPE_DIR ) {
   
      rc = vdevfs_access_check( vdev, fs_state, "opendir", grp->path );
   }
   else {
      
      rc = vdevfs_access_check( vdev, fs_state, "open", grp->path );
   }
   
   if( rc < 0 ) {
      
      // denied!
      return -ENOENT;
   }
   
   // only care about open() for files 
   if( fskit_entry_get_type( fent ) == FSKIT_ENTRY_TYPE_DIR ) {
      
      // done!
      return 0;
   }
   
   // must be relative path 
   path = grp->path;
   while( *path == '/' && *path != '\0' ) {
      path++;
   }
   
   if( *path == '\0' ) {
      path = ".";
   }
   
   fd = openat( vdev->mountpoint_dirfd, path, flags );
   if( fd < 0 ) {
      
      fd = -errno;
      vdev_error("openat(%d, '%s') rc = %d\n", vdev->mountpoint_dirfd, path, fd );
      
      return fd;
   }
   
   // careful...
   void* handle_data = NULL;
   memcpy( &handle_data, &fd, 4 );
   
   *handle_cls = handle_data;
   
   return 0;
}


// read: read as usual, but from the underlying filesystem 
// NOTE: since this is backed by FUSE, this handler will only be called for regular files 
int vdevfs_read( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, char* buf, size_t len, off_t offset, void* handle_cls ) {
   
   // careful...
   int fd = 0;
   memcpy( &fd, &handle_cls, 4 );
   
   int rc = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   
   rc = lseek( fd, offset, SEEK_SET );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("lseek(%d '%s') rc = %d\n", fd, grp->path, rc );
      
      return rc;
   }
   
   rc = read( fd, buf, len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("read(%d '%s') rc = %d\n", fd, grp->path, rc );
      
      return rc;
   }
   
   return rc;
}


// write; write as usual, but to the underlying filesystem 
// NOTE: since this is backed by FUSE, this handler will only be called for regular files 
int vdevfs_write( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, char* buf, size_t len, off_t offset, void* handle_cls ) {
   
   // careful...
   int fd = 0;
   memcpy( &fd, &handle_cls, 4 );
   
   
   int rc = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   
   rc = lseek( fd, offset, SEEK_SET );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("lseek(%d '%s') rc = %d\n", fd, grp->path, rc );
      
      return rc;
   }
   
   rc = write( fd, buf, len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("write(%d '%s') rc = %d\n", fd, grp->path, rc );
      
      return rc;
   }
   
   return rc;
}


// close: close as usual
// NOTE: since this is backed by FUSE, this handler will only be called for regular files
int vdevfs_close( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, void* handle_cls ) {
   
   // only care about close() for files 
   if( fskit_entry_get_type( fent ) == FSKIT_ENTRY_TYPE_DIR ) {
      
      // done!
      return 0;
   }
   
   // careful...
   int fd = 0;
   memcpy( &fd, &handle_cls, 4 );
   
   int rc = 0;
   
   rc = close( fd );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("close(%d '%s') rc = %d\n", fd, grp->path, rc );
      
      return rc;
   }
   
   return rc;
}


// sync: sync as usual
// NOTE: since this is backed by FUSE, this handler will only be called for regular files
int vdevfs_sync( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent ) {
   
   void* user_data = fskit_entry_get_user_data( fent );
   
   // careful...
   int fd = 0;
   memcpy( &fd, &user_data, 4 );
   
   int rc = 0;
   
   rc = fsync( fd );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("fsync(%d '%s') rc = %d\n", fd, grp->path, rc );
      
      return rc;
   }
   
   return rc;
}


// unlink/rmdir: remove the file or device node from the underlying filesystem 
int vdevfs_detach( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, void* inode_cls ) {
   
   int rc = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   char const* method = NULL;
   
   if( fskit_entry_get_type( fent ) == FSKIT_ENTRY_TYPE_DIR ) {
      
      method = "rmdir";
   }
   else {
      
      method = "unlink";
   }
   
   
   rc = vdevfs_access_check( vdev, fs_state, method, grp->path );
   if( rc < 0 ) {
      
      // denied!
      return -ENOENT;
   }
   
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("%s('%s', '%s') rc = %d\n", method, vdev->mountpoint, grp->path, rc );
      
      return rc;
   }
   
   return 0;
}


// stat: equvocate about which devices exist, depending on who's asking
// return -ENOENT not only if the file doesn't exist, but also if the file is blocked by the ACL
int vdevfs_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb ) {
   
   int rc = 0;
   struct vdevfs* vdev = (struct vdevfs*)fskit_core_get_user_data( core );
   struct fskit_fuse_state* fs_state = fskit_fuse_get_state();
   
   rc = vdevfs_access_check( vdev, fs_state, "stat", grp->path );
   if( rc < 0 ) {
      
      // denied!
      return -ENOENT;
   }
   else {
      
      return 0;
   }
}

// readdir: equivocate about which devices exist, depending on who's asking
// omit entries if the ACLs forbid them
int vdevfs_readdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents ) {
   
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
   
   vdev_debug("vdevfs_readdir(%s, %zu) from user %d group %d task %d\n", grp->path, num_dirents, uid, gid, pid );
   
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
