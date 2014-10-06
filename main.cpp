/*
   vdev: a FUSE filesystem to allow unprivileged users to access privileged files on UNIX-like systems.
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

int main( int argc, char** argv ) {
   
   // don't run as root 
   if( getuid() == 0 || geteuid() == 0 ) {
      fprintf(stderr, "Running vdev as root opens unacceptable security holes\n");
      exit(1);
   }
   
   struct vdev_state state;
   int rc = 0;
   
   // set up library
   rc = fskit_library_init();
   if( rc != 0 ) {
      fprintf( stderr, "fskit_library_init rc = %d\n", rc );
      exit(1);
   }
   
   // set up fskit 
   struct fskit_core* core = CALLOC_LIST( struct fskit_core, 1 );
   
   rc = fskit_core_init( core, NULL );
   if( rc != 0 ) {
      fprintf( stderr, "fskit_core_init rc = %d\n", rc );
      exit(1);
   }
   
   // set up FUSE state
   struct fuse_operations fskit_fuse = vdev_get_opers();
   
   memset( &state, 0, sizeof(struct vdev_state) );
   
   state.core = core;
   
   // run!
   rc = fuse_main(argc, argv, &fskit_fuse, &state );
   
   // clean up
   // blow away all inodes
   rc = fskit_detach_all( core, "/", core->root.children );
   if( rc != 0 ) {
      fprintf( stderr, "fskit_detach_all(\"/\") rc = %d\n", rc );
   }
   
   // destroy the core
   rc = fskit_core_destroy( core, NULL );
   if( rc != 0 ) {
      fprintf( stderr, "fskit_core_destroy rc = %d\n", rc );
   }
   
   // shut down the library
   rc = fskit_library_shutdown();
   if( rc != 0 ) {
      fprintf( stderr, "fskit_library_shutdown rc = %d\n", rc );
   }
   
   free( core );
   
   return rc;
}

