/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

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

#include "vdev.h"
#include "action.h"
#include "libvdev/config.h"

SGLIB_DEFINE_VECTOR_FUNCTIONS( cstr );


// context for removing unplugged device 
struct vdev_device_unplug_context {
   
   struct sglib_cstr_vector* device_paths;      // queue of device paths to search 
   struct vdev_state* state;                    // vdev state
};

// get the instance of the vdevd program that made this device, given its device path 
// instance_str must be at least VDEV_CONFIG_INSTANCE_NONCE_STRLEN bytes
// return 0 on success
// return -errno on failure to stat, open, or read
static int vdev_device_read_vdevd_instance( char const* mountpoint, char const* dev_fullpath, char* instance_str ) {
   
   int rc = 0;
   char const* devpath = dev_fullpath + strlen(mountpoint);
   
   char* instance_attr_relpath = vdev_fullpath( VDEV_METADATA_PREFIX "/dev", devpath, NULL );
   if( instance_attr_relpath == NULL ) {
      
      return -ENOMEM;
   }
   
   char* instance_attr_path = vdev_fullpath( mountpoint, instance_attr_relpath, NULL );
   
   free( instance_attr_relpath );
   instance_attr_relpath = NULL;
   
   if( instance_attr_path == NULL ) {
      
      return -ENOMEM;
   }
   
   char* instance_path = vdev_fullpath( instance_attr_path, VDEV_METADATA_PARAM_INSTANCE, NULL );
   
   free( instance_attr_path );
   instance_attr_path = NULL;
   
   if( instance_path == NULL ) {
      
      return -ENOMEM;
   }
   
   // read the instance string 
   rc = vdev_read_file( instance_path, instance_str, VDEV_CONFIG_INSTANCE_NONCE_STRLEN - 1 );
   if( rc < 0 ) {
      
      vdev_error("vdev_read_file('%s') rc = %d\n", instance_path, rc );
   }
   
   free( instance_path );
   instance_path = NULL;
   
   return rc;
}


// scan callback for a directory.
// queue directories, and unlink device files that are no longer plugged in.
// return 0 on success
// return -ENOMEM on OOM
// NOTE: mask unlink() failures, but log them.
static int vdev_remove_unplugged_device( char const* path, void* cls ) {
   
   int rc = 0;
   struct stat sb;
   char instance_str[ VDEV_CONFIG_INSTANCE_NONCE_STRLEN + 1 ];
   char basename[ NAME_MAX+1 ];
   
   memset( instance_str, 0, VDEV_CONFIG_INSTANCE_NONCE_STRLEN + 1 );
   
   // extract cls 
   struct vdev_device_unplug_context* ctx = (struct vdev_device_unplug_context*)cls;
   
   struct sglib_cstr_vector* device_paths = ctx->device_paths;
   struct vdev_state* state = ctx->state;
   
   if( strlen(path) == 0 ) {
      
      // nothing to do 
      return 0;
   }
   
   vdev_basename( path, basename );
   
   // is this . or ..?
   if( strcmp(basename, ".") == 0 || strcmp(basename, "..") == 0 ) {
      return 0;
   }
   
   // what is this?
   rc = lstat( path, &sb );
   if( rc != 0 ) {
      
      vdev_error("stat('%s') rc = %d\n", path, rc );
      
      // mask
      return 0;
   }
   
   // is this a directory?
   if( S_ISDIR( sb.st_mode ) ) {
       
      // skip the hwdb 
      if( strcmp( basename, "hwdb" ) == 0 ) {
         return 0;
      }

      // search this later 
      char* path_dup = vdev_strdup_or_null( path );
      if( path_dup == NULL ) {
         
         // really can't continue 
         return -ENOMEM;
      }
      
      sglib_cstr_vector_push_back( device_paths, path_dup );
      
      return 0;
   }
   
   // is this a device file?
   if( S_ISBLK( sb.st_mode ) || S_ISCHR( sb.st_mode ) ) {
      
      // what's the instance value?
      rc = vdev_device_read_vdevd_instance( state->config->mountpoint, path, instance_str );
      if( rc != 0 ) {
         
         vdev_error("vdev_device_read_vdevd_instance('%s') rc = %d\n", path, rc );
         
         // mask 
         return 0;
      }
      
      // does it match ours?
      if( strcmp( state->config->instance_str, instance_str ) != 0 ) {

         struct vdev_device_request* to_delete = NULL;
         char const* device_path = NULL;
         
         vdev_debug("Remove unplugged device '%s'\n", path );
         
         device_path = path + strlen( state->config->mountpoint );
         
         to_delete = VDEV_CALLOC( struct vdev_device_request, 1 );
         if( to_delete == NULL ) {
            
            // OOM 
            return -ENOMEM;
         }
         
         rc = vdev_device_request_init( to_delete, state, VDEV_DEVICE_REMOVE, device_path );
         if( rc != 0 ) {
            
            // OOM 
            return rc;
         }
         
         // populate 
         vdev_device_request_set_dev( to_delete, sb.st_rdev );
         vdev_device_request_set_mode( to_delete, S_ISBLK( sb.st_mode ) ? S_IFBLK : S_IFCHR );
         
         // remove it 
         rc = vdev_device_remove( to_delete );
         if( rc != 0 ) {
            
            vdev_warn("vdev_device_remove('%s') rc = %d\n", device_path, rc );
            rc = 0;
         }
      }
   }
   
   return 0;
}


// remove all devices that no longer exist--that is, the contents of the /dev/metadata/$DEVICE_PATH/dev_instance file 
// does not match this vdev's instance nonce.
// this is used when running with --once.
int vdev_remove_unplugged_devices( struct vdev_state* state ) {
   
   int rc = 0;
   struct sglib_cstr_vector device_paths;
   char* devroot = vdev_strdup_or_null( state->config->mountpoint );
   char* next_dir = NULL;
   size_t next_dir_index = 0;
   struct stat sb;
   
   struct vdev_device_unplug_context unplug_ctx;
   
   unplug_ctx.state = state;
   unplug_ctx.device_paths = &device_paths;
   
   if( devroot == NULL ) {
      return -ENOMEM;
   }
   
   sglib_cstr_vector_init( &device_paths );
   
   // walk /dev breadth-first
   rc = sglib_cstr_vector_push_back( &device_paths, devroot );
   if( rc != 0 ) {
      
      sglib_cstr_vector_free( &device_paths );
      free( devroot );
      
      return rc;
   }
   
   while( next_dir_index < sglib_cstr_vector_size( &device_paths ) ) {
      
      // next path 
      next_dir = sglib_cstr_vector_at( &device_paths, next_dir_index );
      sglib_cstr_vector_set( &device_paths, NULL, next_dir_index );
      
      next_dir_index++;
      
      // scan this directory, and remove unplugged device files and remember the directories to search
      rc = vdev_load_all( next_dir, vdev_remove_unplugged_device, &unplug_ctx );
      if( rc != 0 ) {
         
         vdev_error("vdev_load_all('%s') rc = %d\n", next_dir, rc );
         
         free( next_dir );
         break;
      }
      
      free( next_dir );
   }
   
   // free any unused vector space
   while( next_dir_index < sglib_cstr_vector_size( &device_paths ) ) {
      
      next_dir = sglib_cstr_vector_at( &device_paths, next_dir_index );
      sglib_cstr_vector_set( &device_paths, NULL, next_dir_index );
      
      next_dir_index++;
      
      free( next_dir );
   }
   
   sglib_cstr_vector_free( &device_paths );
   
   return rc;
}


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


// process a single line of text into a device request
// 
// line format is:
//    type name PARAM=VALUE...
// $type is "c", "b", or "u" for character, block, or unknown device;
// $name is the path to the device relative to the /dev mountpoint (or the string "None")
// $uid is the user ID that shall own the device (if we'll mknod it)
// $gid is the group ID that shall own the device (if we'll mknod it)
// PARAM=VALUE... is a list of $PARAM and $VALUE pairs that shall be the OS-specific parameters.
// NOTE: currently, we do not tolerate spaces in any of these fields.  Sorry.
//
// fill in the given vdev_device_request structure.
// return 0 on success 
// return -ENOENT if we're missing a parameter
// return -EINVAL if the parameter is malformed
// return -ENOMEM on OOM 
int vdev_parse_device_request( struct vdev_state* state, struct vdev_device_request* vreq, char* line ) {
   
   int rc = 0;
   int stat_rc;
   char* tok = NULL;
   char* tokstr = line;
   char* tok_ctx = NULL;
   char* tmp = NULL;
   
   mode_t mode = 0;
   dev_t major = 0;
   dev_t minor = 0;
   char name[4097];
   char fullpath[ PATH_MAX+1 ];
   char keyvalue_buf[4097];
   char* key = NULL;
   char* value = NULL;
   
   struct stat sb;
   
   // type 
   tok = strtok_r( tokstr, " \t", &tok_ctx );
   tokstr = NULL;
   
   if( tok == NULL ) {
      
      // no type 
      fprintf(stderr, "Missing type\n");
      return -ENOENT;
   }
   
   if( strlen(tok) != 1 ) {
      
      // invalid type 
      fprintf(stderr, "Unrecognized type '%s'.  Expected 'c', 'b', or 'u'\n", tok );
      return -EINVAL;
   }
   
   if( tok[0] != 'c' && tok[0] != 'b' && tok[0] != 'u' ) {
      
      // invalid type 
      fprintf(stderr, "Unrecognized type '%s'.  Expected 'c', 'b', or 'u'\n", tok );
      return -EINVAL;
   }
   
   if( tok[0] == 'c' ) {
      mode = S_IFCHR;
   }
   else if( tok[0] == 'b' ) {
      mode = S_IFBLK;
   }
   
   // name 
   tok = strtok_r( tokstr, " \t", &tok_ctx );
   if( tok == NULL ) {
      
      // no name 
      fprintf(stderr, "Missing name\n");
      return -ENOENT;
   }
   
   strcpy( name, tok );
   
   // major 
   tok = strtok_r( tokstr, " \t", &tok_ctx );
   if( tok == NULL ) {
      
      // no major 
      fprintf(stderr, "Missing major device number\n");
      return -ENOENT;
   }
   
   major = (dev_t)strtoul( tok, &tmp, 10 );
   if( tmp == tok || *tmp != '\0' ) {
      
      // invalid major 
      fprintf(stderr, "Invalid major device number '%s'\n", tok );
      return -EINVAL;
   }
   
   // minor 
   tok = strtok_r( tokstr, " \t", &tok_ctx );
   if( tok == NULL ) {
      
      // no minor 
      fprintf(stderr, "Missing minor device number\n");
      return -ENOENT;
   }
   
   minor = (dev_t)strtoul( tok, &tmp, 10 );
   if( tmp == tok || *tmp != '\0' ) {
      
      // invalid minor 
      fprintf(stderr, "Invalid minor device number '%s'\n", tok );
      return -EINVAL;
   }
   
   // set up the device... 
   rc = vdev_device_request_init( vreq, state, VDEV_DEVICE_ADD, name );
   if( rc != 0 ) {
      
      vdev_error("vdev_device_request_init('%s') rc = %d\n", name, rc );
      return rc;
   }
   
   vdev_device_request_set_type( vreq, VDEV_DEVICE_ADD );
   vdev_device_request_set_mode( vreq, mode );
   vdev_device_request_set_dev( vreq, makedev( major, minor ) );
   
   // parameters 
   while( tok != NULL ) {
      
      tok = strtok_r( tokstr, " \t", &tok_ctx );
      if( tok == NULL ) {
         break;
      }
      
      if( strlen(tok) > 4096 ) {
         
         // too big 
         fprintf(stderr, "OS parameter too long: '%s'\n", tok );
         vdev_device_request_free( vreq );
         return -EINVAL;
      }
      
      strcpy( keyvalue_buf, tok );
      
      rc = vdev_keyvalue_next( keyvalue_buf, &key, &value );
      if( rc < 0 ) {
         
         // could not parse 
         fprintf(stderr, "Unparsible OS parameter: '%s'\n", tok );
         vdev_device_request_free( vreq );
         return -EINVAL;
      }
      
      rc = vdev_device_request_add_param( vreq, key, value );
      if( rc != 0 ) {
         
         vdev_device_request_free( vreq );
         return rc;
      }
   }
   
   // finally, does this device exist already?
   snprintf( fullpath, PATH_MAX, "%s/%s", state->config->mountpoint, name );
   stat_rc = stat( fullpath, &sb );
   
   if( stat_rc == 0 ) {
      
      vdev_device_request_set_exists( vreq, true );
   }
   else {
      
      vdev_device_request_set_exists( vreq, false );
   }
   
   return rc;
}


// process a newline-delimited textual list of device events.
// 
// line format is:
//    type name major minor PARAM=VALUE...
// $type is "c", "b", or "u" for character, block, or unknown device
// $name is the path to the device relative to the /dev mountpoint (or the string "None")
// $major is the major device number (ignored if $type == 'u')
// $minor is the minor device number (ignored if $type == 'u')
// PARAM=VALUE... is a list of $PARAM and $VALUE pairs that shall be the OS-specific parameters.
//
// convert each line into a device request and enqueue it.
// NOTE: each line should be less than 4096 characters.
// NOTE: currently, we do not tolerate spaces in any of these fields.  Sorry.
// return 0 on success 
// return -ERANGE if a line exceeded 4096 characters
int vdev_load_device_requests( struct vdev_state* state, char* text_buf, size_t text_buflen ) {
   
   int rc = 0;
   char* cur_line = NULL;
   char* next_line = NULL;
   
   struct vdev_device_request* vreq = NULL;
   
   size_t consumed = 0;
   size_t line_len = 0;
   char line_buf[4097];
   char line_buf_dbg[4097];     // for debugging
   
   dev_t major = 0;
   dev_t minor = 0;
   
   cur_line = text_buf;
   while( consumed < text_buflen && cur_line != NULL ) {
      
      next_line = strchr( cur_line, '\n' );
      if( next_line == NULL ) {
         
         line_len = strlen( cur_line );
         
         if( line_len == 0 ) {
            // done 
            break;
         }
         
         // last line
         if( line_len < 4096 ) {
            
            strcpy( line_buf, cur_line );
         }
         else {
            
            // too big
            return -ERANGE;
         }
      }
      else {
         
         // copy until '\n'
         line_len = (size_t)(next_line - cur_line) / sizeof(char);
         memcpy( line_buf, cur_line, line_len );
         
         line_buf[ line_len ] = '\0';
      }
      
      vdev_debug("Preseed device: '%s'\n", line_buf );
      
      strcpy( line_buf_dbg, line_buf );
      
      vreq = VDEV_CALLOC( struct vdev_device_request, 1 );
      if( vreq == NULL ) {
         
         // OOM 
         return -ENOMEM;
      }
      
      // consume the line 
      rc = vdev_parse_device_request( state, vreq, line_buf );
      if( rc != 0 ) {
         
         fprintf(stderr, "Could not parse line '%s' (rc = %d)\n", line_buf_dbg, rc );
         vdev_error("vdev_parse_device_request('%s') rc = %d\n", line_buf_dbg, rc );
         
         free( vreq );
         return rc;
      }
      
      // enqueue the device!
      rc = vdev_device_request_enqueue( &state->device_wq, vreq );
      if( rc != 0 ) {
         
         vdev_error("vdev_device_request_enqueue('%s') rc = %d\n", line_buf_dbg, rc );
         
         free( vreq );
         return rc;
      }
      
      // next line 
      cur_line = next_line;
      consumed += strlen(next_line);
      
      while( consumed < text_buflen && *cur_line == '\n' ) {
         
         cur_line++;
         consumed++;
      }
   }
   
   return rc;
}


// run the pre-seed command, if given 
// return 0 on success
// return -ENOMEM on OOM 
// return non-zero on non-zero exit status
// TODO: "unlimited" output buffer space--like a pipe
int vdev_preseed_run( struct vdev_state* vdev ) {
   
   int rc = 0;
   int exit_status = 0;
   char* command = NULL;
   
   size_t output_len = 1024 * 1024;         // 1MB buffer for initial devices, just in case
   char* output = NULL;
   
   if( vdev->config->preseed_path == NULL ) {
      // nothing to do 
      return 0;
   }
   
   output = VDEV_CALLOC( char, output_len );
   if( output == NULL ) {
      
      // OOM 
      return -ENOMEM;
   }
   
   command = VDEV_CALLOC( char, strlen( vdev->config->preseed_path ) + 2 + strlen( vdev->config->mountpoint ) + 2 + strlen( vdev->config->config_path ) + 1 );
   if( command == NULL ) {
      
      // OOM
      free( output );
      return -ENOMEM;
   }
   
   sprintf(command, "%s %s %s", vdev->config->preseed_path, vdev->config->mountpoint, vdev->config->config_path );
   
   rc = vdev_subprocess( command, NULL, &output, output_len, &exit_status, true );
   if( rc != 0 ) {
      
      vdev_error("vdev_subprocess('%s') rc = %d\n", command, rc );
   }
   else if( exit_status != 0 ) {
      
      vdev_error("vdev_subprocess('%s') exit status %d\n", command, exit_status );
      rc = exit_status;
   }
   
   free( command );
   command = NULL;
   
   if( rc != 0 ) {
      
      free( output );
      return rc;
   }
   
   // process the preseed devices...
   rc = vdev_load_device_requests( vdev, output, output_len );
   if( rc != 0 ) {
      
      vdev_error("vdev_load_device_requests rc = %d\n", rc);
   }
   
   free( output );
   output = NULL;
   
   return rc;
}


// global vdev initialization 
int vdev_init( struct vdev_state* vdev, int argc, char** argv ) {
   
   // global setup 
   vdev_setup_global();
   
   int rc = 0;
   
   int fuse_argc = 0;
   char** fuse_argv = VDEV_CALLOC( char*, argc + 1 );
   
   if( fuse_argv == NULL ) {
      
      return -ENOMEM;
   }
   
   // config...
   vdev->config = VDEV_CALLOC( struct vdev_config, 1 );
   if( vdev->config == NULL ) {
      
      free( fuse_argv );
      return -ENOMEM;
   }
   
   // config init
   rc = vdev_config_init( vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_init rc = %d\n", rc );
      return rc;
   }
   
   // parse config options from command-line 
   rc = vdev_config_load_from_args( vdev->config, argc, argv, &fuse_argc, fuse_argv );
   
   // not needed for vdevd
   free( fuse_argv );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_config_load_from_argv rc = %d\n", rc );
      
      vdev_config_usage( argv[0] );
      
      return rc;
   }
   
   // if we didn't get a config file, use the default one
   if( vdev->config->config_path == NULL ) {
      
      vdev->config->config_path = vdev_strdup_or_null( VDEV_CONFIG_FILE );
      if( vdev->config->config_path == NULL ) {

         // OOM 
         return -ENOMEM;
      }
   }
   
   vdev_set_debug_level( vdev->config->debug_level );
   vdev_set_error_level( vdev->config->error_level );
   
   vdev_info("Config file:      '%s'\n", vdev->config->config_path );
   vdev_info("Log debug level:  '%s'\n", (vdev->config->debug_level == VDEV_LOGLEVEL_DEBUG ? "debug" : (vdev->config->debug_level == VDEV_LOGLEVEL_INFO ? "info" : "none")) );
   vdev_info("Log error level:  '%s'\n", (vdev->config->error_level == VDEV_LOGLEVEL_WARN ? "warning" : (vdev->config->error_level == VDEV_LOGLEVEL_ERROR ? "error" : "none")) );
   
   // load from file...
   rc = vdev_config_load( vdev->config->config_path, vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_load('%s') rc = %d\n", vdev->config->config_path, rc );
      
      return rc;
   }
   
   // if no command-line loglevel is given, then take it from the config file (if given)
   if( vdev->config->debug_level != VDEV_LOGLEVEL_NONE ) {
      
      vdev_set_debug_level( vdev->config->debug_level );
   }
   
   if( vdev->config->error_level != VDEV_LOGLEVEL_NONE ) {
      
      vdev_set_error_level( vdev->config->error_level );
   }
   
   // convert to absolute paths 
   rc = vdev_config_fullpaths( vdev->config );
   if( rc != 0 ) {
      
      vdev_error("vdev_config_fullpaths rc = %d\n", rc );
      
      return rc;
   }
   
   vdev_info("vdev actions dir: '%s'\n", vdev->config->acts_dir );
   vdev_info("helpers dir:      '%s'\n", vdev->config->helpers_dir );
   vdev_info("logfile path:     '%s'\n", vdev->config->logfile_path );
   vdev_info("pidfile path:     '%s'\n", vdev->config->pidfile_path );
   vdev_info("default mode:      0%o\n", vdev->config->default_mode );
   vdev_info("preseed script:   '%s'\n", vdev->config->preseed_path );
   
   vdev->mountpoint = vdev_strdup_or_null( vdev->config->mountpoint );
   vdev->once = vdev->config->once;
   
   if( vdev->mountpoint == NULL ) {
      
      vdev_error("Failed to set mountpoint, config->mountpount = '%s'\n", vdev->config->mountpoint );
      
      return -EINVAL;
   }
   else {
      
      vdev_info("mountpoint:       '%s'\n", vdev->mountpoint );
   }
   
   vdev->argc = argc;
   vdev->argv = argv;
   
   // load actions 
   rc = vdev_action_load_all( vdev->config, &vdev->acts, &vdev->num_acts );
   if( rc != 0) {
      
      vdev_error("vdev_action_load_all('%s') rc = %d\n", vdev->config->acts_dir, rc );
      
      return rc;
   }
   
   // initialize request work queue 
   rc = vdev_wq_init( &vdev->device_wq, vdev );
   if( rc != 0 ) {
      
      vdev_error("vdev_wq_init rc = %d\n", rc );
      
      return rc;
   }
   
   return 0;
}


// main loop for the back-end 
// takes a file descriptor to be written to once coldplug processing has finished.
// return 0 on success
// return -errno on failure to daemonize, or abnormal OS-specific back-end failure
int vdev_main( struct vdev_state* vdev, int coldplug_finished_fd ) {
   
   int rc = 0;
   
   vdev->coldplug_finished_fd = coldplug_finished_fd;
   
   char* metadata_dir = vdev_device_metadata_fullpath( vdev->mountpoint, "" );
   if( metadata_dir == NULL ) {
      
      return -ENOMEM;
   }
   
   // create metadata directory 
   rc = vdev_mkdirs( metadata_dir, 0, 0755 );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_mkdirs('%s') rc = %d\n", metadata_dir, rc );
      
      free( metadata_dir );
      return rc;
   }
   
   free( metadata_dir );
   
   rc = vdev_os_main( vdev->os );
   
   return rc;
}


// signal that we've processed all coldplug devices
int vdev_signal_coldplug_finished( struct vdev_state* vdev, int status ) {
   
   if( vdev->coldplug_finished_fd > 0 ) {
      write( vdev->coldplug_finished_fd, &status, sizeof(status));
      close( vdev->coldplug_finished_fd );
      vdev->coldplug_finished_fd = -1;
   }
   
   return 0;
}


// stop vdev 
// NOTE: if this fails, there's not really a way to recover
// return 0 on success
// return non-zero if we failed to stop the work queue
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
   
   // stop all actions' daemonlets
   vdev_action_daemonlet_stop_all( vdev->acts, vdev->num_acts );
   
   return rc;
}

// free up vdev.
// only call after vdev_stop().
// return 0 on success, and print out benchmarks 
// return -EINVAL if we're still running.
int vdev_shutdown( struct vdev_state* vdev, bool unlink_pidfile ) {
   
   if( vdev->running ) {
      return -EINVAL;
   }
   
   // remove the PID file, if we have one 
   if( vdev->config->pidfile_path != NULL && unlink_pidfile ) {
      unlink( vdev->config->pidfile_path );
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
