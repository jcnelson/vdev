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

#include "param.h"

// sglib methods 
SGLIB_DEFINE_RBTREE_FUNCTIONS(vdev_params, left, right, color, VDEV_PARAM_CMP);

// add a parameter to a params set
// return 0 on success
// return -EEXIST if the parameter exists
// return -ENOMEM if OOM
int vdev_params_add(vdev_params ** params, char const *key, char const *value)
{

	char *key_dup = NULL;
	char *value_dup = NULL;
	struct vdev_param_t *new_param = NULL;

	struct vdev_param_t lookup;

	// exists?  fill in requisite comparator information and check
	memset(&lookup, 0, sizeof(lookup));
	lookup.key = (char *)key;

	new_param = sglib_vdev_params_find_member(*params, &lookup);

	if (new_param != NULL) {
		return -EEXIST;
	}

	new_param = VDEV_CALLOC(struct vdev_param_t, 1);

	if (new_param == NULL) {
		return -ENOMEM;
	}

	key_dup = vdev_strdup_or_null(key);

	if (key_dup == NULL) {

		free(new_param);
		return -ENOMEM;
	}

	value_dup = vdev_strdup_or_null(value);

	if (value_dup == NULL) {

		free(new_param);
		free(key_dup);
		return -ENOMEM;
	}

	new_param->key = key_dup;
	new_param->value = value_dup;

	sglib_vdev_params_add(params, new_param);

	return 0;
}

// free params 
int vdev_params_free(vdev_params * params)
{

	struct sglib_vdev_params_iterator itr;
	struct vdev_param_t *dp = NULL;

	for (dp = sglib_vdev_params_it_init_inorder(&itr, params); dp != NULL;
	     dp = sglib_vdev_params_it_next(&itr)) {

		if (dp->key != NULL) {

			free(dp->key);
			dp->key = NULL;
		}

		if (dp->value != NULL) {

			free(dp->value);
			dp->value = NULL;
		}
	}

	for (dp = sglib_vdev_params_it_init(&itr, params); dp != NULL;
	     dp = sglib_vdev_params_it_next(&itr)) {

		free(dp);
	}

	return 0;
}
