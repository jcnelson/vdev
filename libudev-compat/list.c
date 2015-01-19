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

#include "list.h"

// simple linked list of (name, value) pairs
struct udev_list_entry {
   
   char* name;
   char* value;
   
   struct udev_list_entry* next;
};

// next
struct udev_list_entry* udev_list_entry_get_next( struct udev_list_entry* ent ) {
   return ent->next;
}

// lookup by name
struct udev_list_entry* udev_list_entry_get_by_name( struct udev_list_entry* ent, char const* name ) {
   
   struct udev_list_entry* itr;
   
   for( itr = ent; itr != NULL; itr = ent->next ) {
      
      if( strcmp( ent->name, name ) == 0 ) {
         return ent;
      }
   }
   
   return NULL;
}


// get name 
char const* udev_list_entry_get_name( struct udev_list_entry* ent ) {
   
   return (const)ent->name;
}

// get value 
char const* udev_list_entry_get_value( struct udev_list_entry* ent ) {
   
   return (const)ent->value;
}
