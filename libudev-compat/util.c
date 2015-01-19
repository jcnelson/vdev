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

#include "util.h"

// return 0 if the whole string was copied
// return non-zero otherwise 
int udev_util_encode_string( char const* str, char* str_enc, size_t len ) {
   
   int k = 0;
   size_t base_len = strlen(str);
   
   for( int i = 0; i < base_len && k < len; i++ ) {
      
      // ASCII printable characters are between 0x20 and 0x7E by convention 
      if( str[i] >= 0x20 && str[i] <= 0x7E ) {
         str_enc[k] = str[i];
      }
      else {
         
         if( k + 1 < len ) {
            // encode non-printable 
            sprintf( str_enc + k, "%02x", str[i] );
            k += 2;
         }
         else {
            // out of space 
            return 1;
         }
      }
   }
   
   if( k >= len ) {
      // out of space
      return 1;
   }
   
   return 0;
}