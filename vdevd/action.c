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
#include "action.h"
#include "libvdev/match.h"

#define INI_MAX_LINE 4096
#define INI_STOP_ON_FIRST_ERROR 1

#include "libvdev/ini.h"

SGLIB_DEFINE_VECTOR_PROTOTYPES( vdev_action );
SGLIB_DEFINE_VECTOR_FUNCTIONS( vdev_action );

// initialize an action 
// return 0 on success 
// return -ENOMEM on OOM 
int vdev_action_init( struct vdev_action* act, vdev_device_request_t trigger, char* path, char* command, bool async ) {
   
   int rc = 0;
   memset( act, 0, sizeof(struct vdev_action) );
   
   act->trigger = trigger;
   act->path = vdev_strdup_or_null( path );
   act->command = vdev_strdup_or_null( command );
   act->async = async;
   
   if( act->path != NULL ) {
      
      rc = vdev_match_regex_init( &act->path_regex, path );
      
      if( rc != 0 ) {
         
         vdev_error("vdev_match_regex_init('%s') rc = %d n", path, rc );
         vdev_action_free( act );
         return rc;
      }
   }
   
   return 0;
}

// add a device parameter to match on 
// return 0 on success 
// return -ENOMEM on OOM  
// return -EEXIST if name already exists 
int vdev_action_add_param( struct vdev_action* act, char const* name, char const* value ) {
   return vdev_params_add( &act->dev_params, name, value );
}

// free an action 
// always succeeds
int vdev_action_free( struct vdev_action* act ) {
   
   if( act->command != NULL ) {
      
      free( act->command );
      act->command = NULL;
   }
   
   if( act->rename_command != NULL ) {
      
      free( act->rename_command );
      act->rename_command = NULL;
   }
   
   if( act->dev_params != NULL ) {
      
      vdev_params_free( act->dev_params );
      act->dev_params = NULL;
   }
   
   if( act->path != NULL ) {
      
      free( act->path );
      act->path = NULL;
      
      
      regfree( &act->path_regex );
   }
   
   if( act->type != NULL ) {
      
      free( act->type );
      act->type = NULL;
   }
   
   return 0;
}


// convert an action into a device request 
// return the constant associated with the type on success
// return VDEV_DEVICE_INVALID if the type is not recognized
vdev_device_request_t vdev_action_parse_trigger( char const* type ) {
   
   if( strcmp(type, VDEV_ACTION_EVENT_ADD) == 0 ) {
      return VDEV_DEVICE_ADD;
   }
   else if( strcmp(type, VDEV_ACTION_EVENT_REMOVE) == 0 ) {
      return VDEV_DEVICE_REMOVE;
   }
   else if( strcmp(type, VDEV_ACTION_EVENT_CHANGE) == 0 ) {
      return VDEV_DEVICE_CHANGE;
   }
   else if( strcmp(type, VDEV_ACTION_EVENT_ANY) == 0 ) {
      return VDEV_DEVICE_ANY;
   }
   
   return VDEV_DEVICE_INVALID;
}

// parse an action from an ini file 
// return 1 for parsed
// return 0 for not parsed
static int vdev_action_ini_parser( void* userdata, char const* section, char const* name, char const* value ) {
   
   struct vdev_action* act = (struct vdev_action*)userdata;
   int rc = 0;
   
   if( strcmp(section, VDEV_ACTION_NAME) == 0 ) {
      
      if( strcmp(name, VDEV_ACTION_NAME_PATH) == 0 ) {
         // path?
         if( act->path != NULL ) {
            
            free( act->path );
         }
         
         act->path = vdev_strdup_or_null( value );
         
         if( act->path == NULL ) {
            // EOM
            return 0;
         } 
         else {
            rc = vdev_match_regex_init( &act->path_regex, act->path );
            
            if( rc != 0 ) {
               
               vdev_error("vdev_match_regex_init('%s') rc = %d n", act->path, rc );
               return 0;
            }
         }
         return 1;
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_RENAME) == 0 ) {
         
         // rename command 
         if( act->rename_command != NULL ) {
            
            free( act->rename_command );
         }
         
         act->rename_command = vdev_strdup_or_null( value );
         return 1;
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_EVENT) == 0 ) {
         
         // event?
         act->trigger = vdev_action_parse_trigger( value );
         
         if( act->trigger == VDEV_DEVICE_INVALID ) {
            
            fprintf(stderr, "Invalid event type '%s' n", value);
            return 0;
         }
         else {
            return 1;
         }
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_SHELL) == 0 ) {
         
         // shell command 
         if( act->command != NULL ) {
            
            free( act->command );
         }
         
         act->command = vdev_strdup_or_null( value );
         return 1;
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_ASYNC) == 0 ) {
         
         // async?
         if( strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0 ) {
            
            act->async = true;
            return 1;
         }
         else if( strcasecmp(value, "false") == 0 || strcmp(value, "0") == 0 ) {
            
            act->async = false;
            return 1;
         }
         else {
            
            fprintf(stderr, "Invalid async value '%s' n", value);
            return 0;
         }
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_TYPE) == 0 ) {
         
         // remember this
         act->has_type = true;
         act->type = vdev_strdup_or_null( value );
         return 1;
      }
         
      if( strncmp(name, VDEV_ACTION_NAME_OS_PREFIX, strlen(VDEV_ACTION_NAME_OS_PREFIX)) == 0 ) {
         
         // OS-specific param 
         rc = vdev_action_add_param( act, name + strlen(VDEV_ACTION_NAME_OS_PREFIX), value );
         
         if( rc == 0 ) {
            return 1;
         }
         else {
            vdev_error("vdev_action_add_param( '%s', '%s' ) rc = %d n", name, value, rc );
            return 0;
         }
      }
      
      fprintf(stderr, "Unknown field '%s' in section '%s' n", name, section );
      return 0;
   }
   
   fprintf(stderr, "Unknown section '%s' n", section);
   return 0;
}

// sanity check an action 
// return 0 on success
// return -EINVAL if invalid
int vdev_action_sanity_check( struct vdev_action* act ) {
   
   int rc = 0;
   
   if( act->command == NULL && act->rename_command == NULL ) {
      
      fprintf(stderr, "Action is missing 'command=' or 'rename_command=' n");
      rc = -EINVAL;
   }
   
   if( act->trigger == VDEV_DEVICE_INVALID ) {
      
      fprintf(stderr, "Action is missing 'event=' n");
      rc = -EINVAL;
   }
   
   return rc;
}

// load an action from a path
// return -ENOMEM if OOM
// return -errno on failure to open or read the file
// return -EINVAL if the file could not be parsed
int vdev_action_load( char const* path, struct vdev_action* act ) {
   
   int rc = 0;
   FILE* f = NULL;
   
   rc = vdev_action_init( act, VDEV_DEVICE_INVALID, NULL, NULL, false );
   if( rc != 0 ) {
      
      vdev_error("vdev_action_init('%s') rc = %d n", path, rc );
      return rc;
   }
   
   f = fopen( path, "r" );
   if( f == NULL ) {
      
      vdev_action_free( act );
      rc = -errno;
      
      return rc;
   }
   
   rc = vdev_action_load_file( f, act );
   
   fclose( f );
   
   if( rc == -EINVAL ) {
      
      vdev_action_free( act );
      vdev_error("Invalid action '%s' n", path );
   }
   
   return rc;
}


// load an action from a file
// return 0 on success
// return -errno on failure to open, read, parse, or close
// return -EINVAL if the loaded action fails our sanity tests
int vdev_action_load_file( FILE* f, struct vdev_action* act ) {
   
   int rc = 0;
   
   rc = ini_parse_file( f, vdev_action_ini_parser, act );
   if( rc != 0 ) {
      vdev_error("ini_parse_file(action) rc = %d n", rc );
   }
   else {
      rc = vdev_action_sanity_check( act );
   }
   
   return rc;
}

// free a C-style list of actions (including the list itself)
// always succeeds
int vdev_action_free_all( struct vdev_action* act_list, size_t num_acts ) {
   
   int rc = 0;
   
   for( unsigned int i = 0; i < num_acts; i++ ) {
      
      rc = vdev_action_free( &act_list[i] );
      if( rc != 0 ) {
         return rc;
      }
   }
   
   free( act_list );
   
   return rc;
}


// action loader 
// return 0 on success
// return -errno on failure to stat(2)
// return -EINVAL if the file is invalid 
// return -ENOMEM if OOM 
int vdev_action_loader( char const* path, void* cls ) {
   
   int rc = 0;
   struct vdev_action act;
   struct stat sb;
   
   struct sglib_vdev_action_vector* acts = (struct sglib_vdev_action_vector*)cls;
   
   // skip if not a regular file 
   rc = stat( path, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("stat(%s) rc = %d n", path, rc );
      return rc;
   }
   
   if( !S_ISREG( sb.st_mode ) ) {
      
      return 0;
   }
   
   vdev_debug("Load Action %s\n", path );
   
   memset( &act, 0, sizeof(struct vdev_action) );
   
   rc = vdev_action_load( path, &act );
   if( rc != 0 ) {
      
      vdev_error("vdev_acl_load(%s) rc = %d\n", path, rc );
      return rc;
   }
   
   // save this action 
   rc = sglib_vdev_action_vector_push_back( acts, act );
   if( rc != 0 ) {
      
      // OOM
      vdev_action_free( &act );
      return rc;
   }
   
   return 0;
}

// free a vector of actions 
// always succeeds
static int vdev_action_vector_free( struct sglib_vdev_action_vector* acts ) {
   
   for( unsigned long i = 0; i < sglib_vdev_action_vector_size( acts ); i++ ) {
      
      struct vdev_action* act = sglib_vdev_action_vector_at_ref( acts, i );
      
      vdev_action_free( act );
   }
   
   sglib_vdev_action_vector_clear( acts );
   return 0;
}


// load all actions in a directory
// return 0 on success
// return -ENOMEM if OOM 
// return -EINVAL if at least one action file failed to load due to a sanity test failure 
// return -errno if at least one action file failed to load due to an I/O error
int vdev_action_load_all( char const* dir_path, struct vdev_action** ret_acts, size_t* ret_num_acts ) {
   
   int rc = 0;
   struct sglib_vdev_action_vector acts;
   
   sglib_vdev_action_vector_init( &acts );
   
   rc = vdev_load_all( dir_path, vdev_action_loader, &acts );
   
   if( rc != 0 ) {
      
      vdev_action_vector_free( &acts );
      sglib_vdev_action_vector_free( &acts );
      
      return rc;
   }
   else {
         
      if( sglib_vdev_action_vector_size( &acts ) == 0 ) {
         
         // nothing 
         *ret_acts = NULL;
         *ret_num_acts = 0;
      }
      else {
      
         // extract values
         unsigned long len_acts = 0;
         
         sglib_vdev_action_vector_yoink( &acts, ret_acts, &len_acts );
         
         *ret_num_acts = len_acts;
      }
   }
   
   return 0;
}


// carry out a command, synchronously, using an environment given by vreq.
// return the exit status on success (non-negative)
// return negative on error
int vdev_action_run_sync( struct vdev_device_request* vreq, char const* command, char** output, size_t max_output ) {
   
   int rc = 0;
   int exit_status = 0;
   char** req_env = NULL;
   size_t num_env = 0;
   
   // convert to environment variables
   rc = vdev_device_request_to_env( vreq, &req_env, &num_env );
   if( rc != 0 ) {
      
      vdev_error("vdev_device_request_to_env(%s) rc = %d\n", vreq->path, rc );
      return rc;
   }
   
   vdev_debug("run command: '%s'\n", command );
   
   for( unsigned int i = 0; i < num_env; i++ ) {
      vdev_debug("command env: '%s'\n", req_env[i] );
   }
   
   rc = vdev_subprocess( command, req_env, output, max_output, &exit_status );
   
   vdev_debug("exit status %d\n", exit_status );
   
   if( output != NULL && *output != NULL ) {
      
      vdev_debug("command output: '%s'\n", *output );
   }
   
   VDEV_FREE_LIST( req_env );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_subprocess('%s') rc = %d\n", command, rc );
      
      return rc;
   }
   
   if( exit_status != 0 ) {
      
      vdev_error("vdev_subprocess('%s') exit status = %d\n", command, exit_status );
   }
   
   return exit_status;
}


// carry out an action, asynchronously.
// return 0 if we were able to fork 
// return -errno on failure to fork
int vdev_action_run_async( struct vdev_device_request* req, char const* command ) {
   
   int rc = 0;
   pid_t pid = 0;
   int max_fd = 0;
   
   pid = fork();
   
   if( pid == 0 ) {
      
      // child 
      max_fd = sysconf(_SC_OPEN_MAX);
      
      // close everything 
      for( int i = 0; i < max_fd; i++ ) {
         close( i );
      }
      
      clearenv();
      
      // generate the environment 
      char** env = NULL;
      size_t num_env = 0;
      
      rc = vdev_device_request_to_env( req, &env, &num_env );
      if( rc != 0 ) {
         
         vdev_error("vdev_device_request_to_env('%s') rc = %d\n", req->path, rc );
         exit(1);
      }
      
      // run the command 
      execle( "/bin/sh", "sh", "-c", command, (char*)0, env );
      
      // keep gcc happy 
      return 0;
   }
   else if( pid > 0 ) {
      
      // parent; succeeded
      return 0;
   }
   else {
      
      // error 
      rc = -errno;
      vdev_error("fork() rc = %d\n", rc);
      
      return rc;
   }
}


// match device request against action
// return 1 if match
// return 0 if not match 
int vdev_action_match( struct vdev_device_request* vreq, struct vdev_action* act ) {
   
   int rc = 0;
   
   // action match?
   if( act->trigger != vreq->type && act->trigger != VDEV_DEVICE_ANY ) {
      return 0;
   }
   
   // path match?
   if( act->path != NULL ) {
      
      rc = vdev_match_regex( vreq->path, &act->path_regex );
      if( rc == 0 ) {
         
         // no match 
         return 0;
      }
      if( rc < 0 ) {
         
         // some error
         return rc;
      }
   }
   
   // type match?
   if( act->has_type ) {
      
      if( !S_ISBLK( vreq->mode ) && !S_ISCHR( vreq->mode ) ) {
         // device has no type 
         return 0;
      }
      
      if( S_ISBLK( vreq->mode ) && strcasecmp( act->type, "block" ) != 0 ) {
         
         // not a block device
         return 0;
      }
      if( S_ISCHR( vreq->mode ) && strcasecmp( act->type, "char" ) != 0 ) {
         
         // not a char device
         return 0;
      }
   }
   
   // OS parameter match?
   if( act->dev_params != NULL ) {
      
      struct sglib_vdev_params_iterator itr;
      struct vdev_param_t* dp = NULL;
      
      for( dp = sglib_vdev_params_it_init_inorder( &itr, act->dev_params ); dp != NULL; dp = sglib_vdev_params_it_next( &itr ) ) {
      
         struct vdev_param_t* match = sglib_vdev_params_find_member( vreq->params, dp );
         
         if( match != NULL ) {
            
            // vreq has this parameter
            char const* vreq_param_value = match->value;
            char const* act_param_value = dp->value;
            
            // if the action has no value (value of length 0), then it matches any vreq value 
            if( act_param_value == NULL || strlen( act_param_value ) == 0 ) {
               
               continue;
            }
            
            // otherwise, compare the values
            if( strcmp( vreq_param_value, act_param_value ) != 0 ) {
               
               // values don't match 
               return 0;
            }
         }
         else {
            
            // vreq does not have this parameter, so no match 
            return 0;
         }
      }
   }
   
   // match!
   return 1;
}


// find the next action in a list of actions to run, given the path 
// return the index into acts of the next match, if found 
// return num_acts if not found 
// return negative on error
int vdev_action_find_next( struct vdev_device_request* vreq, struct vdev_action* acts, size_t num_acts ) {
   
   int rc = 0;
   int i = 0;
   
   for( i = 0; (unsigned)i < num_acts; i++ ) {
      
      rc = vdev_action_match( vreq, &acts[i] );
      
      if( rc > 0 ) {
         return i;
      }
      else if( rc < 0 ) {
         return rc;
      }
   }
   
   return num_acts;
}


// find the path to create for the given device request, but in a fail-fast manner (i.e. return on first error)
// *path will be filled in with path to the device node, relative to the mountpoint.  *path must be NULL on call.
// return 0 on success
// return -EINVAL if *path is not NULL, or if we failed to match the vreq against our actions due to a regex error 
// return -ENODATA if *path has zero-length
int vdev_action_create_path( struct vdev_device_request* vreq, struct vdev_action* acts, size_t num_acts, char** path ) {
   
   int rc = 0;
   int act_offset = 0;
   int i = 0;
   char* new_path = NULL;
   
   if( *path != NULL ) {
      return -EINVAL;
   }
   
   while( act_offset < (signed)num_acts ) {
      
      rc = 0;
      
      // skip this action if there is no rename command 
      if( acts[act_offset].rename_command == NULL ) {
         act_offset++;
         continue;
      }
      
      // find the next action that matches this path
      rc = vdev_action_find_next( vreq, acts + act_offset, num_acts - act_offset );
      
      if( rc == (signed)(num_acts - act_offset) ) {
         
         // not found 
         rc = 0;
         break;
      }
      else if( rc < 0 ) {
         
         // error
         vdev_error("vdev_action_find_next(%s, offset = %d) rc = %d\n", vreq->path, act_offset, rc );
         break;
      }
      else {
         
         // matched! advance offset to next action
         i = act_offset + rc;
         act_offset += rc + 1;
         rc = 0;
         
         if( acts[i].rename_command == NULL ) {
            continue;
         }
         
         // generate the new name
         rc = vdev_action_run_sync( vreq, acts[i].rename_command, &new_path, PATH_MAX + 1 );
         if( rc < 0 ) {
            
            vdev_error("vdev_action_run_sync('%s') rc = %d\n", acts[i].rename_command, rc );
            break;
         }
         else {
            
            if( *path != NULL ) {
               free( *path );
            }
            
            *path = new_path;
            new_path = NULL;
         }
      }
   }
   
   if( *path != NULL && strlen(*path) == 0 ) {
      
      // if this is "UNKNOWN", then just reset to "UNKNOWN"
      if( strcmp(vreq->path, VDEV_DEVICE_PATH_UNKNOWN) == 0 ) {
         
         free( *path );
         *path = vdev_strdup_or_null( VDEV_DEVICE_PATH_UNKNOWN );
      }
      else {
         
         vdev_error("Zero-length path generated for '%s'\n", vreq->path );
         
         free( *path );
         *path = NULL;
         
         rc = -ENODATA;
      }
   }
   
   return rc;
}


// run all actions for a device, sequentially, in lexographic order.
// commands are executed optimistically--even if one fails, the other subsequent ones will be attempted
// return 0 on success
// return negative on failure
int vdev_action_run_commands( struct vdev_device_request* vreq, struct vdev_action* acts, size_t num_acts ) {
   
   int rc = 0;
   int act_offset = 0;
   int i = 0;
   char const* method = NULL;
   
   while( act_offset < (signed)num_acts ) {
      
      // skip this action if there is no command 
      if( acts[act_offset].command == NULL ) {
         act_offset++;
         continue;
      }
      
      // find the next action that matches this path 
      rc = vdev_action_find_next( vreq, acts + act_offset, num_acts - act_offset );
      
      if( rc == (signed)(num_acts - act_offset) ) {
         
         // not found 
         rc = 0;
         break;
      }
      else if( rc < 0 ) {
         
         vdev_error("vdev_action_find_next(%s, offset = %d) rc = %d\n", vreq->path, act_offset, rc );
         break;
      }
      else {
         
         // matched! advance offset to next action
         i = act_offset + rc;
         act_offset += rc + 1;
         rc = 0;
         
         if( acts[i].command == NULL ) {
            continue;
         }
         
         // what kind of action?
         if( acts[i].async ) {
            
            method = "vdev_action_run_async";
            rc = vdev_action_run_async( vreq, acts[i].command );
         }
         else {
            
            method = "vdev_action_run_sync";
            rc = vdev_action_run_sync( vreq, acts[i].command, NULL, 0 );
         }
         
         if( rc != 0 ) {
            
            vdev_error("%s('%s') rc = %d\n", method, acts[i].command, rc );
            
            if( rc < 0 ) {
               return rc;
            }
            else {
               
               // mask non-zero exit statuses 
               rc = 0;  
            }
         }
      }
   }
   
   return rc;
}


