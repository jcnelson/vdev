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

#include "config.h"

#define INI_MAX_LINE 4096
#define INI_STOP_ON_FIRST_ERROR 1

#include "ini.h"

// ini parser callback 
static int vdev_config_ini_parser( void* userdata, char const* section, char const* name, char const* value ) {
   
   struct vdev_config* conf = (struct vdev_config*)userdata;
   bool success = false;
   int rc = 0;
   
   if( strcmp(section, VDEV_CONFIG_NAME) == 0 ) {
      
      
      if( strcmp(name, VDEV_CONFIG_FIRMWARE_DIR) == 0 ) {
         
         // save this 
         conf->firmware_dir = vdev_strdup_or_null( value );
         return 1;
      }
      
      if( strcmp(name, VDEV_CONFIG_ACLS) == 0 ) {
         
         // save this 
         conf->acls_dir = vdev_strdup_or_null( value );
         return 1;
      }
      
      if( strcmp(name, VDEV_CONFIG_ACTIONS) == 0 ) {
         
         // save this 
         conf->acts_dir = vdev_strdup_or_null( value );
         return 1;
      }
      
      if( strcmp(name, VDEV_CONFIG_HELPERS) == 0 ) {
         
         // save this 
         conf->helpers_dir = vdev_strdup_or_null( value );
         return 1;
      }
      
      if( strcmp(name, VDEV_CONFIG_DEFAULT_MODE) == 0 ) {
         
         conf->default_mode = (mode_t)vdev_parse_uint64( value, &success );
         if( !success ) {
            
            fprintf(stderr, "Invalid value '%s' for '%s'\n", value, name );
            return 0;
         }
         else {
            
            return 1;
         }
      }
      
      if( strcmp(name, VDEV_CONFIG_DEFAULT_POLICY) == 0 ) {
         
         conf->default_policy = strcasecmp( value, "allow" ) ? 1 : 0;
         return 1;
      }
      
      fprintf(stderr, "Unrecognized '%s' field '%s'\n", section, name);
      return 1;
   }
   
   if( strcmp( section, VDEV_OS_CONFIG_NAME) == 0 ) {
      
      // OS-specific config value
      rc = vdev_params_add( &conf->os_config, name, value );
      if( rc != 0 ) {
         
         return 0;
      }
      return 1;
   }
   
   fprintf(stderr, "Unrecognized field '%s'\n", name);
   return 1;
}


// config sanity check 
int vdev_config_sanity_check( struct vdev_config* conf ) {
   
   int rc = 0;
   
   if( conf->acls_dir == NULL ) {
      
      fprintf(stderr, "ERR: Config file is missing 'acls='\n");
      rc = -EINVAL;
   }
   
   if( conf->acts_dir == NULL ) {
      
      fprintf(stderr, "ERR: Config file is missing 'actions='\n");
      rc = -EINVAL;
   }
   
   return rc;
}

// initialize a config 
int vdev_config_init( struct vdev_config* conf ) {
   
   memset( conf, 0, sizeof(struct vdev_config) );
   return 0;
}

// load from a file, by path
int vdev_config_load( char const* path, struct vdev_config* conf ) {
   
   FILE* f = NULL;
   int rc = 0;
   
   f = fopen( path, "r" );
   if( f == NULL ) {
      rc = -errno;
      return rc;
   }
   
   rc = vdev_config_load_file( f, conf );
   
   fclose( f );
   return rc;
}

// load from a file
int vdev_config_load_file( FILE* file, struct vdev_config* conf ) {
   
   int rc = 0;
   
   rc = ini_parse_file( file, vdev_config_ini_parser, conf );
   if( rc != 0 ) {
      vdev_error("ini_parse_file(config) rc = %d\n", rc );
   }
   
   return rc;
}

// free a config
int vdev_config_free( struct vdev_config* conf ) {
   
   if( conf->firmware_dir != NULL ) {
      
      free( conf->firmware_dir );
      conf->firmware_dir = NULL;
   }
   
   if( conf->acls_dir != NULL ) {
      
      free( conf->acls_dir );
      conf->acls_dir = NULL;
   }
   
   if( conf->acts_dir != NULL ) {
      
      free( conf->acts_dir );
      conf->acts_dir = NULL;
   }
   
   if( conf->os_config != NULL ) {
      
      vdev_params_free( conf->os_config );
      conf->os_config = NULL;
   }
   
   if( conf->helpers_dir != NULL ) {
      
      free( conf->helpers_dir );
      conf->helpers_dir = NULL;
   }
   
   return 0;
}
