/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

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

#ifndef _VDEV_H_
#define _VDEV_H_

#include "libvdev/config.h"
#include "libvdev/util.h"
#include "os/common.h"
#include "device.h"
#include "workqueue.h"

// global vdev state 
struct vdev_state {
   
   // configuration 
   struct vdev_config* config;
   
   // arguments 
   int argc;
   char** argv;
   
   // mountpoint; where /dev is
   char* mountpoint;
   
   // OS context
   struct vdev_os_context* os;
   
   // actions
   struct vdev_action* acts;
   size_t num_acts;
   
   // pending requests
   struct vdev_pending_context* pending;
   
   // device processing workqueue (back-end)
   struct vdev_wq device_wq;
   
   // are we taking events from the OS? (back-end)
   bool running;
   
   // run once?
   bool once;
   
   // fd to write to to signal when the initial devices have all been processed
   int flush_fd;
};

typedef char* cstr;

C_LINKAGE_BEGIN

int vdev_init( struct vdev_state* vdev, int argc, char** argv );
int vdev_start( struct vdev_state* vdev );
int vdev_main( struct vdev_state* vdev, int flush_fd );
int vdev_stop( struct vdev_state* vdev );
int vdev_shutdown( struct vdev_state* vdev, bool unlink_pidfile );

// NOTE: this is meant to be called by the workqueue
int vdev_signal_wq_flushed( struct vdev_state* state );

int vdev_preseed_run( struct vdev_state* state );
int vdev_remove_unplugged_devices( struct vdev_state* state );

SGLIB_DEFINE_VECTOR_PROTOTYPES( cstr );

C_LINKAGE_END

#endif
