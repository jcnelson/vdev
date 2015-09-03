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

#include "log.h"

#define LIBUDEV_COMPAT_WHERESTR "%05d: [%16s:%04u] %s: "

// set log_max_level to indicate that no LIBUDEV_COMPAT_DEBUG environment variable has been set
#define LIBUDEV_COMPAT_NO_DEBUG -100
#define LIBUDEV_COMPAT_DEBUG LOG_INFO

// undefined by default...
static int log_max_level = -1;

void log_set_max_level(int level) {
    // don't allow invalid log levels (only these methods can do so)
    assert((level & LOG_PRIMASK) == level);

    log_max_level = level;
}

static void log_check_disabled(void) {
    
    if( log_max_level == -1 ) {
        
        // undefined default max log level
        char* libudev_compat_loglevel_str = secure_getenv( LIBUDEV_COMPAT_DEBUG_ENVAR );
        if( libudev_compat_loglevel_str != NULL && strlen(libudev_compat_loglevel_str) > 0 ) {
            
            // enable debugging 
            log_max_level = LIBUDEV_COMPAT_DEBUG;
        }
        else {
            
            // disable debugging
            log_max_level = LIBUDEV_COMPAT_NO_DEBUG;
        }
    }
}

int log_get_max_level(void) {
    
    log_check_disabled();
    return log_max_level;
}

void log_impl( int level, char const* fmt, ... ) {
 
   log_check_disabled();  
   if( level <= log_max_level ) {
      va_list args;
      va_start( args, fmt );
      vfprintf( stderr, fmt, args );
      va_end( args );
   }
}
