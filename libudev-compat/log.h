/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

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

// provide compatibility for libudev-compat, so it doesn't need to talk to journald or syslog or whatever.

#ifndef _LIBUDEV_COMPAT_LOG_H_
#define _LIBUDEV_COMPAT_LOG_H_

#include <assert.h>
#include <syslog.h>

extern int LIBUDEV_COMPAT_LOG_DEBUG;
extern int LIBUDEV_COMPAT_LOG_TRACE;
extern int LIBUDEV_COMPAT_LOG_ERROR;

#define LIBUDEV_COMPAT_WHERESTR "%05d: [%16s:%04u] %s: "
#define LIBUDEV_COMPAT_WHEREARG (int)getpid(), __FILE__, __LINE__, __func__

#define log_trace(x, ...) if( LIBUDEV_COMPAT_LOG_TRACE ) { fprintf(stderr, LIBUDEV_COMPAT_WHERESTR x "\n", LIBUDEV_COMPAT_WHEREARG, __VA_ARGS__); }
#define log_debug(x, ...) if( LIBUDEV_COMPAT_LOG_DEBUG ) { fprintf(stderr, LIBUDEV_COMPAT_WHERESTR x "\n", LIBUDEV_COMPAT_WHEREARG, __VA_ARGS__); }
#define log_error(x, ...) if( LIBUDEV_COMPAT_LOG_ERROR ) { fprintf(stderr, LIBUDEV_COMPAT_WHERESTR x "\n", LIBUDEV_COMPAT_WHEREARG, __VA_ARGS__); }

void log_set_max_level(int level);
int log_get_max_level(void);
  
#endif