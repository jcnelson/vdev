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

#include "config.h"

#define INI_MAX_LINE 4096
#define INI_STOP_ON_FIRST_ERROR 1

#include "ini.h"

// FUSE reserved options
static const char* FUSE_OPT_O = "-o";
static const char* FUSE_OPT_D = "-d";
static const char* FUSE_OPT_F = "-f";

// ini parser callback 
// return 1 on parsed (WARNING: masks OOM)
// return 0 on not parsed
static int vdev_config_ini_parser( void* userdata, char const* section, char const* name, char const* value ) {
   
   struct vdev_config* conf = (struct vdev_config*)userdata;
   bool success = false;
   int rc = 0;
   
   if( strcmp( section, VDEV_CONFIG_NAME ) == 0 ) {
      
      
      if( strcmp( name, VDEV_CONFIG_FIRMWARE_DIR ) == 0 ) {
         
         if( conf->firmware_dir == NULL ) {
            // save this 
            conf->firmware_dir = vdev_strdup_or_null( value );
         }
         
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_ACLS ) == 0 ) {
         
         if( conf->acls_dir == NULL ) {
            // save this 
            conf->acls_dir = vdev_strdup_or_null( value );
         }
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_ACTIONS ) == 0 ) {
         
         if( conf->acts_dir == NULL ) {
            // save this 
            conf->acts_dir = vdev_strdup_or_null( value );
         }
         
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_HELPERS ) == 0 ) {
         
         if( conf->helpers_dir == NULL ) {
            // save this 
            conf->helpers_dir = vdev_strdup_or_null( value );
         }
         
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_DEFAULT_MODE ) == 0 ) {
      
         char* tmp = NULL;
         conf->default_mode = (mode_t)strtoul( value, &tmp, 8 );
         
         if( *tmp != '\0' ) {
            
            fprintf(stderr, "Invalid value '%s' for '%s'\n", value, name );
            return 0;
         }
         else {
            
            return 1;
         }
      }
      
      if( strcmp( name, VDEV_CONFIG_DEFAULT_POLICY ) == 0 ) {
         
         conf->default_policy = strcasecmp( value, "allow" ) ? 1 : 0;
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_PIDFILE_PATH ) == 0 ) {
         
         if( conf->pidfile_path == NULL ) {
            conf->pidfile_path = vdev_strdup_or_null( value );
         }
         
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_LOGFILE_PATH ) == 0 ) {
         
         if( conf->logfile_path == NULL ) {
            conf->logfile_path = vdev_strdup_or_null( value );
         }
         
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_LOG_LEVEL ) == 0 ) {
         
         if( strcasecmp( name, "debug" ) == 0 ) {
            
            conf->debug_level = 2;
            conf->error_level = 2;
         }
         else if( strcasecmp( name, "info" ) == 0 ) {
            
            conf->debug_level = 1;
            conf->error_level = 2;
         }
         else if( strcasecmp( name, "warn" ) == 0 || strcasecmp( name, "warning" ) == 0 ) {
            
            conf->debug_level = 0;
            conf->error_level = 2;
         }
         else if( strcasecmp( name, "error" ) == 0 || strcasecmp( name, "critical" ) == 0 ) {
            
            conf->debug_level = 0;
            conf->error_level = 1;
         }
         else {
            
            fprintf(stderr, "Unrecognized value '%s' for '%s'\n", value, name );
            return 0;
         }
         
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_MOUNTPOINT ) == 0 ) {
         
         if( conf->mountpoint == NULL ) {
            conf->mountpoint = vdev_strdup_or_null( value );
         }
         
         return 1;
      }
      
      if( strcmp( name, VDEV_CONFIG_ONCE ) == 0 ) {
         
         if( strcasecmp( name, "true" ) == 0 ) {
            
            conf->once = true;
         }
         else if( strcasecmp( name, "false" ) == 0 ) {
            
            conf->once = false;
         }
         else if( !conf->once ) {
            
            // maybe it's 0 or non-zero?
            conf->once = (bool)vdev_parse_uint64( value, &success );
            if( !success ) {
               
               fprintf(stderr, "Invalid value '%s' for '%s'\n", value, name );
               return 0;
            }
            else {
               
               return 1;
            }
         }
      }
      
      if( strcmp( name, VDEV_CONFIG_IFNAMES ) == 0 ) {
         
         if( conf->ifnames_path == NULL ) {
            
            conf->ifnames_path = vdev_strdup_or_null( value );
         }
            
         return 1;  
      }
      
      fprintf(stderr, "Unrecognized '%s' field '%s'\n", section, name);
      return 1;
   }
   
   if( strcmp( section, VDEV_OS_CONFIG_NAME ) == 0 ) {
      
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
      
      fprintf(stderr, "ERR: missing acls\n");
      rc = -EINVAL;
   }
   
   if( conf->acts_dir == NULL ) {
      
      fprintf(stderr, "ERR: missing actions\n");
      rc = -EINVAL;
   }
   
   if( conf->mountpoint == NULL ) {
      
      fprintf(stderr, "ERR: missing mountpoint\n");
      rc = -EINVAL;
   }
   
   return rc;
}


// convert a number between 0 and 16 to its hex representation 
// hex must have at least 2 characters
// always succeeds 
void vdev_bin_to_hex( unsigned char num, char* hex ) {
   
   unsigned char upper = num >> 4;
   unsigned char lower = num & 0xf;
   
   if( upper < 10 ) {
      hex[0] = upper + '0';
   }
   else {
      hex[0] = upper + 'A';
   }
   
   if( lower < 10 ) {
      hex[1] = lower + '0';
   }
   else {
      hex[1] = lower + 'A';
   }
}

// generate an instance nonce 
// NOTE: not thread-safe, since it uses mrand48(3) (can't use /dev/urandom, since it doesn't exist yet)
// always succeeds 
static void vdev_config_make_instance_nonce( struct vdev_config* conf ) {

   char instance[VDEV_CONFIG_INSTANCE_NONCE_LEN];
   
   // generate an instance nonce 
   for( int i = 0; i < VDEV_CONFIG_INSTANCE_NONCE_LEN; i++ ) {
      
      instance[i] = (char)mrand48();
   }
   
   memset( conf->instance_str, 0, VDEV_CONFIG_INSTANCE_NONCE_STRLEN );
   
   for( int i = 0; i < VDEV_CONFIG_INSTANCE_NONCE_LEN; i++ ) {
      
      vdev_bin_to_hex( (unsigned char)instance[i], &conf->instance_str[2*i] );
   }
}
   

// initialize a config 
// always succeeds
int vdev_config_init( struct vdev_config* conf ) {
   
   memset( conf, 0, sizeof(struct vdev_config) );
   return 0;
}

// load from a file, by path
// return on on success
// return -errno on failure to open 
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
   
   if( rc == 0 ) {
      
      vdev_config_make_instance_nonce( conf );
   }
   return rc;
}

// load from a file
// return 0 on success
// return -errno on failure to load
int vdev_config_load_file( FILE* file, struct vdev_config* conf ) {
   
   int rc = 0;
   
   rc = ini_parse_file( file, vdev_config_ini_parser, conf );
   if( rc != 0 ) {
      vdev_error("ini_parse_file(config) rc = %d\n", rc );
   }
   
   return rc;
}

// free a config
// always succeeds
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
   
   if( conf->config_path != NULL ) {
      
      free( conf->config_path );
      conf->config_path = NULL;
   }
   
   if( conf->mountpoint != NULL ) {
      
      free( conf->mountpoint );
      conf->mountpoint = NULL;
   }
   
   if( conf->ifnames_path != NULL ) {
      
      free( conf->ifnames_path );
      conf->ifnames_path = NULL;
   }
   
   return 0;
}


// convert all paths in the config to absolute paths 
// return 0 on success 
// return -ENOMEM on OOM 
// return -ERANGE if cwd is too long
int vdev_config_fullpaths( struct vdev_config* conf ) {
   
   int rc = 0;
   
   char** need_fullpath[] = {
      &conf->firmware_dir,
      &conf->acls_dir,
      &conf->acts_dir,
      &conf->helpers_dir,
      &conf->pidfile_path,
      &conf->logfile_path,
      NULL
   };
   
   char cwd_buf[ PATH_MAX + 1 ];
   memset( cwd_buf, 0, PATH_MAX + 1 );
   
   char* tmp = getcwd( cwd_buf, PATH_MAX );
   if( tmp == NULL ) {
      
      vdev_error("Current working directory exceeds %u bytes\n", PATH_MAX);
      return -ERANGE;
   }
   
   for( int i = 0; need_fullpath[i] != NULL; i++ ) {
      
      if( need_fullpath[i] != NULL && (*need_fullpath[i]) != NULL ) {
         
         if( *(need_fullpath[i])[0] != '/' ) {
               
            // relative path 
            char* new_path = vdev_fullpath( cwd_buf, *(need_fullpath)[i], NULL );
            if( new_path == NULL ) {
               
               return -ENOMEM;
            }
            
            free( *(need_fullpath[i]) );
            *(need_fullpath[i]) = new_path;
         }
      }
   }
   
   return 0;
}



// print usage statement 
int vdev_config_usage( char const* progname ) {
   fprintf(stderr, "\
\
Usage: %s [options] mountpoint\n\
Options include:\n\
\n\
   -c, --config-file CONFIG_FILE\n\
                  Path to the config file to use.\n\
                  \n\
   -v, --verbose-level VERBOSITY\n\
                  Set the level of verbose output.  Valid values are\n\
                  positive integers.  Larger integers lead to more\n\
                  debugging information.\n\
                  \n\
   -l, --log-file LOGFILE_PATH\n\
                  Path to which to log information.\n\
                  Pass 'syslog' to log to syslog, instead of a logfile.\n\
                  \n\
   -1, --once\n\
                  Exit once all extant devices have been processed.\n\
                  \n\
   -f, --foreground\n\
                  Run in the foreground; do not daemonize\n\
                  \n\
   -p, --pidfile PATH\n\
                  Write the PID of the daemon to PATH\n\
", progname );
  
  return 0;
}

// get the mountpoint option, by taking the last argument that wasn't an optarg
static int vdev_config_get_mountpoint_from_fuse( int fuse_argc, char** fuse_argv, char** ret_mountpoint ) {
   
   *ret_mountpoint = realpath( fuse_argv[ fuse_argc - 1 ], NULL );
   
   if( *ret_mountpoint == NULL ) {
      
      int rc = -errno;
      printf("No mountpoint, rc = %d\n", rc);
      
      for( int i = 0; i < fuse_argc; i++ ) {
         printf("argv[%d]: '%s'\n", i, fuse_argv[i]);
      }
      
      return -EINVAL;
   }
   
   return 0;
}

// parse command-line options from argv.
// fill in fuse_argv with fuse-specific options.
// config must be initialized; this method simply augments it 
// return 0 on success 
// return -1 on unrecognized option 
int vdev_config_load_from_args( struct vdev_config* config, int argc, char** argv, int* fuse_argc, char** fuse_argv ) {
   
   static struct option vdev_options[] = {
      {"config-file",     required_argument,   0, 'c'},
      {"verbose-level",   required_argument,   0, 'v'},
      {"logfile",         required_argument,   0, 'l'},
      {"pidfile",         required_argument,   0, 'p'},
      {"once",            no_argument,         0, '1'},
      {"foreground",      no_argument,         0, 'f'},
      {0, 0, 0, 0}
   };

   int rc = 0;
   int opt_index = 0;
   int c = 0;
   int fuse_optind = 0;
   
   char const* optstr = "c:v:l:o:f1p:";
   
   fuse_argv[fuse_optind] = argv[0];
   fuse_optind++;
   
   while(rc == 0 && c != -1) {
      
      c = getopt_long(argc, argv, optstr, vdev_options, &opt_index);
      
      if( c == -1 ) {
         break;
      }
      
      switch( c ) {
         
         case 'c': {
            
            if( config->config_path != NULL ) {
               free( config->config_path );
            }
            
            config->config_path = vdev_strdup_or_null( optarg );
            break;
         }
         
         case 'l': {
            
            if( config->logfile_path != NULL ) {
               free( config->logfile_path );
            }
            
            config->logfile_path = vdev_strdup_or_null( optarg );
            break;
         }
         
         case 'p': {
            
            if( config->pidfile_path != NULL ) {
               free( config->pidfile_path );
            }
            
            config->pidfile_path = vdev_strdup_or_null( optarg );
            break;
         }
         
         case 'v': {
            
            long debug_level = 0;
            char* tmp = NULL;
            
            debug_level = strtol( optarg, &tmp, 10 );
            
            if( *tmp != '\0' ) {
               fprintf(stderr, "Invalid argument for -d\n");
               rc = -1;
            }
            else {
               config->debug_level = debug_level;  
            }
            break;
         }
         
         case '1': {
            
            config->once = true;
            break;
         }
         
         case 'd': {
            // FUSE option 
            fuse_argv[fuse_optind] = (char*)FUSE_OPT_D;
            fuse_optind++;
            break;
         }
         
         case 'f': {
            // FUSE option 
            fuse_argv[fuse_optind] = (char*)FUSE_OPT_F;
            fuse_optind++;
            
            config->foreground = true;
            break;
         }
         
         case 'o': {
            // FUSE option 
            fuse_argv[fuse_optind] = (char*)FUSE_OPT_O;
            fuse_optind++;
            
            fuse_argv[fuse_optind] = optarg;
            fuse_optind++;
            break;
         }
         
         default: {
            
            fprintf(stderr, "Unrecognized option -%c\n", c );
            rc = -1;
            break;
         }
      }
   }
   
   if( rc != 0 ) {
      return rc;
   }
   
   // copy over non-option arguments to fuse_argv 
   for( int i = optind; i < argc; i++ ) {
      
      fuse_argv[ fuse_optind ] = argv[i];
      fuse_optind++;
   }
   
   *fuse_argc = fuse_optind;
   
   // parse FUSE args to get the mountpoint 
   rc = vdev_config_get_mountpoint_from_fuse( *fuse_argc, fuse_argv, &config->mountpoint );
   
   return rc;
}

