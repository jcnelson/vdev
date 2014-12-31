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

#ifndef _VDEV_OS_METHODS_H_
#define _VDEV_OS_METHODS_H_

#include "vdev.h"
#include "common.h"

// add OS-specific headers here
#ifdef _VDEV_OS_LINUX
#include "linux.h"
#endif

#ifdef _VDEV_OS_TEST
#include "test.h"
#endif

extern "C" {

int vdev_os_init( struct vdev_os_context* ctx, void** cls );
int vdev_os_shutdown( void* cls );

// yield the next device 
int vdev_os_next_device( struct vdev_device_request* request, void* cls );

}

#endif 