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

#include "action.h"
#include "match.h"

#define INI_MAX_LINE 4096
#define INI_STOP_ON_FIRST_ERROR 1

#include "ini.h"

// initialize an action 
int vdev_action_init( struct vdev_action* act, vdev_device_request_t trigger, char* path, char* command, bool async ) {
   
   int rc = 0;
   memset( act, 0, sizeof(struct vdev_action) );
   
   act->dev_params = new (nothrow) vdev_device_params_t();
   
   if( act->dev_params == NULL ) {
      return -ENOMEM;
   }
   
   act->trigger = trigger;
   act->path = vdev_strdup_or_null( path );
   act->command = vdev_strdup_or_null( command );
   act->async = async;
   
   if( act->path != NULL ) {
      
      rc = vdev_match_regex_init( &act->path_regex, path );
      
      if( rc != 0 ) {
         
         vdev_error("vdev_match_regex_init('%s') rc = %d\n", path );
         vdev_action_free( act );
         return rc;
      }
   }
   
   return 0;
}

// add a device parameter to match on 
int vdev_action_add_param( struct vdev_action* act, char const* name, char const* value ) {
   
   try {
      (*act->dev_params)[ string(name) ] = string(value);
      return 0;
   }
   catch( bad_alloc& ba ) {
      return -ENOMEM;
   }
}

// free an action 
int vdev_action_free( struct vdev_action* act ) {
   
   if( act->command != NULL ) {
      
      free( act->command );
      act->command = NULL;
   }
   
   if( act->dev_params != NULL ) {
      
      delete act->dev_params;
      act->dev_params = NULL;
   }
   
   if( act->path != NULL ) {
      
      free( act->path );
      act->path = NULL;
      
      
      regfree( &act->path_regex );
   }
   
   return 0;
}


// convert an action into a device request 
vdev_device_request_t vdev_action_parse_trigger( char const* type ) {
   
   if( strcmp(type, VDEV_ACTION_EVENT_ADD) == 0 ) {
      return VDEV_DEVICE_ADD;
   }
   else if( strcmp(type, VDEV_ACTION_EVENT_REMOVE) == 0 ) {
      return VDEV_DEVICE_REMOVE;
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
               
               vdev_error("vdev_match_regex_init('%s') rc = %d\n", act->path );
               return 0;
            }
         }
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
      
      if( strcmp(name, VDEV_ACTION_NAME_SHELL) == 0 ) {
         
         // shell command 
         if( act->command != NULL ) {
            
            free( act->path );
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
            
            fprintf(stderr, "Invalid async value '%s'\n", value);
            return 0;
         }
      }
      
      fprintf(stderr, "Unknown field '%s' in section '%s'\n", name, section );
      return 0;
   }
   
   if( strcmp(section, VDEV_OS_ACTION_NAME) == 0 ) {
      
      rc = vdev_action_add_param( act, name, value );
      
      if( rc == 0 ) {
         return 1;
      }
      else {
         vdev_error("vdev_action_add_param( '%s', '%s' ) rc = %d\n", name, value, rc );
         return 0;
      }
   }
   
   fprintf(stderr, "Unknown section '%s'\n", section);
   return 0;
}

// sanity check an action 
// return 0 on success
// return negative on invalid 
int vdev_action_sanity_check( struct vdev_action* act ) {
   
   int rc = 0;
   
   if( act->path == NULL ) {
      
      fprintf(stderr, "Action is missing 'path='\n");
      rc = -EINVAL;
   }
   
   if( act->command == NULL ) {
      
      fprintf(stderr, "Action is missing 'command='\n");
      rc = -EINVAL;
   }
   
   if( act->trigger == VDEV_DEVICE_INVALID ) {
      
      fprintf(stderr, "Action is missing 'event='\n");
      rc = -EINVAL;
   }
   
   return rc;
}

// load an action from a path
int vdev_action_load( char const* path, struct vdev_action* act ) {
   
   int rc = 0;
   FILE* f = NULL;
   
   f = fopen( path, "r" );
   if( f == NULL ) {
      
      rc = -errno;
      return rc;
   }
   
   rc = vdev_action_load_file( f, act );
   
   fclose( f );
   
   if( rc == -EINVAL ) {
      
      vdev_error("Invalid action %s\n", path );
   }
   
   return rc;
}


// load an action from a file
int vdev_action_load_file( FILE* f, struct vdev_action* act ) {
   
   int rc = 0;
   
   rc = ini_parse_file( f, vdev_action_ini_parser, act );
   if( rc != 0 ) {
      vdev_error("ini_parse_file(action) rc = %d\n", rc );
   }
   else {
      rc = vdev_action_sanity_check( act );
   }
   
   return rc;
}

// free a vector of actions
static void vdev_action_free_vector( vector<struct vdev_action>* acts ) {
   
   for( unsigned int i = 0; i < acts->size(); i++ ) {
      
      vdev_action_free( &acts->at(i) );
   }
   
   acts->clear();
}

// free a C-style list of actions (including the list itself)
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
int vdev_action_loader( char const* path, void* cls ) {
   
   int rc = 0;
   struct vdev_action act;
   struct stat sb;
   
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
   
   vector<struct vdev_action>* acts = (vector<struct vdev_action>*)cls;

   memset( &act, 0, sizeof(struct vdev_action) );
   
   rc = vdev_action_load( path, &act );
   if( rc != 0 ) {
      
      vdev_error("vdev_acl_load(%s) rc = %d\n", path, rc );
      return rc;
   }
   
   // save this action 
   try {
      acts->push_back( act );
   }
   catch( bad_alloc& ba ) {
      
      return -ENOMEM;
   }
   
   return 0;
}

// load all actions in a directory
// return 0 on success
// return negative on error
int vdev_action_load_all( char const* dir_path, struct vdev_action** ret_acts, size_t* ret_num_acts ) {
   
   int rc = 0;
   vector<struct vdev_action> acts;
   
   rc = vdev_load_all( dir_path, vdev_action_loader, &acts );
   
   if( rc != 0 ) {
      
      vdev_action_free_vector( &acts );
      return rc;
   }
   else {
         
      if( acts.size() == 0 ) {
         
         // nothing 
         *ret_acts = NULL;
         *ret_num_acts = 0;
      }
      else {
      
         // extract values
         struct vdev_action* act_list = VDEV_CALLOC( struct vdev_action, acts.size() );
         if( act_list == NULL ) {
            
            vdev_action_free_vector( &acts );
            return -ENOMEM;   
         }
         
         // NOTE: vectors are contiguous in memory 
         memcpy( act_list, &acts[0], sizeof(struct vdev_action) * acts.size() );
         
         *ret_acts = act_list;
         *ret_num_acts = acts.size();
      }
   }
   
   return 0;
}


// carry out an action, synchronously.
// return the exit status on success (non-negative)
// return negative on error
int vdev_action_run_sync( struct vdev_action* act ) {
   
   int rc = 0;
   int exit_status = 0;
   
   rc = vdev_subprocess( act->command, NULL, 0, &exit_status );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_subprocess(%s) rc = %d\n", act->command, rc );
      return rc;
   }
   
   return exit_status;
}


// carry out an action, asynchronously
// return 0 if we were able to fork 
int vdev_action_run_async( struct vdev_action* act ) {
   
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
      
      // preserve only the path environment variable
      char* path = vdev_strdup_or_null( getenv("PATH") );
      
      clearenv();
      setenv("PATH", path, 1 );
      
      // run the command 
      execl( "/bin/sh", "sh", "-c", act->command, (char*)0 );
      
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


// find the next action in a list of actions to run, given the path 
// return the index into acts of the next match, if found 
// return num_acts if not found 
// return negative on error
int vdev_action_find_next( char const* path, struct vdev_action* acts, size_t num_acts ) {
   
   int rc = 0;
   int i = 0;
   
   for( i = 0; (unsigned)i < num_acts; i++ ) {
      
      rc = vdev_match_regex( path, &acts[i].path_regex );
      
      if( rc > 0 ) {
         return i;
      }
      else if( rc < 0 ) {
         return rc;
      }
   }
   
   return num_acts;
}


// run all actions for a device, sequentially, in lexographic order
// return 0 on success
// return negative on failure
int vdev_action_run_all( struct vdev_device_request* vreq, struct vdev_action* acts, size_t num_acts ) {
   
   int rc = 0;
   int act_offset = 0;
   int i = 0;
   char const* method = NULL;
   
   while( act_offset < (signed)num_acts ) {
      
      // find the next action
      rc = vdev_action_find_next( vreq->path, acts + act_offset, num_acts - act_offset );
      
      if( rc == (signed)(num_acts - act_offset) ) {
         
         // not found 
         break;
      }
      else if( rc < 0 ) {
         
         vdev_error("vdev_action_find_next(%s, offset = %d) rc = %d\n", vreq->path, act_offset, rc );
         break;
      }
      else {
         
         // matched! advance offset to next acl
         i = act_offset + rc;
         act_offset += rc + 1;
         
         // what kind of action?
         if( acts[i].async ) {
            
            method = "vdev_action_run_async";
            rc = vdev_action_run_async( &acts[i] );
         }
         else {
            
            method = "vdev_action_run_sync";
            rc = vdev_action_run_sync( &acts[i] );
         }
         
         if( rc != 0 ) {
            
            vdev_error("%s('%s') rc = %d\n", method, acts[i].command, rc );
            return rc;
         }
      }
   }
   
   return 0;
}


