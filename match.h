/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _VDEV_MATCH_H_
#define _VDEV_MATCH_H_

#include "util.h"

#include <regex.h>


extern "C" {

int vdev_match_regex_init( regex_t* regex, char const* str );
int vdev_match_regex_append( char*** strings, regex_t** regexes, size_t* len, char const* next );
int vdev_match_regexes_free( char** regex_strs, regex_t* regexes, size_t len );
int vdev_match_regex( char const* path, regex_t* regex );
int vdev_match_first_regex( char const* path, regex_t* regexes, size_t num_regexes );
   
}

#endif 