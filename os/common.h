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

#ifndef _VDEV_OS_COMMON_H_
#define _VDEV_OS_COMMON_H_

#include "util.h"

// connection to the OS's device notification 
struct vdev_os_context {
   
   void* os_cls;        // OS-specific data
   
   bool running;
   
   // reference to global state 
   struct vdev_state* state;
};

extern "C" {

// context management
int vdev_os_context_init( struct vdev_os_context* vos, struct vdev_state* state );
int vdev_os_context_free( struct vdev_os_context* vos );

int vdev_os_main( struct vdev_os_context* vos );

}

#endif 