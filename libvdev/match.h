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

#ifndef _VDEV_MATCH_H_
#define _VDEV_MATCH_H_

#include "util.h"

#include <regex.h>

C_LINKAGE_BEGIN int vdev_match_regex_init(regex_t * regex, char const *str);
int vdev_match_regex_append(char ***strings, regex_t ** regexes, size_t * len,
			    char const *next);
int vdev_match_regexes_free(char **regex_strs, regex_t * regexes, size_t len);
int vdev_match_regex(char const *path, regex_t * regex);
int vdev_match_first_regex(char const *path, regex_t * regexes,
			   size_t num_regexes);

C_LINKAGE_END
#endif
