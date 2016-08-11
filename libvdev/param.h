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

#ifndef _VDEV_PARAM_H_
#define _VDEV_PARAM_H_

#include "util.h"
#include "sglib.h"

// red-black tree for (string, string) pairs (i.e. named parameter pairs)
struct vdev_param_t {
   
   char* key;
   char* value;
   
   struct vdev_param_t* left;
   struct vdev_param_t* right;
   char color;
};

typedef struct vdev_param_t vdev_params;

#define VDEV_PARAM_CMP( dp1, dp2 ) (strcmp( (dp1)->key, (dp2)->key ))

C_LINKAGE_BEGIN

// vdev_param_t
SGLIB_DEFINE_RBTREE_PROTOTYPES(vdev_params, left, right, color, VDEV_PARAM_CMP)
int vdev_params_add( vdev_params** params, char const* key, char const* value );
int vdev_params_free( vdev_params* params );

C_LINKAGE_END

#endif
