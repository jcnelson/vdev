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

#include "opts.h"
#include "vdev.h"

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
   -d, --debug-level DEBUG_LEVEL\n\
                  Set the debug level.  Valid values are positive integers.\n\
                  Larger integers lead to more debugging information.\n\
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

// parse command-line options 
int vdev_opts_parse( struct vdev_opts* opts, int argc, char** argv ) {
   
   static struct option vdev_options[] = {
      {"config-file",     required_argument,   0, 'c'},
      {"debug-level",     required_argument,   0, 'd'},
      {"log-file",        required_argument,   0, 'l'},
      {0, 0, 0, 0}
   };

   int rc = 0;
   int opt_index = 0;
   int c = 0;
   
   char const* optstr = "c:d:l:";
   
   vdev_opts_default( opts );
   
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
         
         case 'd': {
            
            long debug_level = 0;
            char* tmp = NULL;
            
            debug_level = strtol( optarg, &tmp, 10 );
            
            if( tmp == NULL ) {
               fprintf(stderr, "Invalid argument for -d\n");
               rc = -1;
            }
            else {
               opts->debug_level = debug_level;  
            }
            break;
         }
         
         default: {
            
            fprintf(stderr, "Unrecognized option -%c\n", c );
            rc = -1;
            break;
         }
      }
   }
   
   return rc;
}


// free opts 
int vdev_opts_free( struct vdev_opts* opts ) {
   
   if( opts->config_file_path != NULL ) {
      
      free( opts->config_file_path );
      opts->config_file_path = NULL;
   }
   
   return 0;
}

