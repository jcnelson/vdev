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

#include "device.h"
#include "workqueue.h"
#include "os/methods.h"
#include "action.h"
#include "vdev.h"

// create a request 
// return 0 on success
// return -ENOMEM if we can't allocate device parameters
int vdev_device_request_init( struct vdev_device_request* req, struct vdev_state* state, vdev_device_request_t req_type, char const* path ) {
   
   memset( req, 0, sizeof(struct vdev_device_request) );
   
   if( path != NULL ) {
      req->path = vdev_strdup_or_null( path );
      if( req->path == NULL ) {
         return -ENOMEM;
      }
   }
   
   req->state = state;
   req->type = req_type;
   
   return 0;
}

// free a request 
int vdev_device_request_free( struct vdev_device_request* req ) {
   
   if( req->params != NULL ) {
      
      vdev_params_free( req->params );
      req->params = NULL;
   }
   
   if( req->path != NULL ) {
      
      free( req->path );
      req->path = NULL;
   }
   
   if( req->renamed_path != NULL ) {
      
      free( req->renamed_path );
      req->renamed_path = NULL;
   }
   
   memset( req, 0, sizeof(struct vdev_device_request) );
   
   return 0;
}

// add a device parameter (must be unique)
// return 0 on success
// return -EEXIST if the parameter exists
// return -ENOMEM if OOM
int vdev_device_request_add_param( struct vdev_device_request* req, char const* key, char const* value ) {
   
   return vdev_params_add( &req->params, key, value );
}

// create a KEY=VALUE string
static int vdev_device_request_make_env_str( char const* key, char const* value, char** ret ) {
   
   char* e = VDEV_CALLOC( char, strlen(key) + 1 + strlen(value) + 1 );
   
   if( e == NULL ) {
      return -ENOMEM;
   }
   
   sprintf(e, "%s=%s", key, value );
   *ret = e;
   
   return 0;
}


// action to const string 
static char const* vdev_device_request_type_to_string( vdev_device_request_t req ) {
   
   static char const* req_tbl[] = {
      "add",
      "remove",
      "any",
      "none"
   };
   
   if( req < 0 || req > VDEV_DEVICE_ANY ) {
      return req_tbl[3];
   }
   
   return req_tbl[req-1];
}


// mode to const string 
static char const* vdev_device_request_mode_to_string( mode_t mode ) {
   
   static char const* blk_str = "block";
   static char const* chr_str = "char";
   static char const* none_str = "none";
   
   if( S_ISBLK(mode) ) {
      return blk_str;
   }
   
   else if( S_ISCHR(mode) ) {
      return chr_str;
   }
   
   return none_str;
}

// convert a device request to a list of null-terminated KEY=VALUE environment variable strings 
// put the resulting vector into **ret_env, and put the number of variables into *num_env 
// if is_daemonlet is set, then set VDEV_DAEMONLET=1
// return 0 on success 
// return negative on error 
int vdev_device_request_to_env( struct vdev_device_request* req, vdev_params* helper_vars, char*** ret_env, size_t* num_env, int is_daemonlet ) {
   
   // type --> VDEV_ACTION
   // path --> VDEV_PATH (if non-null)
   // dev --> VDEV_MAJOR, VDEV_MINOR (if given)
   // mode --> VDEV_MODE (if given)
   // params --> VDEV_OS_* 
   // helper vars --> VDEV_VAR_*
   // mountpoint --> VDEV_MOUNTPOINT
   // metadata --> VDEV_METADATA (if path is non-null)
   // global metadata --> VDEV_GLOBAL_METADATA
   // helpers --> VDEV_HELPERS
   // logfile --> VDEV_LOGFILE
   // vdev instance nonce --> VDEV_INSTANCE
   // config file --> VDEV_CONFIG_FILE
   // daemonlet --> VDEV_DAEMONLET (0 by default, 1 if is_daemonlet is non-zero)
   
   size_t num_vars = 15 + sglib_vdev_params_len( req->params ) + sglib_vdev_params_len( helper_vars );
   int i = 0;
   int rc = 0;
   char dev_buf[51];
   struct vdev_param_t* dp = NULL;
   struct sglib_vdev_params_iterator itr;
   char* vdev_path = req->renamed_path;
   char metadata_dir[ PATH_MAX + 1 ];
   char global_metadata_dir[ PATH_MAX + 1 ];
   char const* is_daemonlet_str = NULL;
   
   if( is_daemonlet != 0 ) {
      is_daemonlet_str = "1";
   }
   else {
      is_daemonlet_str = "0";
   }
   
   memset( metadata_dir, 0, PATH_MAX+1 );
   memset( global_metadata_dir, 0, PATH_MAX+1 );
   memset( dev_buf, 0, 51 );
   
   if( vdev_path == NULL ) {
      
      vdev_path = req->path;
   }
   
   if( vdev_path != NULL ) {
      snprintf( metadata_dir, PATH_MAX, "%s/" VDEV_METADATA_PREFIX "/dev/%s", req->state->mountpoint, vdev_path );
   }
   
   char** env = VDEV_CALLOC( char*, num_vars + 1 );
   
   if( env == NULL ) {
      return -ENOMEM;
   }
   
   snprintf( global_metadata_dir, PATH_MAX, "%s/" VDEV_METADATA_PREFIX, req->state->mountpoint );
   
   rc = vdev_device_request_make_env_str( "VDEV_MOUNTPOINT", req->state->mountpoint, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   rc = vdev_device_request_make_env_str( "VDEV_ACTION", vdev_device_request_type_to_string( req->type ), &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   if( vdev_path != NULL ) {
      
      rc = vdev_device_request_make_env_str( "VDEV_PATH", vdev_path, &env[i] );
      if( rc != 0 ) {
         
         VDEV_FREE_LIST( env );
         return rc;
      }
      
      i++;
      
      rc = vdev_device_request_make_env_str( "VDEV_METADATA", metadata_dir, &env[i] );
      if( rc != 0 ) {
         
         VDEV_FREE_LIST( env );
         return rc;
      }
      
      i++;
   }
   
   rc = vdev_device_request_make_env_str( "VDEV_GLOBAL_METADATA", global_metadata_dir, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   rc = vdev_device_request_make_env_str( "VDEV_CONFIG_FILE", req->state->config->config_path, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   if( req->dev != 0 ) {
         
      snprintf(dev_buf, 50, "%u", major(req->dev) );
      
      rc = vdev_device_request_make_env_str( "VDEV_MAJOR", dev_buf, &env[i] );
      if( rc != 0 ) {
         
         VDEV_FREE_LIST( env );
         return rc;
      }
      
      i++;
      
      snprintf(dev_buf, 50, "%u", minor(req->dev) );
      
      rc = vdev_device_request_make_env_str( "VDEV_MINOR", dev_buf, &env[i] );
      if( rc != 0 ) {
         
         VDEV_FREE_LIST( env );
         return rc;
      }
      
      i++;
   }
   
   if( (req->mode & (S_IFBLK | S_IFCHR)) != 0 ) {
      
      rc = vdev_device_request_make_env_str( "VDEV_MODE", vdev_device_request_mode_to_string( req->mode ), &env[i] );
      if( rc != 0 ) {
         
         VDEV_FREE_LIST( env );
         return rc;
      }
      
      i++;
   }
   
   rc = vdev_device_request_make_env_str( "VDEV_HELPERS", req->state->config->helpers_dir, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   if( req->state->config->logfile_path != NULL && strcasecmp( req->state->config->logfile_path, "syslog" ) != 0 ) {
      
      rc = vdev_device_request_make_env_str( "VDEV_LOGFILE", req->state->config->logfile_path, &env[i] );
      if( rc != 0 ) {
         
         VDEV_FREE_LIST( env );
         return rc;
      }
      
      i++;
   }
   
   rc = vdev_device_request_make_env_str( "VDEV_INSTANCE", req->state->config->instance_str, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   rc = vdev_device_request_make_env_str( "VDEV_DAEMONLET", is_daemonlet_str, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   // add all OS-specific parameters 
   for( dp = sglib_vdev_params_it_init_inorder( &itr, req->params ); dp != NULL; dp = sglib_vdev_params_it_next( &itr ) ) {
      
      char const* param_key = dp->key;
      char const* param_value = dp->value;
      
      // prepend with "VDEV_OS_"
      char* varname = VDEV_CALLOC( char, strlen(param_key) + 1 + strlen("VDEV_OS_") );
      
      if( varname == NULL ) {
         
         VDEV_FREE_LIST( env );
         return -ENOMEM;
      }
      
      sprintf( varname, "VDEV_OS_%s", param_key );
      
      rc = vdev_device_request_make_env_str( varname, param_value, &env[i] );
      
      free( varname );
      
      if( rc != 0 ) {
         
         VDEV_FREE_LIST( env );
         return rc;
      }
      
      i++;
   }
   
   // add all helper-specific variables 
   for( dp = sglib_vdev_params_it_init_inorder( &itr, helper_vars ); dp != NULL; dp = sglib_vdev_params_it_next( &itr ) ) {
      
      char const* param_key = dp->key;
      char const* param_value = dp->value;
      
      // prepend with "VDEV_OS_"
      char* varname = VDEV_CALLOC( char, strlen(param_key) + 1 + strlen("VDEV_VAR_") );
      
      if( varname == NULL ) {
         
         VDEV_FREE_LIST( env );
         return -ENOMEM;
      }
      
      sprintf( varname, "VDEV_VAR_%s", param_key );
      
      rc = vdev_device_request_make_env_str( varname, param_value, &env[i] );
      
      free( varname );
      
      if( rc != 0 ) {
         
         VDEV_FREE_LIST( env );
         return rc;
      }
      
      i++;
   }
   
   *ret_env = env;
   *num_env = i;
   
   return 0;
}


// set the path 
// return 0 on success
// return -ENOMEM on OOM
int vdev_device_request_set_path( struct vdev_device_request* req, char const* path ) {
   
   if( req->path != NULL ) {
      free( req->path );
   }
   
   req->path = vdev_strdup_or_null( path );
   
   if( req->path == NULL && path != NULL ) {
      return -ENOMEM;
   }
   
   return 0;
}

// set the device 
// always succeeds
int vdev_device_request_set_dev( struct vdev_device_request* req, dev_t dev ) {
   
   req->dev = dev;
   return 0;
}


// mark this device as already existing 
// always succeeds
int vdev_device_request_set_exists( struct vdev_device_request* req, bool exists ) {
   
   req->exists = exists;
   return 0;
}
   

// device request sanity check 
// return 0 if valid 
// return -EINVAL if not valid
int vdev_device_request_sanity_check( struct vdev_device_request* req ) {
   
   if( req->path == NULL ) {
      
      vdev_error("request %p missing path\n", req );
      return -EINVAL;
   }
   
   if( req->type == VDEV_DEVICE_INVALID ) {
      
      vdev_error("request %p has no request type\n", req );
      return -EINVAL;
   }
   
   return 0;
}


// set the request type 
int vdev_device_request_set_type( struct vdev_device_request* req, vdev_device_request_t req_type ) {
   
   req->type = req_type;
   return 0;
}


// set the request device mode 
int vdev_device_request_set_mode( struct vdev_device_request* req, mode_t mode ) {
   
   req->mode = mode;
   return 0;
}


// generate a path to a device's metadata
// return the malloc'ed path on success
// return NULL on error 
char* vdev_device_metadata_fullpath( char const* mountpoint, char const* device_path ) {
   
   char* base_dir = NULL;
   char* metadata_dir = VDEV_CALLOC( char, strlen( mountpoint ) + 1 + strlen(VDEV_METADATA_PREFIX) + 2 + strlen(device_path) + 1 );
   if( metadata_dir == NULL ) {
      
      return NULL;
   }
   
   sprintf( metadata_dir, VDEV_METADATA_PREFIX "/dev/%s", device_path );
   
   base_dir = vdev_fullpath( mountpoint, metadata_dir, NULL );
   
   free( metadata_dir );
   
   if( base_dir == NULL ) {
      
      return NULL;
   }
   
   return base_dir;
}


// create or update an item of metadata to $VDEV_MOUNTPOINT/$VDEV_METADATA_PREFIX/dev/$VDEV_PATH/$PARAM_KEY
// return 0 on success
// return negative on error 
static int vdev_device_put_metadata_item( char* base_dir, char const* param_name, char const* param_value, int flags, mode_t mode ) {
   
   int rc = 0;
   char* param_value_with_newline = NULL;
   char* key_value_path = NULL;
   
   key_value_path = vdev_fullpath( base_dir, param_name, NULL );
   
   if( key_value_path == NULL ) {
      
      rc = -ENOMEM;
      return rc;
   }
   
   param_value_with_newline = VDEV_CALLOC( char, strlen(param_value) + 2 );
   if( param_value_with_newline == NULL ) {
      
      rc = -ENOMEM;
      free( key_value_path );
      return rc;
   }
   
   strcpy( param_value_with_newline, param_value );
   strcat( param_value_with_newline, "\n");
   
   rc = vdev_write_file( key_value_path, param_value_with_newline, strlen(param_value_with_newline), flags, mode );
   
   free( param_value_with_newline );
   
   if( rc < 0 ) {
      
      vdev_error("vdev_write_file('%s', '%s') rc = %d\n", key_value_path, param_value, rc );
   }
   else {
      
      rc = 0;
   }
   
   free( key_value_path );
   
   return rc;
}


// record extra metadata (i.e. vdev parameters) for a device node
// overwrite existing metadata if it already exists for this device.
// return 0 on success
// return -ENOMEM on OOM 
// return -EINVAL if there is no device path defined for this request
// return negative on I/O error
// NOTE: the device metadata directory ($VDEV_MOUNTPOINT/$VDEV_METADATA_PREFIX/dev) must exist
static int vdev_device_put_metadata( struct vdev_device_request* req ) {

   int rc = 0;
   struct sglib_vdev_params_iterator itr;
   struct vdev_param_t* dp;
   
   char* base_dir = NULL;
   char* device_path = NULL;
   
   // only create device metadata if the device path is known.
   if( req->renamed_path != NULL ) {
      
      device_path = req->renamed_path;
   }
   else if( req->path != NULL ) {
      
      device_path = req->path;
   }
   else {
      
      return -EINVAL;
   }
   
   // unknown?
   if( strcmp( device_path, VDEV_DEVICE_PATH_UNKNOWN ) == 0 ) {
      
      return -EINVAL;
   }
   
   // path to device metadata
   base_dir = vdev_device_metadata_fullpath( req->state->mountpoint, req->renamed_path );
   if( base_dir == NULL ) {
      
      return -ENOMEM;
   }
   
   // all directories must exist 
   rc = vdev_mkdirs( base_dir, strlen( req->state->mountpoint ), 0755 );
   if( rc != 0 ) {
      
      vdev_error("vdev_mkdirs('%s') rc = %d\n", base_dir, rc );
      
      free( base_dir );
      base_dir = NULL;
      
      return rc;
   }
   
   // save instance nonce
   rc = vdev_device_put_metadata_item( base_dir, VDEV_METADATA_PARAM_INSTANCE, req->state->config->instance_str, O_CREAT | O_WRONLY | O_TRUNC, 0644 );
   if( rc != 0 ) {
      
      if( rc == -EEXIST ) {
         
         // try to update instead
         rc = vdev_device_put_metadata_item( base_dir, VDEV_METADATA_PARAM_INSTANCE, req->state->config->instance_str, O_WRONLY | O_TRUNC, 0644 );
      }
      
      if( rc != 0 ) {
         vdev_error("vdev_device_put_metadata_item('%s', '%s') rc = %d\n", VDEV_METADATA_PARAM_INSTANCE, req->state->config->instance_str, rc );
      }
   }
   
   free( base_dir );
   
   return rc;
}


// remove helper for vdev_device_remove_metadata
// return 0 on success
// return -errno for unlink failure
static int vdev_device_remove_metadata_file( char const* fp, void* cls ) {
   
   int rc = 0;
   struct stat sb;
   
   char name_buf[ VDEV_NAME_MAX + 1 ];
   
   vdev_basename( fp, name_buf );
   
   if( strcmp(name_buf, ".") == 0 || strcmp(name_buf, "..") == 0 ) {
      // skip 
      return 0;
   }
   
   // is this a directory?
   rc = stat( fp, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_warn("stat('%s') rc = %d\n", fp, rc );
      return rc;
   }
   
   if( S_ISDIR( sb.st_mode ) ) {
      
      // blow away children too 
      rc = vdev_load_all( fp, vdev_device_remove_metadata_file, NULL );
      if( rc != 0 ) {
         
         vdev_warn("removing '%s' rc = %d\n", fp, rc );
         return rc;
      }
      
      // blow away dir 
      rc = rmdir( fp );
   }
   else {
      
      // regular file
      rc = unlink( fp );
   }
   
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_warn("unlink('%s') rc = %d\n", fp, rc );
      return rc;
   }
   
   return rc;
}

// remove extra metadata (i.e. vdev and OS parameters) for a deivce node 
// return 0 on success
// return negative on error 
static int vdev_device_remove_metadata( struct vdev_device_request* req ) {
   
   int rc = 0;
   
   char* base_dir = NULL;
   char metadata_dir[ PATH_MAX + 1 ];
   
   // NOTE: req->path is guaranteed to be <= 256 characters
   sprintf( metadata_dir, VDEV_METADATA_PREFIX "/dev/%s", req->path );
   
   base_dir = vdev_fullpath( req->state->mountpoint, metadata_dir, NULL );
   if( base_dir == NULL ) {
      
      return -ENOMEM;
   }
   
   // remove everything in this directory 
   rc = vdev_load_all( base_dir, vdev_device_remove_metadata_file, NULL );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_load_all('%s') rc = %d\n", base_dir, rc );
   }
   
   // remove the directory itself 
   rc = rmdir( base_dir );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_warn("rmdir('%s') rc = %d\n", base_dir, rc );
   }
   
   free( base_dir );
   return rc;
}


// do we have metadata logged for a device?
// return 0 on success
// return negative on error 
static int vdev_device_has_metadata( struct vdev_device_request* req ) {
   
   int rc = 0;
   struct stat sb;
   char* md_fp = vdev_device_metadata_fullpath( req->state->mountpoint, req->renamed_path );
   if( md_fp == NULL ) {
      
      rc = -ENOMEM;
   }
   else {
      
      rc = stat( md_fp, &sb );

      if( rc != 0 ) {
         rc = -errno;
      }
      
      free( md_fp );
   }
   
   return rc;
}


// create all directories leading up to a device 
// return 0 on success
// return negative on error 
static int vdev_device_mkdirs( struct vdev_device_request* req, char** dev_fullpath ) {
   
   int rc = 0;
   char* fp = NULL;
   char* fp_dir = NULL;
   
   fp = vdev_fullpath( req->state->mountpoint, req->renamed_path, NULL );
   if( fp == NULL ) {
      
      rc = -ENOMEM;
      return rc;
   }
   
   if( strchr(req->renamed_path, '/') != NULL ) {
      
      fp_dir = vdev_dirname( fp, NULL );
      if( fp_dir == NULL ) {
         
         rc = -ENOMEM;
         free( fp );
         return rc;
      }
   
      // make sure the directories leading to this path exist
      rc = vdev_mkdirs( fp_dir, strlen(req->state->mountpoint), 0755 );
      if( rc != 0 ) {
      
         vdev_error("vdev_mkdirs('%s') rc = %d\n", fp_dir, rc );
      }
      
      free( fp_dir );
   }
   
   *dev_fullpath = fp;
   
   return rc;
}


// handler to add a device
// rename the device, and if it succeeds, mknod the device (if it exists), 
// return 0 on success, masking failure to write metadata or failure to run a specific command.
// return -ENOMEM on OOM 
// return -EACCES if we do not have enough privileges to create the device node 
// return -EEXIST if the device node already exists
int vdev_device_add( struct vdev_device_request* req ) {
   
   int rc = 0;
   int do_mknod = 1;            // if 1, issue mknod.  Otherwise, check to see if the device exists by checking for metadata.
   int device_exists = 0;       // if 1, the device already exists.  only run commands with the if_exists directive set to "run"
   
   // do the rename, possibly generating it
   rc = vdev_action_create_path( req, req->state->acts, req->state->num_acts, &req->renamed_path );
   if( rc != 0 ) {
      
      vdev_error("vdev_action_create_path('%s') rc = %d\n", req->path, rc);
      
      // done with this request
      vdev_device_request_free( req );
      free( req );
   
      return rc;
   }
   
   if( req->renamed_path == NULL ) {
      
      // base path becomes renamed path (trivial rename)
      req->renamed_path = vdev_strdup_or_null( req->path );
      if( req->renamed_path == NULL && req->path != NULL ) {
         
         // done with this request
         vdev_device_request_free( req );
         free( req );
      
         return -ENOMEM;
      }
   }
   
   vdev_debug("ADD device: type '%s' at '%s' ('%s' %d:%d)\n", (S_ISBLK(req->mode) ? "block" : S_ISCHR(req->mode) ? "char" : "unknown"), req->renamed_path, req->path, major(req->dev), minor(req->dev) );
   
   if( req->renamed_path != NULL ) {
      
      // device has a name (i.e. something for us to add)?
      if( strcmp( req->renamed_path, VDEV_DEVICE_PATH_UNKNOWN ) != 0 ) {
         
         // device has major/minor/mode?
         if( req->dev != 0 && req->mode != 0 ) {
            
            char* fp = NULL;       // full path to the device 
            
            rc = vdev_device_mkdirs( req, &fp );
            if( rc != 0 ) {
               
               vdev_error("vdev_device_mkdirs('%s/%s') rc = %d\n", req->state->mountpoint, req->renamed_path, rc );
               
               // done with this request 
               vdev_device_request_free( req );
               free( req );
               return rc;
            }
            
            // do we need to make the device?
            if( vdev_config_has_OS_quirk( req->state->config->OS_quirks, VDEV_OS_QUIRK_DEVICE_EXISTS ) ) {
               
               // nope, but did we process it already?
               if( vdev_device_has_metadata( req ) ) {
                  
                  // this device already exists, insofar as 
                  // we (or some other device manager)
                  // have already processed it.
                  device_exists = 1;
               }
            }
            
            else {
               
               if( !req->exists ) {
               
                  // file is not expected to exist
                  rc = mknod( fp, req->mode | req->state->config->default_mode, req->dev );
               }
               else {
                  
                  rc = 0;
               }
               
               if( rc != 0 ) {
                  
                  rc = -errno;
                  if( rc == -EEXIST ) {
                     
                     // device already exists, insofar as 
                     // we (or some other device manager)
                     // have already processed it.
                     device_exists = 1;
                     rc = 0;
                  }
               }
            }
            
            free( fp );
         }
         
         // no major/minor/mode
         else {
         
            // not creating a device file, but a device-add event nevertheless.
            // creating or updating?
            rc = vdev_device_has_metadata( req );
            if( rc != 0 ) {
               
               if( rc == -ENOENT ) {
                  
                  // add metadata, since none exists!
                  rc = 0;
               }
            }
            else {
               
               // metadata already present, meaning
               // that this device has been processed
               rc = 0;
               device_exists = 1;
            }
         }
         
         if( rc != 0 ) {
            
            // some mknod or metadata I/O error occurred
            vdev_error("Add device '%s/%s', rc = %d\n", req->state->mountpoint, req->renamed_path, rc );
         }
            
         else {

            // put/update metadata 
            rc = vdev_device_put_metadata( req );
            
            if( rc != 0 ) {
               
               vdev_error("vdev_device_put_metadata('%s/%s') rc = %d\n", req->state->mountpoint, req->renamed_path, rc );
            }
         }
      }
      
      if( rc == 0 ) {

         // no problems yet.  call all ADD actions 
         rc = vdev_action_run_commands( req, req->state->acts, req->state->num_acts, device_exists );
         if( rc != 0 ) {
            
            vdev_error("vdev_action_run_commands(ADD %s, dev=(%u, %u)) rc = %d\n", req->renamed_path, major(req->dev), minor(req->dev), rc );
         }  
      }
   }
   
   // done with this request
   vdev_device_request_free( req );
   free( req );
   
   return 0;
}

// workqueue call to vdev_device_add
static int vdev_device_add_wq( struct vdev_wreq* wreq, void* cls ) {
   
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   return vdev_device_add( req );
}


// handler to remove a device (unlink)
// return 0 on success.  This masks failure to unlink or clean up metadata or a failed action.
// return -ENOMEM on OOM 
int vdev_device_remove( struct vdev_device_request* req ) {
   
   int rc = 0;
   
   // do the rename, possibly generating it
   rc = vdev_action_create_path( req, req->state->acts, req->state->num_acts, &req->renamed_path );
   if( rc != 0 ) {
      
      vdev_error("vdev_action_create_path('%s') rc = %d\n", req->path, rc);
      
      // done with this request
      vdev_device_request_free( req );
      free( req );
   
      return rc;
   }
   
   if( req->renamed_path == NULL ) {
      
      req->renamed_path = vdev_strdup_or_null( req->path );
      if( req->renamed_path == NULL && req->path == NULL ) {
         
         // done with this request
         vdev_device_request_free( req );
         free( req );
      
         return -ENOMEM;
      }
   }
   
   vdev_info("REMOVE device: type '%s' at '%s' ('%s' %d:%d)\n", (S_ISBLK(req->mode) ? "block" : S_ISCHR(req->mode) ? "char" : "unknown"), req->renamed_path, req->path, major(req->dev), minor(req->dev) );
      
   if( req->renamed_path != NULL ) {
      
      // call all REMOVE actions
      rc = vdev_action_run_commands( req, req->state->acts, req->state->num_acts, true );
      if( rc != 0 ) {
         
         vdev_error("vdev_action_run_all(REMOVE %s) rc = %d\n", req->renamed_path, rc );
         rc = 0;
      }
      
      if( strcmp( req->renamed_path, VDEV_DEVICE_PATH_UNKNOWN ) != 0 ) {
         
         // known path
         // only remove files from /dev if this is a device, and we created it
         if( req->dev != 0 && req->mode != 0 && !vdev_config_has_OS_quirk( req->state->config->OS_quirks, VDEV_OS_QUIRK_DEVICE_EXISTS ) ) {
               
            // remove the data itself, if there is data 
            char* fp = vdev_fullpath( req->state->mountpoint, req->renamed_path, NULL );
            
            rc = unlink( fp );
            if( rc != 0 ) {
               
               rc = -errno;
               
               if( rc != -ENOENT ) {
                  vdev_error("unlink(%s) rc = %d\n", fp, rc );
               }
               
               rc = 0;
            }
               
            // try to clean up directories
            rc = vdev_rmdirs( fp );
            if( rc != 0 && rc != -ENOTEMPTY && rc != -ENOENT ) {
               
               vdev_error("vdev_rmdirs('%s') rc = %d\n", fp, rc );
               rc = 0;
            }
            
            free( fp );
         }
      
         // remove metadata 
         rc = vdev_device_remove_metadata( req );
         
         if( rc != 0 ) {
            
            vdev_warn("unable to clean up metadata for %s\n", req->renamed_path );
            rc = 0;
         }
      }
   }
   
   // done with this request 
   vdev_device_request_free( req );
   free( req );
   
   return rc;
}


// workqueue call to vdev_device_remove 
static int vdev_device_remove_wq( struct vdev_wreq* wreq, void* cls ) {
   
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   return vdev_device_remove( req );
}


// handler to change a device
// Process any rename commands to get the final path, and then update that path's metadata
// Fails if the device does not exist.
// return 0 on success, masking failure to write metadata or failure to run a specific command.
// return -ENOMEM on OOM 
// return -EACCES if we do not have enough privileges to create the device node 
// return -EEXIST if the device node already exists
int vdev_device_change( struct vdev_device_request* req ) {
   
   int rc = 0;
   
   // do the rename, possibly generating it
   rc = vdev_action_create_path( req, req->state->acts, req->state->num_acts, &req->renamed_path );
   if( rc != 0 ) {
      
      vdev_error("vdev_action_create_path('%s') rc = %d\n", req->path, rc);
      
      // done with this request
      vdev_device_request_free( req );
      free( req );
   
      return rc;
   }
   
   if( req->renamed_path == NULL ) {
      
      // base path becomes renamed path (trivial rename)
      req->renamed_path = vdev_strdup_or_null( req->path );
      if( req->renamed_path == NULL && req->path != NULL ) {
         
         // done with this request
         vdev_device_request_free( req );
         free( req );
      
         return -ENOMEM;
      }
   }
   
   vdev_debug("CHANGE device: type '%s' at '%s' ('%s' %d:%d)\n", (S_ISBLK(req->mode) ? "block" : S_ISCHR(req->mode) ? "char" : "unknown"), req->renamed_path, req->path, major(req->dev), minor(req->dev) );
   
   if( req->renamed_path != NULL && vdev_device_has_metadata( req ) ) {
   
      // call all CHANGE actions 
      rc = vdev_action_run_commands( req, req->state->acts, req->state->num_acts, 1 );
      if( rc != 0 ) {
         
         vdev_error("vdev_action_run_commands(ADD %s, dev=(%u, %u)) rc = %d\n", req->renamed_path, major(req->dev), minor(req->dev), rc );
      }  
   }
   
   // done with this request
   vdev_device_request_free( req );
   free( req );
   
   return 0;
}


// workqueue call to vdev_device_change
static int vdev_device_change_wq( struct vdev_wreq* wreq, void* cls ) {
   
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   return vdev_device_change( req );
}


// enqueue a device request
// NOTE: the workqueue takes ownership of the request.  The caller should not free it.
// return 0 on success
// return -EINVAL if the device request is missing required fields 
// return -ENOMEM on OOM 
int vdev_device_request_enqueue( struct vdev_wq* wq, struct vdev_device_request* req ) {
   
   int rc = 0;
   struct vdev_wreq wreq;
   
   memset( &wreq, 0, sizeof(struct vdev_wreq) );
   
   // sanity check 
   rc = vdev_device_request_sanity_check( req );
   if( rc != 0 ) {
      
      vdev_error("Invalid device request (type %d)\n", req->type );
      return -EINVAL;
   }
   
   // which handler?
   switch( req->type ) {
      
      case VDEV_DEVICE_ADD: {
         
         vdev_wreq_init( &wreq, vdev_device_add_wq, req );
         break;
      }
      
      case VDEV_DEVICE_REMOVE: {
         
         vdev_wreq_init( &wreq, vdev_device_remove_wq, req );
         break;
      }
      
      case VDEV_DEVICE_CHANGE: {
         
         vdev_wreq_init( &wreq, vdev_device_change_wq, req );
         break;
      }
      
      default: {
         
         vdev_error("Invalid device request type %d\n", req->type );
         return -EINVAL;
      }  
   }
   
   rc = vdev_wq_add( wq, &wreq );
   if( rc != 0 ) {
      
      vdev_error("vdev_wq_add('%s') rc = %d\n", req->path, rc );
   }
   
   return rc;
}
