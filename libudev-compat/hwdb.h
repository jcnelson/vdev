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

#ifndef _LIBUDEV_COMPAT_HWDB_H_
#define _LIBUDEV_COMPAT_HWDB_H_

#include "util.h"
#include "udev.h"
#include "list.h"

struct udev_hwdb;

C_LINKAGE_BEGIN

struct udev_hwdb* udev_hwdb_ref( struct udev_hwdb* hwdb );
struct udev_hwdb* udev_hwdb_unref( struct udev_hwdb* hwdb );
struct udev_hwdb* udev_hwdb_new( struct udev* udev );

struct udev_list_entry* udev_hwdb_get_properties_list_entry( struct udev_hwdb* hwdb, char const* modalias, unsigned int flags );

C_LINKAGE_END

#endif