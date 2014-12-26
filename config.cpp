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

#include "config.h"

#define INI_MAX_LINE 4096
#define INI_STOP_ON_FIRST_ERROR 1

#include "ini.h"

// ini parser callback 
static int vdev_config_ini_parser( void* userdata, char const* section, char const* name, char const* value ) {
   
   struct vdev_config* conf = (struct vdev_config*)userdata;
   
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
      
      if( strcmp(name, VDEV_CONFIG_PSTAT) == 0 ) {
         
         if( strcmp(value, VDEV_CONFIG_NAME_PSTAT_HASH) == 0 ) {
            
            // force sha256 check on pstat 
            conf->pstat_discipline |= PSTAT_HASH;
            return 1;
         }
         
         fprintf(stderr, "Invalid value '%s' for '%s'\n", value, name );
         return 0;
      }
      
      fprintf(stderr, "Unrecognized '%s' field '%s'\n", section, name);
      return 1;
   }
   
   if( strcmp( section, VDEV_OS_CONFIG_NAME) == 0 ) {
      
      // OS-specific config value
      try {
         (*conf->os_config)[ string(name) ] = string(value);
         return 1;
      }
      catch( bad_alloc& ba ) {
         
         // out of memory 
         return 0;
      }
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
   
   conf->os_config = new (nothrow) vdev_config_map_t();
   
   if( conf->os_config == NULL ) {
      return -ENOMEM;
   }
   
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
      
      delete conf->os_config;
      conf->os_config = NULL;
   }
   
   return 0;
}
