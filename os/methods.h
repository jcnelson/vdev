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