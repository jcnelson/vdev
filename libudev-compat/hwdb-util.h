/*
 * hwdb-util.h
 * 
 * Forked from systemd/src/libsystemd/hwdb-util.h on March 26, 2015
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/***
  This file is part of systemd.

  Copyright 2014 Tom Gundersen <teg@jklm.no>

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef _LIBUDEV_COMPAT_HWDB_H_
#define _LIBUDEV_COMPAT_HWDB_H_

#include "util.h"

#include "sd-hwdb.h"

DEFINE_TRIVIAL_CLEANUP_FUNC(sd_hwdb *, sd_hwdb_unref);
#define _cleanup_hwdb_unref_ _cleanup_(sd_hwdb_unrefp)

bool hwdb_validate(sd_hwdb * hwdb);

#endif
