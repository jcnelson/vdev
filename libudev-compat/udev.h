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

#ifndef _LIBUDEV_COMPAT_UDEV_H_
#define _LIBUDEV_COMPAT_UDEV_H_

#include "util.h"

struct udev;

C_LINKAGE_BEGIN

struct udev* udev_ref( struct udev* udev );
struct udev* udev_unref( struct udev* udev );
struct udev* udev_new( void );

void udev_set_log_fn( struct udev* udev, void (*log_fn)( struct udev*, int, char const*, int, char const*, char const*, va_args ) );
int udev_get_log_priority( struct udev* udev );
void udev_set_log_priority( struct udev* udev, int priority );

void* udev_get_userdata( struct udev* udev );
void udev_set_userdata( struct udev* udev, void* userdata );

C_LINKAGE_END

#endif