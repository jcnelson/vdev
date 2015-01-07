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

#include "workqueue.h"
#include "os/methods.h"
#include "action.h"
#include "vdev.h"
#include "fskit/fskit.h"

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
   req->params = new (nothrow) vdev_device_params_t();
   
   if( req->params == NULL ) {
      
      if( req->path != NULL ) {
         free( req->path );
      }
      
      return -ENOMEM;
   }
   
   return 0;
}


// free a request 
int vdev_device_request_free( struct vdev_device_request* req ) {
   
   if( req->params != NULL ) {
      
      delete req->params;
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
   
   try {
      
      vdev_device_params_t::iterator itr = req->params->find( string(key) );
      if( itr == req->params->end() ) {
         
         (*req->params)[ string(key) ] = string(value);
      }
      else {
         
         return -EEXIST;
      }
   }
   catch ( bad_alloc& ba ) {
      return -ENOMEM;
   }
   
   return 0;
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
      "ADD",
      "REMOVE",
      "ANY",
      "NONE"
   };
   
   if( req < 0 || req > VDEV_DEVICE_ANY ) {
      return req_tbl[3];
   }
   
   return req_tbl[req-1];
}


// mode to const string 
static char const* vdev_device_request_mode_to_string( mode_t mode ) {
   
   static char const* blk_str = "BLOCK";
   static char const* chr_str = "CHAR";
   static char const* none_str = "NONE";
   
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
// return 0 on success 
// return negative on error 
int vdev_device_request_to_env( struct vdev_device_request* req, char*** ret_env, size_t* num_env ) {
   
   // type --> VDEV_ACTION
   // path --> VDEV_PATH
   // dev --> VDEV_MAJOR, VDEV_MINOR
   // mode --> VDEV_MODE
   // params --> VDEV_OS_* 
   // mountpoint --> VDEV_MOUNTPOINT
   // helpers --> VDEV_HELPERS
   
   size_t num_vars = 8 + req->params->size();
   int i = 0;
   int rc = 0;
   char dev_buf[50];
   char* vdev_path = req->renamed_path;
   if( vdev_path == NULL ) {
      vdev_path = req->path;
   }
   
   char** env = VDEV_CALLOC( char*, num_vars + 1 );
   
   if( env == NULL ) {
      return -ENOMEM;
   }
   
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
   
   rc = vdev_device_request_make_env_str( "VDEV_PATH", vdev_path, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   sprintf(dev_buf, "%u", major(req->dev) );
   
   rc = vdev_device_request_make_env_str( "VDEV_MAJOR", dev_buf, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   sprintf(dev_buf, "%u", minor(req->dev) );
   
   rc = vdev_device_request_make_env_str( "VDEV_MINOR", dev_buf, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   rc = vdev_device_request_make_env_str( "VDEV_MODE", vdev_device_request_mode_to_string( req->mode ), &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   rc = vdev_device_request_make_env_str( "VDEV_HELPERS", req->state->config->helpers_dir, &env[i] );
   if( rc != 0 ) {
      
      VDEV_FREE_LIST( env );
      return rc;
   }
   
   i++;
   
   // add all OS-specific parameters 
   for( vdev_device_params_t::iterator itr = req->params->begin(); itr != req->params->end(); itr++ ) {
      
      char const* param_key = itr->first.c_str();
      char const* param_value = itr->second.c_str();
      
      // prepend with "VDEV_OS_"
      char* varname = VDEV_CALLOC( char, strlen(param_key) + 1 + strlen("VDEV_OS_") );
      
      if( varname == NULL ) {
         
         VDEV_FREE_LIST( env );
         return rc;
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
   
   *ret_env = env;
   *num_env = i;
   
   return 0;
}


// set the path 
int vdev_device_request_set_path( struct vdev_device_request* req, char const* path ) {
   
   if( req->path != NULL ) {
      free( req->path );
   }
   
   req->path = vdev_strdup_or_null( path );
   
   return 0;
}

// set the device 
int vdev_device_request_set_dev( struct vdev_device_request* req, dev_t dev ) {
   
   req->dev = dev;
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
   
   if( req->params == NULL ) {
      
      vdev_error("request %p has no params\n", req );
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

// record extra metadata (i.e. vdev and OS parameters) onto a device node, in the form of xattrs.
// return 0 on success
// return negative on error
static int vdev_device_add_metadata( struct vdev_device_request* req, char const* fp ) {

   int rc = 0;
   
   // save all OS-specific attributes to this, via setxattr 
   for( vdev_device_params_t::iterator itr = req->params->begin(); itr != req->params->end(); itr++ ) {
      
      char const* param_name = itr->first.c_str();
      char const* param_value = itr->second.c_str();
      
      char* vdev_param_name = VDEV_CALLOC( char, strlen(VDEV_OS_XATTR_NAMESPACE) + 1 + strlen(param_name) + 1 );
      
      if( vdev_param_name == NULL ) {
         
         rc = -ENOMEM;
         break;
      }
      
      sprintf(vdev_param_name, "%s%s", VDEV_OS_XATTR_NAMESPACE, param_name );
      
      rc = setxattr( fp, vdev_param_name, param_value, strlen(param_value) + 1, 0 );
      
      if( rc != 0 ) {
         
         rc = -errno;
         vdev_error("setxattr('%s' = '%s') rc = %d\n", vdev_param_name, param_value, rc );
         
         free( vdev_param_name );
         
         break;
      }
      
      free( vdev_param_name );
   }
   
   return rc;
}


// handler to add a device
static int vdev_device_add( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
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
   }
   
   vdev_debug("ADD device %p type '%s' at '%s' (%d:%d, original path='%s')\n", req, (S_ISBLK(req->mode) ? "block" : S_ISCHR(req->mode) ? "char" : "unknown"), req->renamed_path, major(req->dev), minor(req->dev), req->path );
   
   if( req->renamed_path != NULL && req->dev != 0 && req->mode != 0 ) {
      
      // make the device file
      char* fp = NULL;
      char* fp_dir = NULL;
      
      fp = fskit_fullpath( req->state->mountpoint, req->renamed_path, NULL );
      if( fp == NULL ) {
         
         rc = -ENOMEM;
               
         // done with this request
         vdev_device_request_free( req );
         free( req );
         return rc;
      }
      
      fp_dir = fskit_dirname( fp, NULL );
      if( fp_dir == NULL ) {
         
         rc = -ENOMEM;
         
         // done with this request
         vdev_device_request_free( req );
         free( req );
         free( fp );
         return rc;
      }
      
      if( strchr(req->renamed_path, '/') != NULL ) {
      
         // make sure the directories leading to this path exist
         // 0777 is okay, since we'll equivocate about them
         rc = vdev_mkdirs( fp_dir, strlen(req->state->mountpoint), 0777 );
         if( rc != 0 ) {
         
            vdev_error("vdev_mkdirs('%s') rc = %d\n", fp_dir, rc );
         }
      }
      
      free( fp_dir );
      
      if( rc == 0 ) {
         
         // call into our mknod() via FUSE (waking up inotify/kqueue listeners)
         // it's okay to use 0777, since we'll equivocate about it later
         rc = mknod( fp, req->mode | 0777, req->dev );
         if( rc != 0 ) {
            
            rc = -errno;
            vdev_error("mknod('%s', dev=(%u, %u)) rc = %d\n", fp, major(req->dev), minor(req->dev), rc );
         }
         else {
            
            // save metadata 
            rc = vdev_device_add_metadata( req, fp );
            
            if( rc != 0 ) {
               
               vdev_error("vdev_device_add_metadata('%s') rc = %d\n", fp, rc );
               
               vdev_device_request_free( req );
               free( req );
               free( fp );
               return rc;
            }
         }  
      }
      
      free( fp );
   }
   
   if( rc == 0 ) {

      // success!  call all ADD actions
      rc = vdev_action_run_commands( req, req->state->acts, req->state->num_acts );
      if( rc != 0 ) {
         
         vdev_error("vdev_action_run_commands(ADD %p, dev=(%u, %u)) rc = %d\n", req, major(req->dev), minor(req->dev), rc );
      }
   }
   
   // done with this request
   vdev_device_request_free( req );
   free( req );
   
   return rc;
}

// handler to remove a device (unlink)
// need to fork to do this
static int vdev_device_remove( struct fskit_wreq* wreq, void* cls ) {
   
   int rc = 0;
   struct vdev_device_request* req = (struct vdev_device_request*)cls;
   
   char* renamed_path = NULL;
      
   // do the rename, possibly generating it
   rc = vdev_action_create_path( req, req->state->acts, req->state->num_acts, &renamed_path );
   if( rc != 0 ) {
      
      vdev_error("vdev_action_create_path('%s') rc = %d\n", req->path, rc);
      
      // done with this request
      vdev_device_request_free( req );
      free( req );
   
      return rc;
   }
   
   if( renamed_path == NULL ) {
      renamed_path = vdev_strdup_or_null( req->path );
   }
   
   vdev_debug("REMOVE device %p type '%s' at '%s' (%d:%d) (original path='%s')\n", req, (S_ISBLK(req->mode) ? "block" : S_ISCHR(req->mode) ? "char" : "unknown"), renamed_path, major(req->dev), minor(req->dev), req->path );
      
   if( renamed_path != NULL ) {
      
      char* fp = fskit_fullpath( req->state->mountpoint, req->path, NULL );
   
      // call into our unlink() via FUSE (waking up inotify/kqueue listeners)
      rc = unlink( fp );
      if( rc != 0 ) {
         
         rc = -errno;
         
         if( rc != -ENOENT ) {
            vdev_error("unlink(%s) rc = %d\n", fp, rc );
            
            vdev_device_request_free( req );
            free( req );
            free( fp );
            
            return rc;
         }
         else {
            
            rc = 0;
         }
      }
      
      // try to clean up directories 
      rc = vdev_rmdirs( fp );
      if( rc != 0 && rc != -ENOTEMPTY ) {
         
         vdev_error("vdev_rmdirs('%s') rc = %d\n", fp );
         
         vdev_device_request_free( req );
         free( req );
         free( fp );
         
         return rc;
      }
      
      free( fp );
   }
      
   if( rc == 0 ) {
      
      // call all REMOVE actions
      rc = vdev_action_run_commands( req, req->state->acts, req->state->num_acts );
      if( rc != 0 ) {
         
         vdev_error("vdev_action_run_all(REMOVE %p) rc = %d\n", req, rc );
      }
   }
   
   if( renamed_path != NULL ) {
      free( renamed_path );
   }
   
   // done with this request 
   vdev_device_request_free( req );
   free( req );
   
   return rc;
}


// enqueue a device request
int vdev_device_request_enqueue( struct fskit_wq* wq, struct vdev_device_request* req ) {
   
   int rc = 0;
   struct fskit_wreq wreq;
   
   // sanity check 
   rc = vdev_device_request_sanity_check( req );
   if( rc != 0 ) {
      
      vdev_error("Invalid device request (type %d)\n", req->type );
      return -EINVAL;
   }
   
   // which handler?
   switch( req->type ) {
      
      case VDEV_DEVICE_ADD: {
         
         fskit_wreq_init( &wreq, vdev_device_add, req, 0 );
         break;
      }
      
      case VDEV_DEVICE_REMOVE: {
         
         fskit_wreq_init( &wreq, vdev_device_remove, req, 0 );
         break;
      }
      
      default: {
         
         vdev_error("Invalid device request type %d\n", req->type );
         return -EINVAL;
      }  
   }
   
   rc = fskit_wq_add( wq, &wreq );
   if( rc != 0 ) {
      
      vdev_error("fskit_wq_add('%s') rc = %d\n", req->path, rc );
   }
   
   return rc;
}
