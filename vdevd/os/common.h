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

#ifndef _VDEV_OS_COMMON_H_
#define _VDEV_OS_COMMON_H_

#include "libvdev/util.h"

// connection to the OS's device notification 
struct vdev_os_context {
   
   void* os_cls;        // OS-specific data
   
   bool running;
   
   // reference to global state 
   struct vdev_state* state;
};

C_LINKAGE_BEGIN

// context management
int vdev_os_context_init( struct vdev_os_context* vos, struct vdev_state* state );
int vdev_os_context_free( struct vdev_os_context* vos );

int vdev_os_main( struct vdev_os_context* vos );

C_LINKAGE_END

#endif 