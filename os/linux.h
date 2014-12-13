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

#ifndef _VDEV_OS_LINUX_H_
#define _VDEV_OS_LINUX_H_

// build Linux-specific method implementations
#ifdef _VDEV_OS_LINUX

#include "vdev.h"

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>

#include <linux/types.h>
#include <linux/netlink.h>

#define VDEV_LINUX_NETLINK_BUF_MAX 4096

// connection to the linux kernel for hotplug
struct vdev_linux_context {
   
   // netlink address
   struct sockaddr_nl nl_addr;
   
   // poll on the netlink socket
   struct pollfd pfd;
   
   // thread listening to events 
   bool running;
   pthread_t thread;
   
   // path to mounted sysfs 
   char sysfs_mountpoint[ PATH_MAX+1 ];
   
   // reference to global state 
   struct vdev_state* state;
};

extern "C" {

int vdev_os_init( struct vdev_state* state, void** cls );
int vdev_os_shutdown( void* cls );

int vdev_os_add_device( struct vdev_device_request* request, void* cls );
int vdev_os_remove_device( struct vdev_device_request* request, void* cls );
int vdev_os_change_device( struct vdev_device_request* request, void* cls );
int vdev_os_move_device( struct vdev_device_request* request, void* cls );
int vdev_os_online_device( struct vdev_device_request* request, void* cls );
int vdev_os_offline_device( struct vdev_device_request* request, void* cls );

}

#endif
#endif