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

#ifndef _LIBUDEV_COMPAT_LIST_H_
#define _LIBUDEV_COMPAT_LIST_H_

#include "libvdev/util.h"
#include "libvdev/sglib.h"

struct udev_list_entry;

C_LINKAGE_BEGIN

struct udev_list_entry* udev_list_entry_get_next( struct udev_list_entry* ent );
struct udev_list_entry* udev_list_entry_get_by_name( struct udev_list_entry* ent, char const* name );

char const* udev_list_entry_get_name( struct udev_list_entry* ent );
char const* udev_list_entry_get_value( struct udev_list_entry* ent );

#define udev_list_entry_foreach( list_entry, first_entry ) for( list_entry = first_entry; first_entry != NULL; list_entry = udev_list_entry_get_next( list_entry ) )

C_LINKAGE_END

#endif