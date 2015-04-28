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

// determine what kind of busses we're on
#include "common.h"

void usage( char const* progname ) {
   
   fprintf(stderr, "Usage: %s /sysfs/path/to/device\n", progname );
   exit(1);
}

// lop off the child of a directory 
// return 0 on success
// return -ENOENT if there is no child to remove
int remove_child( char* path ) {

   char* delim = NULL;
   
   delim = strrchr( path, '/' );
   if( delim == NULL || delim == path ) {
      return -ENOENT;
   }
   
   while( *delim == '/' && delim != path ) {
      *delim = '\0';
      delim--;
   }
   
   return 0;
}
   
int main( int argc, char** argv ) {
   
   if( argc != 2 ) {
      usage( argv[0] );
   }
   
   char* next_bus = NULL;
   char subsystem_list[ 4097 ];
   char subsystem_list_uniq[ 4097 ];
   char* delim = NULL;
   char* subsystem_buf = (char*)calloc( strlen(argv[1]) + strlen("/subsystem") + 1, 1 );
   char readlink_buf[ 4097 ];
   int rc = 0;
   struct stat sb;
   
   if( subsystem_buf == NULL ) {
      exit(1);
   }
   
   next_bus = argv[1];
   memset( subsystem_list, 0, 4097 );
   memset( subsystem_list_uniq, 0, 4097 );
   
   // directory must exist 
   rc = stat( next_bus, &sb );
   if( rc != 0 ) {
      exit(1);
   }
   
   while( 1 ) {
      
      // what's the subsystem here?
      sprintf( subsystem_buf, "%s/subsystem", next_bus );
      
      rc = lstat( subsystem_buf, &sb );
      if( rc != 0 ) {
         
         rc = remove_child( next_bus );
         if( rc != 0 ) {
            break;
         }
         else {
            continue;
         }
      }
      
      if( !S_ISLNK( sb.st_mode ) ) {
         
         rc = remove_child( next_bus );
         if( rc != 0 ) {
            break;
         }
         else {
            continue;
         }
      }
      
      memset( readlink_buf, 0, 4097 );
      
      rc = readlink( subsystem_buf, readlink_buf, 4096 );
      if( rc < 0 ) {
         
         rc = remove_child( next_bus );
         if( rc != 0 ) {
            break;
         }
         else {
            continue;
         }
      }
      
      delim = strrchr( readlink_buf, '/' );
      if( delim == NULL ) {
         
         rc = remove_child( next_bus );
         if( rc != 0 ) {
            break;
         }
         else {
            continue;
         }
      }
      
      // delim + 1 is the subsystem name 
      if( subsystem_list[0] != 0 ) {
         strncat( subsystem_list, ",", 4096 - strlen(subsystem_list) - 1 );
      }
      strncat( subsystem_list, delim + 1, 4096 - strlen(subsystem_list) - 1 );
      
      if( strstr( subsystem_list_uniq, delim + 1 ) == NULL ) {
         
         // haven't seen this subsystem name before...
         if( subsystem_list_uniq[0] != 0 ) {
            strncat( subsystem_list_uniq, ",", 4096 - strlen(subsystem_list_uniq) - 1 );
         }
         strncat( subsystem_list_uniq, delim + 1, 4096 - strlen(subsystem_list_uniq) - 1 );
      }
      
      rc = remove_child( next_bus );
      if( rc != 0 ) {
         break;
      }
      else {
         continue;
      }
   }
   
   free( subsystem_buf );
   
   vdev_property_add( "VDEV_BUS_SUBSYSTEMS", subsystem_list );
   vdev_property_add( "VDEV_BUS_SUBSYSTEMS_UNIQ", subsystem_list_uniq );
   
   vdev_property_print();
   vdev_property_free_all();
   
   exit(0);
}
