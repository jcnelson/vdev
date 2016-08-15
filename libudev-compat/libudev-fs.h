/*
  This file is part of libudev-compat.
   
  Copyright 2015 Jude Nelson (judecn@gmail.com)

  libudev-compat is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  libudev-compat is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with libudev-compat; If not, see <http://www.gnu.org/licenses/>.
*/

// this module emulates udevd, so libudev-compat clients don't need
// udevd to be running to receive device events

#ifndef _LIBUDEV_COMPAT_FS_H_
#define _LIBUDEV_COMPAT_FS_H_

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/filter.h>
#include <sys/inotify.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/sendfile.h>
#include <sys/uio.h>
#include <sys/epoll.h>

#include "libudev-private.h"

int udev_monitor_fs_setup(struct udev_monitor *monitor);
int udev_monitor_fs_destroy(struct udev_monitor *monitor);
int udev_monitor_fs_shutdown(struct udev_monitor *monitor);
int udev_monitor_fs_push_events(struct udev_monitor *monitor);

#endif
