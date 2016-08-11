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
#include "vdev.h"
#include "libvdev/match.h"

#define INI_MAX_LINE 4096
#define INI_STOP_ON_FIRST_ERROR 1

#include "libvdev/ini.h"

// to be passed into the action loader loop
struct vdev_action_loader_cls {

   struct sglib_vdev_action_vector* acts;
   struct vdev_config* config;
};

// prototypes
SGLIB_DEFINE_VECTOR_PROTOTYPES( vdev_action );
SGLIB_DEFINE_VECTOR_FUNCTIONS( vdev_action );

static int vdev_action_daemonlet_stop( struct vdev_action* act );

// initialize an action 
// return 0 on success 
// return -ENOMEM on OOM 
int vdev_action_init( struct vdev_action* act, vdev_device_request_t trigger, char* path, char* command, char* helper, bool async ) {
   
   int rc = 0;
   memset( act, 0, sizeof(struct vdev_action) );
   
   act->trigger = trigger;
   act->path = vdev_strdup_or_null( path );
   act->command = vdev_strdup_or_null( command );
   act->helper = vdev_strdup_or_null( helper );
   act->async = async;
   
   act->is_daemonlet = false;
   act->daemonlet_stdin = -1;
   act->daemonlet_stdout = -1;
   act->daemonlet_pid = -1;
   
   if( act->path != NULL ) {
      
      rc = vdev_match_regex_init( &act->path_regex, path );
      
      if( rc != 0 ) {
         
         vdev_error("vdev_match_regex_init('%s') rc = %d\n", path, rc );
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


// add an environment variable to send to a helper 
// return 0 on success 
// return -ENOMEM on OOM 
// return -EEXIST if the name already exists 
int vdev_action_add_var( struct vdev_action* act, char const* name, char const* value ) {
   return vdev_params_add( &act->helper_vars, name, value );
}

// free an action 
// always succeeds
// NOTE: does NOT touch the daemonlet state.
int vdev_action_free( struct vdev_action* act ) {
   
   if( act->name != NULL ) {
      
      free( act->name );
      act->name = NULL;
   }
   
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
   
   if( act->helper_vars != NULL ) {
      
      vdev_params_free( act->helper_vars );
      act->helper_vars = NULL;
   }
   
   if( act->helper != NULL ) {
      
      free( act->helper );
      act->helper = NULL;
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
               
               vdev_error("vdev_match_regex_init('%s') rc = %d\n", act->path, rc );
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
            
            fprintf(stderr, "Invalid event type '%s'\n", value);
            return 0;
         }
         else {
            return 1;
         }
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_COMMAND) == 0 ) {
         
         // shell command 
         if( act->command != NULL ) {
            
            free( act->command );
         }
         
         act->command = vdev_strdup_or_null( value );
         return 1;
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_HELPER) == 0 ) {
         
         // helper in /lib/vdev 
         if( act->helper != NULL ) {
            
            free( act->helper );
         }
         
         act->helper = vdev_strdup_or_null( value );
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
            
            fprintf(stderr, "Invalid async value '%s'\n", value);
            return 0;
         }
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_TYPE) == 0 ) {
         
         // remember this
         act->has_type = true;
         act->type = vdev_strdup_or_null( value );
         return 1;
      }
      
      if( strcmp(name, VDEV_ACTION_NAME_IF_EXISTS) == 0 ) {
         
         // if-exists flag 
         if( strcmp( value, VDEV_ACTION_IF_EXISTS_ERROR ) == 0 ) {
            
            act->if_exists = VDEV_IF_EXISTS_ERROR;
            return 1;
         }
         
         if( strcmp( value, VDEV_ACTION_IF_EXISTS_MASK ) == 0 ) {
            
            act->if_exists = VDEV_IF_EXISTS_MASK;
            return 1;
         }
         
         if( strcmp( value, VDEV_ACTION_IF_EXISTS_RUN ) == 0 ) {
            
            act->if_exists = VDEV_IF_EXISTS_RUN;
            return 1;
         }
         
         fprintf(stderr, "Invalid '%s' value '%s'\n", name, value );
         return 0;
      }
      
      if( strcmp(name, VDEV_ACTION_DAEMONLET) == 0 ) {
         
         // this is a daemonlet 
         if( strcasecmp( value, "true" ) == 0 ) {
            
            act->is_daemonlet = true;
         }
         
         return 1;
      }
      
      if( strncmp(name, VDEV_ACTION_NAME_OS_PREFIX, strlen(VDEV_ACTION_NAME_OS_PREFIX)) == 0 ) {
         
         // OS-specific param 
         rc = vdev_action_add_param( act, name + strlen(VDEV_ACTION_NAME_OS_PREFIX), value );
         
         if( rc == 0 ) {
            return 1;
         }
         else {
            vdev_error("vdev_action_add_param( '%s', '%s' ) rc = %d\n", name, value, rc );
            return 0;
         }
      }
      
      if( strncmp(name, VDEV_ACTION_NAME_VAR_PREFIX, strlen(VDEV_ACTION_NAME_VAR_PREFIX)) == 0 ) {
          
         // helper-specific variable 
         rc = vdev_action_add_var( act, name + strlen(VDEV_ACTION_NAME_VAR_PREFIX), value );
         
         if( rc == 0 ) {
            return 1;
         }
         else {
            vdev_error("vdev_action_add_var( '%s', '%s' ) rc = %d\n", name, value, rc );
            return 0;
         }
      }
      fprintf(stderr, "Unknown field '%s' in section '%s'\n", name, section );
      return 0;
   }
   
   fprintf(stderr, "Unknown section '%s'\n", section);
   return 0;
}

// sanity check an action 
// return 0 on success
// return -EINVAL if invalid
int vdev_action_sanity_check( struct vdev_action* act ) {
   
   int rc = 0;
   
   if( act->command == NULL && act->rename_command == NULL && act->helper == NULL ) {
      
      fprintf(stderr, "Action is missing 'command=', 'rename_command=', and 'helper='\n");
      rc = -EINVAL;
   }
   
   if( act->command != NULL && act->helper != NULL ) {
       
      fprintf(stderr, "Action has both 'command=' and 'helper='\n");
      rc = -EINVAL;
   }
   
   if( act->trigger == VDEV_DEVICE_INVALID ) {
      
      fprintf(stderr, "Action is missing 'event='\n");
      rc = -EINVAL;
   }
   
   return rc;
}


// perform misc. post-processing on an action:
// * if the command is NULL but the helper is not, then set command to be the full path to the helper, and don't use a shell
// return 0 on success 
// return -ENOMEM on OOM 
int vdev_action_postprocess( struct vdev_config* config, struct vdev_action* act ) {
    
   int rc = 0;
   
   if( act->command == NULL && act->helper != NULL ) {
       
      act->command = VDEV_CALLOC( char, strlen(config->helpers_dir) + 1 + strlen(act->helper) + 1 );
      if( act->command == NULL ) {
          return -ENOMEM;
      }
      
      sprintf( act->command, "%s/%s", config->helpers_dir, act->helper );
      
      act->use_shell = false;
   }
   else {
      
      // given a shell command string 
      act->use_shell = true;
   }
   
   return rc;
}

// load an action from a file
// return 0 on success
// return -errno on failure to open, read, parse, or close
// return -EINVAL if the loaded action fails our sanity tests
int vdev_action_load_file( struct vdev_config* config, char const* path, struct vdev_action* act, FILE* f ) {
   
   int rc = 0;
   
   rc = ini_parse_file( f, vdev_action_ini_parser, act );
   if( rc != 0 ) {
      vdev_error("ini_parse_file(action) rc = %d\n", rc );
   }
   else {
      rc = vdev_action_sanity_check( act );
      if( rc != 0 ) {
         vdev_error("Invalid action '%s'\n", path );
      }
   }
   
   if( rc == 0 ) {
      
      // postprocess 
      rc = vdev_action_postprocess( config, act );
   }
   return rc;
}


// load an action from a path
// return -ENOMEM if OOM
// return -errno on failure to open or read the file
// return -EINVAL if the file could not be parsed
int vdev_action_load( struct vdev_config* config, char const* path, struct vdev_action* act ) {
   
   int rc = 0;
   FILE* f = NULL;
   
   rc = vdev_action_init( act, VDEV_DEVICE_INVALID, NULL, NULL, NULL, false );
   if( rc != 0 ) {
      
      vdev_error("vdev_action_init('%s') rc = %d\n", path, rc );
      return rc;
   }
   
   f = fopen( path, "r" );
   if( f == NULL ) {
      
      vdev_action_free( act );
      rc = -errno;
      
      return rc;
   }
   
   rc = vdev_action_load_file( config, path, act, f );
   
   fclose( f );
   
   if( rc == -EINVAL ) {
      
      vdev_action_free( act );
      vdev_error("Invalid action '%s'\n", path );
   }
   else if( rc == 0 ) {
      
      // store the name 
      act->name = vdev_strdup_or_null( path );
      if( act->name == NULL ) {
         
         // OOM 
         vdev_action_free( act );
         rc = -ENOMEM;
      }
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
   struct vdev_action_loader_cls* loader_cls = (struct vdev_action_loader_cls*)cls;
   
   struct sglib_vdev_action_vector* acts = loader_cls->acts;
   struct vdev_config* config = loader_cls->config;
   
   // skip if not a regular file 
   rc = stat( path, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("stat(%s) rc = %d\n", path, rc );
      return rc;
   }
   
   if( !S_ISREG( sb.st_mode ) ) {
      
      return 0;
   }
   
   vdev_debug("Load Action %s\n", path );
   
   memset( &act, 0, sizeof(struct vdev_action) );
   
   rc = vdev_action_load( config, path, &act );
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
int vdev_action_load_all( struct vdev_config* config, struct vdev_action** ret_acts, size_t* ret_num_acts ) {
   
   int rc = 0;
   struct vdev_action_loader_cls loader_cls;
   struct sglib_vdev_action_vector acts;
   
   sglib_vdev_action_vector_init( &acts );
   
   loader_cls.acts = &acts;
   loader_cls.config = config;
   
   rc = vdev_load_all( config->acts_dir, vdev_action_loader, &loader_cls );
   
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
// stdout will be captured to *output
// return the exit status on success (non-negative)
// return negative on error
int vdev_action_run_sync( struct vdev_device_request* vreq, char const* command, vdev_params* helper_vars, bool use_shell, char** output, size_t max_output ) {
   
   int rc = 0;
   int exit_status = 0;
   char** req_env = NULL;
   size_t num_env = 0;
   
   // convert to environment variables
   rc = vdev_device_request_to_env( vreq, helper_vars, &req_env, &num_env, 0 );
   if( rc != 0 ) {
      
      vdev_error("vdev_device_request_to_env(%s) rc = %d\n", vreq->path, rc );
      return rc;
   }
   
   vdev_debug("run command: '%s'\n", command );
   
   for( unsigned int i = 0; i < num_env; i++ ) {
      vdev_debug("command env: '%s'\n", req_env[i] );
   }
   
   rc = vdev_subprocess( command, req_env, output, max_output, vreq->state->error_fd, &exit_status, use_shell );
   
   vdev_debug("exit status %d\n", exit_status );
   
   if( output != NULL && *output != NULL ) {
      
      vdev_debug("command output: '%s'\n", *output );
   }
   
   VDEV_FREE_LIST( req_env );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_subprocess('%s', use_shell=%d) rc = %d\n", command, use_shell, rc );
      
      return rc;
   }
   
   if( exit_status != 0 ) {
      
      vdev_error("vdev_subprocess('%s', use_shell=%d) exit status = %d\n", command, use_shell, exit_status );
   }
   
   return exit_status;
}


// carry out an action, asynchronously.
// fork, setsid, fork, and exec, so the helper gets reparented to init.
// return 0 if we were able to fork 
// return -errno on failure to fork
// TODO: configurable interpreter (defaults to /bin/dash)
int vdev_action_run_async( struct vdev_device_request* req, char const* command, vdev_params* helper_vars, bool use_shell ) {
   
   int rc = 0;
   pid_t pid = 0;
   pid_t sid = 0;
   int max_fd = 0;
   
   // generate the environment 
   char** env = NULL;
   size_t num_env = 0;
   
   rc = vdev_device_request_to_env( req, helper_vars, &env, &num_env, 0 );
   if( rc != 0 ) {
      
      // NOTE: must be async-safe!
      vdev_error("vdev_device_request_to_env rc = %d\n", rc);
      return rc;
   }
   
   vdev_debug("run command async: '%s'\n", command );
   
   for( unsigned int i = 0; i < num_env; i++ ) {
      vdev_debug("command async env: '%s'\n", env[i] );
   }
   
   pid = fork();
   
   if( pid == 0 ) {
      
      // child
      // detach from parent 
      sid = setsid();
      if( sid < 0 ) {
         
         rc = -errno;
         
         // NOTE: must be async-safe!
         vdev_error_async_safe("setsid failed\n" );
         exit(1);
      }
      
      pid = fork();
      
      if( pid == 0 ) {
         
         // fully detached
         max_fd = sysconf(_SC_OPEN_MAX);
         if( max_fd < 0 ) {
            
            // should be big enough...
            max_fd = 1024;
         }
         
         // close everything except stderr, which should be directed to our log
         for( int i = 0; i < max_fd; i++ ) {
            if( i != STDERR_FILENO ) {
               close( i );
            }
         }
         
         clearenv();
         
         // run the command 
         if( use_shell ) {
            execle( "/bin/dash", "dash", "-c", command, (char*)0, env );
         }
         else {
            execle( command, command, (char*)0, env );
         }
         
         // keep gcc happy 
         exit(0);
      }
      else if( pid > 0 ) {
         
         exit(0);
      }
      else {
         
         rc = -errno;
         vdev_error_async_safe("fork() failed\n");
         exit(-rc);
      }
   }
   else if( pid > 0 ) {
      
      rc = 0;
      
      VDEV_FREE_LIST( env );
      
      // parent; succeeded
      // wait for intermediate process
      pid_t child_pid = waitpid( pid, &rc, 0 );
      if( child_pid < 0 ) {
         
         rc = -errno;
         vdev_error("waitpid(%d) rc = %d\n", pid, rc );
         return rc;
      }
      else if( WIFEXITED( rc ) && WEXITSTATUS( rc ) != 0 ) {
         
         rc = -WEXITSTATUS( rc );
         vdev_error("fork() rc = %d\n", rc );
      }
      
      return rc;
   }
   else {
      
      // error 
      rc = -errno;
      
      VDEV_FREE_LIST( env );
      
      vdev_error("fork() rc = %d\n", rc);
      
      return rc;
   }
}


// clean up a daemonlet's state in an action 
// always succeeds 
static int vdev_action_daemonlet_clean( struct vdev_action* act ) {
   
   // dead 
   act->daemonlet_pid = -1;
   
   if( act->daemonlet_stdin >= 0 ) {
      close( act->daemonlet_stdin );
      act->daemonlet_stdin = -1;
   }
   
   if( act->daemonlet_stdout >= 0 ) {
      close( act->daemonlet_stdout );
      act->daemonlet_stdout = -1;
   }
   
   return 0;
}

// start up a daemonlet, using the daemonlet helper program at $VDEV_HELPERS/daemonlet.
// stderr will be routed to /dev/null
// return 0 on success, and fill in the relevant information to act.
// return 0 if the daemonlet is already running
// return -errno from stat(2) if we couldn't find the daemonlet runner (i.e. -ENOENT)
// return -EPERM if the daemonlet is not executable
// return -errno from pipe(2) if we could not allocate a pipe
// return -errno from open(2) if we could not open /dev/null
// return -errno from fork(2) if we could not fork
// return -ECHILD if the daemonlet died before it could signal readiness
// NOTE: /dev/null *must* exist already--this should be taken care of by the pre-seed script.
static int vdev_action_daemonlet_start( struct vdev_state* state, struct vdev_action* act ) {
   
   int rc = 0;
   pid_t pid = 0;
   int daemonlet_pipe_stdin[2];
   int daemonlet_pipe_stdout[2];
   char daemonlet_runner_path[ PATH_MAX+1 ];
   char vdevd_global_metadata[ PATH_MAX+1 ];
   char null_path[ PATH_MAX+1 ];
   long max_open = sysconf( _SC_OPEN_MAX );
   struct stat sb;
   struct vdev_config* config = state->config;

   char* daemonlet_argv[] = {
      "vdevd-daemonlet",
      act->command,
      NULL
   };
   
   if( max_open <= 0 ) {
      max_open = 1024;  // a good guess
   }
   
   if( act->daemonlet_pid > 0 ) {
      return 0;
   }
   
   memset( daemonlet_runner_path, 0, PATH_MAX+1 );
   memset( vdevd_global_metadata, 0, PATH_MAX+1 );
   
   snprintf( daemonlet_runner_path, PATH_MAX, "%s/daemonlet", config->helpers_dir );
   snprintf( vdevd_global_metadata, PATH_MAX, "%s/" VDEV_METADATA_PREFIX, config->mountpoint );
   
   // the daemonlet runner must exist 
   rc = stat( daemonlet_runner_path, &sb );
   if( rc != 0 ) {
      
      vdev_error("stat('%s') rc = %d\n", daemonlet_runner_path, rc );
      return rc;
   }
   
   // the daemonlet runner must be executable
   if( !S_ISREG( sb.st_mode ) || !(((S_IXUSR & sb.st_mode) && sb.st_uid == geteuid()) || ((S_IXGRP & sb.st_mode) && sb.st_gid == getegid()) || (S_IXOTH & sb.st_mode )) ) {
      
      vdev_error("%s is not a regular file, or is not executable for vdevd\n", daemonlet_runner_path );
      return -EPERM;
   }
   
   rc = pipe( daemonlet_pipe_stdin );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("pipe rc = %d\n", rc );
      return rc;
   }
   
   rc = pipe( daemonlet_pipe_stdout );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("pipe rc = %d\n", rc );
      
      close( daemonlet_pipe_stdin[0] );
      close( daemonlet_pipe_stdin[1] );
      return rc;
   }
  
   pid = fork();
   if( pid == 0 ) {
      
      // child 
      close( daemonlet_pipe_stdin[1] );
      close( daemonlet_pipe_stdout[0] );
      
      // redirect 
      rc = dup2( daemonlet_pipe_stdin[0], STDIN_FILENO );
      if( rc < 0 ) {
         
         vdev_error_async_safe("dup2 stdin failed\n");
         exit(1);
      }
      
      rc = dup2( daemonlet_pipe_stdout[1], STDOUT_FILENO );
      if( rc < 0 ) {
         
         vdev_error_async_safe("dup2 stdout to vdevd failed\n");
         _exit(1);
      }

      rc = dup2( state->error_fd, STDERR_FILENO );
      if( rc < 0 ) {

         vdev_error_async_safe("dup2 stderr to null failed\n");
         _exit(1);
      }
      
      // close everything else
      // the daemonlet should capture stderr itself
      for( int i = 3; i < max_open; i++ ) {
         
         close( i );
      }
   
      // set basic environment variables 
      clearenv();
      setenv( "VDEV_GLOBAL_METADATA", vdevd_global_metadata, 1 );
      setenv( "VDEV_MOUNTPOINT", config->mountpoint, 1 );
      setenv( "VDEV_HELPERS", config->helpers_dir, 1 );
      setenv( "VDEV_LOGFILE", config->logfile_path, 1 );
      setenv( "VDEV_CONFIG_FILE", config->config_path, 1 );
      setenv( "VDEV_INSTANCE", config->instance_str, 1 );
      
      // start the daemonlet 
      execv( daemonlet_runner_path, daemonlet_argv );
      
      // keep gcc happy
      _exit(0);
   }
   else if( pid > 0 ) {
      
      // parent vdevd 
      close( daemonlet_pipe_stdin[0] );
      close( daemonlet_pipe_stdout[1] );
      
      // wait for child to indicate readiness, if running synchronously
      if( !act->async ) {
         
         while( 1 ) {
            
            char tmp = 0;
            rc = vdev_read_uninterrupted( daemonlet_pipe_stdout[0], &tmp, 1 );
            
            if( rc <= 0 ) {
               
               // some fatal error, or process has closed stdout
               // not much we can safely do at this point (don't want to risk SIGKILL'ing)
               close( daemonlet_pipe_stdin[1] );
               close( daemonlet_pipe_stdout[0] );
               
               if( rc == 0 ) {
                  
                  // process has closed stdout
                  vdev_error("vdev_read_uninterrupted(%d PID=%d action='%s') rc = %d\n", daemonlet_pipe_stdout[0], pid, act->name, rc );
                  rc = -ECHILD;
               }
               
               return rc;
            }
            else {
               
               // got back readiness hint
               break;
            }
         }
      }
      
      // record runtime state 
      act->is_daemonlet = true;
      act->daemonlet_pid = pid;
      act->daemonlet_stdin = daemonlet_pipe_stdin[1];
      act->daemonlet_stdout = daemonlet_pipe_stdout[0];
      
      // daemonlet started!
      return 0;
   }
   else {
      
      // fork() error 
      rc = -errno;
      vdev_error("fork() rc = %d\n", rc );
      
      close( daemonlet_pipe_stdin[0] );
      close( daemonlet_pipe_stdin[1] );
      close( daemonlet_pipe_stdout[0] );
      close( daemonlet_pipe_stdout[1] );
      
      return rc;
   }
}


// stop a daemonlet and join with it
// first, ask it by writing "exit" to its stdin pipe.
// wait 30 seconds before SIGTERM'ing the daemonlet.
// return 0 on success, even if the daemonlet is already dead
static int vdev_action_daemonlet_stop( struct vdev_action* act ) {
   
   int rc = 0;
   pid_t child_pid = 0;
   struct timespec timeout;
   struct timespec stop_deadline;
   sigset_t sigchld_sigset;
   siginfo_t sigchld_info;
   
   timeout.tv_sec = 30;
   timeout.tv_nsec = 0;
   
   if( act->daemonlet_pid <= 0 || act->daemonlet_stdin < 0 || act->daemonlet_stdout < 0 ) {
      // not running
      return 0;
   }
   
   // confirm that it is still running by trying to join with it 
   child_pid = waitpid( act->daemonlet_pid, &rc, WNOHANG );
   if( child_pid == act->daemonlet_pid || child_pid < 0 ) {
      
      // joined, or not running
      vdev_debug("Daemonlet %d (%s) dead\n", act->daemonlet_pid, act->name );
      vdev_action_daemonlet_clean( act );
      return 0;
   }
   
   // ask it to die: close stdin 
   rc = close( act->daemonlet_stdin );
   
   if( rc < 0 ) {
      
      vdev_error("close(%d PID=%d name=%s) rc = %d\n", act->daemonlet_stdin, act->daemonlet_pid, act->name, rc );
      act->daemonlet_stdin = -1;
      
      // no choice but to kill this one 
      kill( act->daemonlet_pid, SIGTERM );
      vdev_action_daemonlet_clean( act );
      
      // join with it...
      rc = 0;
   }
   else {
      
      // tell daemonlet to die 
      rc = kill( act->daemonlet_pid, SIGINT );
      if( rc < 0 ) {
         
         vdev_error("kill(PID=%d name=%s) rc = %d\n", act->daemonlet_pid, act->name, rc );
      }
      
      // will wait for the child to die
      act->daemonlet_stdin = -1;
   }
   
   // join with it.
   while( 1 ) {
         
      // attempt to join 
      child_pid = waitpid( act->daemonlet_pid, &rc, 0 );
      
      if( child_pid < 0 ) {
         
         rc = -errno;
         if( rc == -ECHILD ) {
            
            // already dead 
            rc = 0;
            vdev_debug("Daemonlet %d (%s) dead\n", act->daemonlet_pid, act->name );
            vdev_action_daemonlet_clean( act );
            break;
         }
         else if( rc == -EINTR ) {
            
            // unhandled signal *or* SIGCHLD 
            child_pid = waitpid( act->daemonlet_pid, &rc, WNOHANG );
            if( child_pid > 0 ) {
               
               // child died 
               break;
            }
            else {
               
               // try waiting again
               continue;
            }
         }
      }
      else if( child_pid > 0 ) {
         
         // joined!
         break;
      }
   }

   // clean this child out (even if we had an error with waitpid)
   vdev_debug("Daemonlet %d (%s) dead\n", act->daemonlet_pid, act->name );
   vdev_action_daemonlet_clean( act );
   
   return rc;
}


// stop all daemonlets 
// always succeeds; mask all stop errors (i.e. only call this on shutdown)
int vdev_action_daemonlet_stop_all( struct vdev_action* actions, size_t num_actions ) {
   
   int rc = 0;
   
   for( unsigned int i = 0; i < num_actions; i++ ) {
      
      if( !actions[i].is_daemonlet ) {
         continue;
      }
      
      rc = vdev_action_daemonlet_stop( &actions[i] );
      if( rc < 0 ) {
         
         vdev_error("vdev_action_daemonlet_stop('%s' PID=%d) rc = %d\n", actions[i].name, actions[i].daemonlet_pid, rc );
      }
   }
   
   return 0;
}

// read a string-ified int64_t from a file descriptor, followed by a newline.
// this is used to get a daemonlet return code 
// return 0 on success, and set *ret 
// return -errno on I/O error (such as -EPIPE)
// return -EAGAIN if we got EOF
static int vdev_action_daemonlet_read_int64( int fd, int64_t* ret ) {
  
   int64_t value = 0;
  
   int cnt = 0; 
   int c = 0;
   int rc = 0;
   int s = 1;
   
   while( 1 ) {
      
      // next character 
      rc = vdev_read_uninterrupted( fd, (char*)&c, 1 );
      if( rc < 0 ) {
         
         return rc;
      }
      if( rc == 0 ) {
         
         return -EAGAIN;
      }
      
      if( c == '\n' ) {
         break;
      }
      
      if( cnt == 0 && c == '-' ) {
         s = -1;
      }
      else if( c < '0' || c > '9' ) {
         
         // invalid 
         rc = -EINVAL;
         break;
      }
      
      value *= 10;
      value += (c - '0');

      cnt++;
   }
   
   *ret = value * s;
   return 0;
}


// issue a command to a daemonlet, and get back its return code.
// the daemonlet should already be running (or thought to be running) before calling this method.
// if this method fails, the caller should consider restarting the daemonlet.
// return 0 on success, and set *daemonlet_rc
// return -ENOMEM on OOM
// return -EAGAIN if the daemonlet needs to be restarted and the request retried.
// return -EPERM on permanent daemonlet failure
static int vdev_action_daemonlet_send_command( struct vdev_device_request* vreq, struct vdev_action* act, int64_t* daemonlet_rc ) {
    
   int rc = 0;
   char** req_env = NULL;
   size_t num_env = 0;
   char env_buf[ PATH_MAX+1 ];

   memset( env_buf, 0, PATH_MAX+1 );

   // generate the environment...
   rc = vdev_device_request_to_env( vreq, act->helper_vars, &req_env, &num_env, 1 );
   if( rc != 0 ) {
      
      vdev_error("vdev_device_request_to_env(%s) rc = %d\n", vreq->path, rc );
      return rc;
   }
   
   
   vdev_debug("run daemonlet (async=%d): '%s'\n", act->async, act->name );
   
   for( unsigned int i = 0; i < num_env; i++ ) {
      vdev_debug("daemonlet env: '%s'\n", req_env[i] );
   } 
   
   // feed environment variables
   for( unsigned int i = 0; i < num_env; i++ ) {
      
      snprintf( env_buf, PATH_MAX, "%s\n", req_env[i] );
      
      rc = vdev_write_uninterrupted( act->daemonlet_stdin, env_buf, strlen(env_buf) );
      if( rc < 0 ) {
         
         // failed to write! 
         vdev_error("vdev_write_uninterrupted(%d) to daemonlet '%s' rc = %d\n", act->daemonlet_stdin, act->name, rc );
         break;
      }
   }
   
   if( rc > 0 ) {
      
      // feed end-of-environment flag
      rc = vdev_write_uninterrupted( act->daemonlet_stdin, "done\n", strlen("done\n") );
      if( rc < 0 ) {
         
         vdev_error("vdev_write_uninterrupted(%d) to daemonlet '%s' rc = %d\n", act->daemonlet_stdin, act->name, rc );
      }
   }
   
   VDEV_FREE_LIST( req_env );
   
   if( rc < 0 ) {

      // -EPIPE means the daemonlet is dead, and the caller should restart it
      if( rc == -EPIPE ) {
         return -EAGAIN;
      }
      else {
         return -EPERM;
      }
   }
   
   // if we're running asynchronously, then we don't care about getting back the reply (stdout is routed to /dev/null anyway)
   if( act->async ) {
      *daemonlet_rc = 0;
      return 0;
   }
   
   // wait for a status code reply 
   rc = vdev_action_daemonlet_read_int64( act->daemonlet_stdout, daemonlet_rc );
   if( rc < 0 ) {
      
      vdev_error("vdev_action_daemonlet_read_int64('%s') rc = %d\n", act->name, rc );

      // -EPIPE means the daemonlet is dead, and the caller should restart it 
      if( rc == -EPIPE ) {
         return -EAGAIN;
      }
      else {
         return -EPERM;
      } 
   }
   
   if( *daemonlet_rc < 0 || *daemonlet_rc > 255 ) {
      
      // invalid daemonlet return code 
      // caller should consider restarting the daemonlet and trying again
      vdev_error("vdev_daemonlet_read_int64('%s', PID=%d) exit status %d\n", act->name, act->daemonlet_pid, (int)*daemonlet_rc );
      return -EAGAIN;
   }
   
   return 0;
}


// carry out a command by sending it to a running daemonlet.
// restart the daemonlet if we need to (e.g. if it hasn't been started, or it died since the last device request).
// return the daemonlet's exit status on success (will be positive if the daemonlet failed in error).
// return -ENOMEM on OOM
// return -EPERM if the daemonlet could not be started, or was not responding and we could not restart it.  A subsequent call probably won't succeed.
// return -EAGAIN if the daemonlet was not responding, but we were able to stop it.  A subsequent call might succeed in starting it up and feeding it the request.
// return -EINVAL if the action is not a daemonlet.
// return a positive exit code if the daemonlet failed to process the device request
// NOTE: if this method fails to communicate with the daemonlet, it will try to "reset" the daemonlet by stopping it, and allowing a subsequent call to start it.
// NOTE: not reload-safe; call while the reload lock is held
int vdev_action_run_daemonlet( struct vdev_device_request* vreq, struct vdev_action* act ) {
   
   int rc = 0;
   int64_t daemonlet_rc = 0;
   int num_attempts = 0;

   if( !act->is_daemonlet ) {
      return -EINVAL;
   }
   
   // do we need to start it?
   if( act->daemonlet_pid <= 0 ) {
      
      rc = vdev_action_daemonlet_start( vreq->state, act );
      if( rc < 0 ) {
         
         vdev_error("vdev_action_daemonlet_start('%s') rc = %d\n", act->name, rc );
         return -EPERM;
      }
   }
  
   // try twice, in case we need to stop and start it.
   while( num_attempts < 2 ) {

      rc = vdev_action_daemonlet_send_command( vreq, act, &daemonlet_rc );
      if( rc != 0 ) {
         
         vdev_error("vdev_action_daemonlet_send_command('%s') rc = %d\n", act->name, rc );

         if( rc == -EAGAIN ) {

            // try restarting and re-dispatching the command 
            rc = vdev_action_daemonlet_stop( act );
            if( rc < 0 ) {

               vdev_error("vdev_action_daemonlet_stop('%s', PID=%d) rc = %d\n", act->name, act->daemonlet_pid, rc );
               rc = -EPERM;
               break;
            }
            else {

               rc = vdev_action_daemonlet_start( vreq->state, act );
               if( rc < 0 ) {

                   vdev_error("vdev_action_daemonlet_start('%s') rc = %d\n", act->name, rc );
                   rc = -EPERM;
                   break;
               }
               else {

                  // try again  
                  num_attempts++;
                  continue;
               }
            }
         }
         else {
            
            // permanent failure 
            rc = -EPERM;
            break;
         }
      }
      else {

         // successfully dispatched the request, and (if not async) received an exit status via daemonlet_rc
         break;
      }
   }

   if( rc == 0 ) {

       vdev_debug("daemonlet '%s' returned %d\n", act->name, (int)daemonlet_rc );
       return (int)daemonlet_rc;
   }
   else {
      
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
         rc = vdev_action_run_sync( vreq, acts[i].rename_command, acts[i].helper_vars, true, &new_path, PATH_MAX + 1 );
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
// commands are executed optimistically--even if one fails, the other subsequent ones will be attempted, unless
// the device already exists and we encounter if_exists=error in one of the matching actions.
// if the device already exists (given by the exists flag), then only run commands with if_exists set to "run"
// return 0 on success
// return negative on failure
int vdev_action_run_commands( struct vdev_device_request* vreq, struct vdev_action* acts, size_t num_acts, bool exists ) {
   
   int rc = 0;
   int act_offset = 0;
   int i = 0;
   char const* method = NULL;
   struct timespec start;
   struct timespec end;
   
   while( act_offset < (signed)num_acts && rc == 0 ) {
      
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
         
         if( vreq->type == VDEV_DEVICE_ADD && exists && acts[i].if_exists != VDEV_IF_EXISTS_RUN ) {
            
            if( acts[i].if_exists == VDEV_IF_EXISTS_ERROR ) {
               
               vdev_error("Will stop processing %s, since it already exists\n", vreq->path );
               rc = 1;
               break;
            }
            else {
              
               // device already exists, but this is not considered by the .act file to be an error 
               continue;
            }
         }
         
         // benchmark this...
         clock_gettime( CLOCK_MONOTONIC, &start );
         
         // what kind of action to take?
         if( !acts[i].is_daemonlet ) {
         
            if( acts[i].async ) {
               
               // fork a subprocess and handle asynchronously
               method = "vdev_action_run_async";
               rc = vdev_action_run_async( vreq, acts[i].command, acts[i].helper_vars, acts[i].use_shell );
            }
            else {
               
               // run as a subprocess, wait, and join with it
               method = "vdev_action_run_sync";
               rc = vdev_action_run_sync( vreq, acts[i].command, acts[i].helper_vars, acts[i].use_shell, NULL, 0 );
            }
         }
         else {
            
            if( acts[i].async ) {
               
               // run as a daemonlet, feed it the request, but don't wait for a reply
               method = "vdev_action_run_daemonlet_async";
            }
            else {
               
               // run as a daemonlet, feed the request, and wait for a reply
               method = "vdev_action_run_daemonlet";
            }
            
            rc = vdev_action_run_daemonlet( vreq, &acts[i] );
         }
         
         clock_gettime( CLOCK_MONOTONIC, &end );
         
         if( rc != 0 ) {
            
            vdev_error("%s('%s') rc = %d\n", method, acts[i].command, rc );
            
            if( rc < 0 ) {
               return rc;
            }
            else {
               
               // mask non-zero exit statuses 
               uint64_t start_millis = 1000L * start.tv_sec + (start.tv_nsec / 1000000L);
               uint64_t end_millis = 1000L * end.tv_sec + (end.tv_nsec / 1000000L);
               
               vdev_debug("Benchmark: action %s failed (exit %d) in %lu millis\n", acts[i].name, rc, (unsigned long)(end_millis - start_millis) );
               rc = 0;
            }
         }
         else {
            
            // success! update benchmark 
            acts[i].num_successful_calls++;
            
            uint64_t start_millis = 1000L * start.tv_sec + (start.tv_nsec / 1000000L);
            uint64_t end_millis = 1000L * end.tv_sec + (end.tv_nsec / 1000000L);
            
            acts[i].cumulative_time_millis += (end_millis - start_millis);
            
            // log timings directly, for finer granularity...
            vdev_debug("Benchmark: action %s succeeded in %lu millis\n", acts[i].name, (unsigned long)(end_millis - start_millis) );
         }
      }
   }
   
   if( rc > 0 ) {
      
      // not an error, but a cause to abort
      rc = 0;
   }
   
   return rc;
}


// print out all benchmark information for this action 
// always succeeds
int vdev_action_log_benchmarks( struct vdev_action* action ) {
   
  //   int rc = 0;
   
   if( action->num_successful_calls > 0 ) {
      vdev_debug("Action '%s' (daemon=%d, async=%d): %lu successful calls; %lu millis total; %lf avg.\n",
                 action->name, action->is_daemonlet, action->async, action->num_successful_calls, action->cumulative_time_millis, (double)(action->cumulative_time_millis) / (double)(action->num_successful_calls));
   }
   else {
      vdev_debug("Action '%s' (daemon=%d, async=%d): 0 successful calls\n", action->name, action->is_daemonlet, action->async );
   }
   
   return 0;
}


