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

#include "opts.h"
#include "vdev.h"

// for options parsing
#define _FILE_OFFSET_BITS 64
#include <fuse.h>

static const char* FUSE_OPT_O = "-o";
static const char* FUSE_OPT_D = "-d";
static const char* FUSE_OPT_F = "-f";

// print usage statement 
int vdev_usage( char const* progname ) {
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
                  \n\
", progname );
  
  return 0;
}


// default options 
static int vdev_opts_default( struct vdev_opts* opts ) {
   
   memset( opts, 0, sizeof(struct vdev_opts) );
   
   opts->config_file_path = vdev_strdup_or_null( VDEV_CONFIG_DEFAULT_PATH );
   opts->debug_level = VDEV_CONFIG_DEFAULT_DEBUG_LEVEL;
   opts->logfile_path = NULL;
   
   return 0;
}


#ifdef _USE_FS

// get the mountpoint option, by parsing the FUSE command line 
static int vdev_opts_get_mountpoint( int fuse_argc, char** fuse_argv, char** ret_mountpoint ) {
   
   struct fuse_args fargs = FUSE_ARGS_INIT(fuse_argc, fuse_argv);
   char* mountpoint = NULL;
   int unused_1;
   int unused_2;
   int rc = 0;
   
   // parse command-line...
   rc = fuse_parse_cmdline( &fargs, &mountpoint, &unused_1, &unused_2 );
   if( rc < 0 ) {

      fskit_error("fuse_parse_cmdline rc = %d\n", rc );
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

#else 

// get the mountpoint option, by taking the last argument that wasn't an optarg
static int vdev_opts_get_mountpoint( int fuse_argc, char** fuse_argv, char** ret_mountpoint ) {
   
   *ret_mountpoint = realpath( fuse_argv[ fuse_argc - 1 ], NULL );
   
   if( *ret_mountpoint == NULL ) {
      return -EINVAL;
   }
   
   return 0;
}

#endif // _USE_FS


// parse command-line options.
// fill in fuse_argv with fuse-specific options.
int vdev_opts_parse( struct vdev_opts* opts, int argc, char** argv, int* fuse_argc, char** fuse_argv ) {
   
   static struct option vdev_options[] = {
      {"config-file",     required_argument,   0, 'c'},
      {"verbose-level",   required_argument,   0, 'v'},
      {"log-file",        required_argument,   0, 'l'},
      {0, 0, 0, 0}
   };

   int rc = 0;
   int opt_index = 0;
   int c = 0;
   int fuse_optind = 0;
   
   char const* optstr = "c:v:l:o:f";
   
   vdev_opts_default( opts );
   
   fuse_argv[fuse_optind] = argv[0];
   fuse_optind++;
   
   while(rc == 0 && c != -1) {
      
      c = getopt_long(argc, argv, optstr, vdev_options, &opt_index);
      
      if( c == -1 )
         break;
      
      switch( c ) {
         
         case 'c': {
            
            if( opts->config_file_path != NULL ) {
               free( opts->config_file_path );
            }
            
            opts->config_file_path = vdev_strdup_or_null( optarg );
            break;
         }
         
         case 'l': {
            
            if( opts->logfile_path != NULL ) {
               free( opts->logfile_path );
            }
            
            opts->logfile_path = vdev_strdup_or_null( optarg );
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
               opts->debug_level = debug_level;  
            }
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
   
   // copy over non-option arguments to fuse_argv 
   for( int i = optind; i < argc; i++ ) {
      
      fuse_argv[ fuse_optind ] = argv[i];
      fuse_optind++;
   }
   
   *fuse_argc = fuse_optind;
   
   // parse FUSE args to get the mountpoint 
   rc = vdev_opts_get_mountpoint( *fuse_argc, fuse_argv, &opts->mountpoint );
   
   return rc;
}


// free opts 
int vdev_opts_free( struct vdev_opts* opts ) {
   
   if( opts->config_file_path != NULL ) {
      
      free( opts->config_file_path );
      opts->config_file_path = NULL;
   }
   
   if( opts->mountpoint != NULL ) {
      
      free( opts->mountpoint );
      opts->mountpoint = NULL;
   }
   
   return 0;
}

