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

#ifndef _LIBUDEV_COMPAT_DEVICE_H_
#define _LIBUDEV_COMPAT_DEVICE_H_

#include "libvdev/util.h"

struct udev_device;

C_LINKAGE_BEGIN

struct udev_device* udev_device_ref( struct udev_device* dev );
struct udev_device* udev_device_unref( struct udev_device* dev );

struct udev* udev_device_get_udev( struct udev_device* dev );

struct udev_device* udev_device_new_from_syspath( struct udev* udev, char const* syspath );
struct udev_device* udev_device_new_from_devnum( struct udev* udev, char type, dev_t devnum );
struct udev_device* udev_device_new_from_subsystem_sysname( struct udev* udev, char const* subsystem, char const* sysname );
struct udev_device* udev_device_new_from_device_id( struct udev* udev, char const* id );
struct udev_device* udev_device_new_from_environment( struct udev* udev );

struct udev_device* udev_device_get_parent( struct udev_device* dev );
struct udev_device* udev_device_get_parent_with_subsystem_devtype( struct udev_device* dev, char const* subsystem, char const* devtype );

char const* udev_device_get_devpath( struct udev_device* dev );
char const* udev_device_get_subsystem( struct udev_device* dev );
char const* udev_device_get_devtype( struct udev_device* dev );
char const* udev_device_get_syspath( struct udev_device* dev );
char const* udev_device_get_sysname( struct udev_device* dev );
char const* udev_device_get_sysnum( struct udev_device* dev );
char const* udev_device_get_devnode( struct udev_device* dev );

int udev_device_get_is_initialized( struct udev_device* dev );
struct udev_device* udev_device_get_devlinks_list_entry( struct udev_device* dev );
struct udev_device* udev_device_get_properties_list_entry( struct udev_device* dev );
struct udev_device* udev_device_get_tags_list_entry( struct udev_device* dev );
char const* udev_device_get_property_value( struct udev_device* dev );
char const* udev_device_get_driver( struct udev_device* dev );
dev_t udev_device_get_devnum( struct udev_device* dev );
char const* udev_device_get_action( struct udev_device* dev );
char const* udev_device_get_sysattr_value( struct udev_device* dev, char const* sysattr );
int udev_device_set_sysattr_value( struct udev_device* dev, char const* sysattr, char* value );
struct udev_device* udev_device_get_sysattr_list_entry( struct udev_device* dev );
unsigned long long int udev_device_get_seqnum( struct udev_device* dev );
unsigned long long int udev_device_get_usec_since_initialized( struct udev_device* dev );
int udev_device_has_tag( struct udev_device* dev, char const* tag );

C_LINKAGE_END 

#endif