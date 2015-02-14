/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

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

#include "match.h"

// parse a regex 
int vdev_match_regex_init( regex_t* regex, char const* str ) {
   
   int rc = 0;
   
   rc = regcomp( regex, str, REG_EXTENDED | REG_NEWLINE );
   
   if( rc != 0 ) {
      
      return -EINVAL;
   }
   else {
      return 0;
   }
}

// parse a regex and append it to a list of regexes and strings
// return 0 on success
// return -EINVAL if the regex is invalid
// return -ENOMEM if we're out of memory
int vdev_match_regex_append( char*** strings, regex_t** regexes, size_t* len, char const* next ) {
   
   // verify that this is a valid regex 
   regex_t reg;
   int rc = 0;
   
   memset( &reg, 0, sizeof(regex_t) );
   
   rc = regcomp( &reg, next, REG_EXTENDED | REG_NEWLINE | REG_NOSUB );
   if( rc != 0 ) {
      
      vdev_error("regcomp(%s) rc = %d\n", next, rc );
      return -EINVAL;
   }
   
   char** new_strings = (char**)realloc( *strings, sizeof(char**) * (*len + 2) );
   regex_t* new_regexes = (regex_t*)realloc( *regexes, sizeof(regex_t) * (*len + 1) );
   
   if( new_strings == NULL || new_regexes == NULL ) {
      return -ENOMEM;
   }
   
   new_strings[*len] = vdev_strdup_or_null( next );
   new_strings[*len + 1] = NULL;
   
   new_regexes[*len] = reg;
   
   *strings = new_strings;
   *regexes = new_regexes;
   
   *len = *len + 1;
   
   return 0;
}


// free a list of regexes and their associated paths
int vdev_match_regexes_free( char** regex_strs, regex_t* regexes, size_t len ) {
   
   if( regex_strs != NULL || regexes != NULL ) {
      
      for( unsigned int i = 0; i < len; i++ ) {
         
         if( regex_strs != NULL && regex_strs[i] != NULL ) {
            
            free( regex_strs[i] );
            regex_strs[i] = NULL;
         }
         
         if( regexes != NULL ) {
            regfree( &regexes[i] );
         }
      }
      
      if( regex_strs != NULL ) {
         
         free( regex_strs );
      }
      
      if( regexes != NULL ) {
         
         free( regexes );
      }
   }
   
   return 0;
}

// does a path match a regex?
// return 1 if so, 0 if not, negative on error 
int vdev_match_regex( char const* path, regex_t* regex ) {
   
   int rc = 0;
   
   rc = regexec( regex, path, 0, NULL, 0 );
   
   if( rc != 0 ) {
      if( rc == REG_NOMATCH ) {
         
         // no match 
         return 0;
      }
      else {
         vdev_error("regexec(%s) rc = %d\n", path, rc );
         return -abs(rc);
      }
   }
   
   // match!
   return 1;
}


// does a path match any regexes in a list?
// return the index of the match if so (>= 0)
// return the size if not
// return negative on error
int vdev_match_first_regex( char const* path, regex_t* regexes, size_t num_regexes ) {
   
   int matched = 0;
   
   for( unsigned int i = 0; i < num_regexes; i++ ) {
      
      matched = vdev_match_regex( path, &regexes[i] );
      if( matched > 0 ) {
         return i;
      }
      else if( matched < 0 ) {
         vdev_error("vdev_acl_regex_match(%s) rc = %d\n", path, matched );
         return matched;
      }
   }
   
   return num_regexes;
}
