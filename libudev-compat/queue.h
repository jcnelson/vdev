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

#ifndef _LIBUDEV_COMPAT_QUEUE_H_
#define _LIBUDEV_COMPAT_QUEUE_H_

#include "util.h"

struct udev_queue;

C_LINKAGE_BEGIN

struct udev_queue* udev_queue_ref( struct udev_queue* queue );
struct udev_queue* udev_queue_unref( struct udev_queue* queue );
struct udev_queue* udev_queue_new( struct udev* udev );

struct udev* udev_queue_get_udev( struct udev_queue* queue );

int udev_queue_get_udev_is_active( struct udev_queue* queue );

int udev_queue_get_queue_is_empty( struct udev_queue* queue );
int udev_queue_get_seqnum_is_finished( struct udev_queue* queue );
int udev_queue_get_seqnum_sequence_is_finished( struct udev_queue* queue );

struct udev_list_entry* udev_queue_get_queued_list_entry( struct udev_queue* queue );

unsigned long long int udev_queue_get_kernel_seqnum( struct udev_queue* queue );
unsigned long long int udev_queue_get_udev_seqnum( struct udev_queue* queue );

int udev_queue_get_fd( struct udev_queue* queue );
int udev_queue_flush( struct udev_queue* queue );

C_LINKAGE_END

#endif