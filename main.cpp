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

#include "main.h"

// run! 
int main( int argc, char** argv ) {
   
   int rc = 0;
   int rh = 0;
   struct fskit_fuse_state state;
   struct fskit_core* core = NULL;
   
   rc = fskit_fuse_init( &state, NULL );
   if( rc != 0 ) {
      fprintf(stderr, "fskit_fuse_init rc = %d\n", rc );
      exit(1);
   }
   
   // make sure the fs can access its methods through the VFS
   fskit_fuse_setting_enable( &state, FSKIT_FUSE_SET_FS_ACCESS );
   
   core = fskit_fuse_get_core( &state );
   
   // add handlers.  reads and writes must happen sequentially, since we seek and then perform I/O
   // NOTE: FSKIT_ROUTE_ANY matches any path, and is a macro for the regex "/([^/]+[/]*)+"
   rh = fskit_route_mkdir( core, FSKIT_ROUTE_ANY, vdev_mkdir, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_mkdir(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_mknod( core, FSKIT_ROUTE_ANY, vdev_mknod, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_mknod(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_readdir( core, FSKIT_ROUTE_ANY, vdev_readdir, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_readdir(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_detach( core, FSKIT_ROUTE_ANY, vdev_detach, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_detach(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_stat( core, FSKIT_ROUTE_ANY, vdev_stat, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_stat(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   // set the root to be owned by the effective UID and GID of user
   fskit_chown( core, "/", 0, 0, geteuid(), getegid() );
   
   // run 
   rc = fskit_fuse_main( &state, argc, argv );
   
   // shutdown
   fskit_fuse_shutdown( &state, NULL );
   
   return rc;
}

