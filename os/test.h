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

#ifndef _VDEV_OS_TEST_H_
#define _VDEV_OS_TEST_H_

// OS test methods--populate devices from a series of synthetic netlink messages
#ifdef _VDEV_OS_TEST

#include "vdev.h"

#define VDEV_TEST_SECTION       "OS-test"
#define VDEV_TEST_NAME_EVENT    "ACTION"
#define VDEV_TEST_NAME_PATH     "DEVPATH"
#define VDEV_TEST_NAME_SUBSYSTEM "SUBSYSTEM"
#define VDEV_TEST_NAME_MAJOR    "MAJOR"
#define VDEV_TEST_NAME_MINOR    "MINOR"


typedef vector<struct vdev_device_request*> vdev_test_device_list_t;

struct vdev_test_context {
   
   vdev_test_device_list_t* devlist;
   
   pthread_mutex_t devlist_lock;
   sem_t devlist_sem;
   
   char* events_dir;
};

extern "C" {

int vdev_os_init( struct vdev_os_context* ctx, void** cls );
int vdev_os_start( void* cls );
int vdev_os_stop( void* cls );
int vdev_os_shutdown( void* cls );

int vdev_os_next_device( struct vdev_device_request* request, void* cls );

}

#endif

#endif