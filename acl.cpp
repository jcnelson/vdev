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

#include "acl.h"

#define INI_MAX_LINE 4096 
#define INI_STOP_ON_FIRST_ERROR 1

#include "ini.h"
#include "vdev.h"

// parse an unsigned 64-bit number 
static uint64_t vdev_parse_uint64( char const* uint64_str, bool* success ) {
   
   char* tmp = NULL;
   uint64_t uid = (uint64_t)strtoll( uint64_str, &tmp, 10 );
   
   if( tmp == NULL ) {
      *success = false;
   }
   else {
      *success = true;
   }
   
   return uid;
}

// get a passwd struct for a user.
// on success, fill in pwd and *pwd_buf (the caller must free *pwd_buf, but not pwd).
// return 0 on success
static int vdev_get_passwd( char const* username, struct passwd* pwd, char** pwd_buf ) {
   
   struct passwd* result = NULL;
   char* buf = NULL;
   int buf_len = 0;
   int rc = 0;
   
   memset( pwd, 0, sizeof(struct passwd) );
   
   buf_len = sysconf( _SC_GETPW_R_SIZE_MAX );
   if( buf_len <= 0 ) {
      buf_len = 65536;
   }
   
   buf = VDEV_CALLOC( char, buf_len );
   if( buf == NULL ) {
      return -ENOMEM;
   }
   
   rc = getpwnam_r( username, pwd, buf, buf_len, &result );
   
   if( result == NULL ) {
      
      if( rc == 0 ) {
         free( buf );
         return -ENOENT;
      }
      else {
         rc = -errno;
         free( buf );
         
         vdev_error("getpwnam_r(%s) errno = %d\n", username, rc);
         return rc;
      }
   }
   
   // success!
   return rc;
}


// free up memory returned by vdev_get_passwd
static int vdev_free_passwd( struct passwd* pwd, char* pwd_buf ) {
   if( pwd_buf != NULL ) {
      free( pwd_buf );
   }
   
   memset( pwd, 0, sizeof(struct passwd) );
   return 0;
}


// get a group struct for a group.
// on success, fill in grp and *grp_buf (the caller must free *grp_buf, but not grp).
// return 0 on success
static int vdev_get_group( char const* groupname, struct group* grp, char** grp_buf ) {
   
   struct group* result = NULL;
   char* buf = NULL;
   int buf_len = 0;
   int rc = 0;
   
   memset( grp, 0, sizeof(struct group) );
   
   buf_len = sysconf( _SC_GETGR_R_SIZE_MAX );
   if( buf_len <= 0 ) {
      buf_len = 65536;
   }
   
   buf = VDEV_CALLOC( char, buf_len );
   if( buf == NULL ) {
      return -ENOMEM;
   }
   
   rc = getgrnam_r( groupname, grp, buf, buf_len, &result );
   
   if( result == NULL ) {
      
      if( rc == 0 ) {
         free( buf );
         return -ENOENT;
      }
      else {
         rc = -errno;
         free( buf );
         
         vdev_error("getgrnam_r(%s) errno = %d\n", groupname, rc);
         return rc;
      }
   }
   
   // success!
   return rc;
}


// free up memory returned by vdev_get_group
static int vdev_free_group( struct group* grp, char* grp_buf ) {
   if( grp_buf != NULL ) {
      free( grp_buf );
   }
   
   memset( grp, 0, sizeof(struct passwd) );
   return 0;
}

// parse a "uid" field.
// translate a username into a uid, if needed
// return 0 and set uid on success 
// return negative on error 
static int vdev_acl_parse_uid( char const* uid_str, uid_t* uid ) {
   
   bool parsed = false;
   int rc = 0;
   
   *uid = (uid_t)vdev_parse_uint64( uid_str, &parsed );
   
   // not a number?
   if( !parsed ) {
      
      // probably a username 
      char* pwd_buf = NULL;
      struct passwd pwd;
      
      // look up the uid...
      rc = vdev_get_passwd( uid_str, &pwd, &pwd_buf );
      if( rc != 0 ) {
         
         vdev_error("vdev_get_passwd(%s) rc = %d\n", uid_str, rc );
         return rc;
      }
      
      *uid = pwd.pw_uid;
      
      vdev_free_passwd( &pwd, pwd_buf );
   }
   
   return 0;
}


// parse a GID 
// translate a group name into a gid, if needed
// return 0 and set gid on success 
// return negative on error
static int vdev_acl_parse_gid( char const* gid_str, gid_t* gid ) {
   
   bool parsed = false;
   int rc = 0;
   
   *gid = (gid_t)vdev_parse_uint64( gid_str, &parsed );
   
   // not a number?
   if( !parsed ) {
      
      // probably a username 
      char* grp_buf = NULL;
      struct group grp;
      
      // look up the gid...
      rc = vdev_get_group( gid_str, &grp, &grp_buf );
      if( rc != 0 ) {
         
         vdev_error("vdev_get_passwd(%s) rc = %d\n", gid_str, rc );
         return rc;
      }
      
      *gid = grp.gr_gid;
      
      vdev_free_group( &grp, grp_buf );
   }
   
   return 0;
}


// verify that a UID is valid 
static int vdev_acl_validate_uid( uid_t uid ) {
   
   struct passwd* result = NULL;
   char* buf = NULL;
   int buf_len = 0;
   int rc = 0;
   struct passwd pwd;
   
   memset( &pwd, 0, sizeof(struct passwd) );
   
   buf_len = sysconf( _SC_GETPW_R_SIZE_MAX );
   if( buf_len <= 0 ) {
      buf_len = 65536;
   }
   
   buf = VDEV_CALLOC( char, buf_len );
   if( buf == NULL ) {
      return -ENOMEM;
   }
   
   rc = getpwuid_r( uid, &pwd, buf, buf_len, &result );
   
   if( result == NULL ) {
      
      if( rc == 0 ) {
         free( buf );
         return -ENOENT;
      }
      else {
         rc = -errno;
         free( buf );
         
         vdev_error("getpwuid_r(%" PRIu64 ") errno = %d\n", uid, rc);
         return rc;
      }
   }
   
   if( uid != pwd.pw_uid ) {
      rc = -EINVAL;
   }
   
   vdev_free_passwd( &pwd, buf );
   
   return rc;
}

// verify that a GID is valid 
static int vdev_acl_validate_gid( gid_t gid ) {
   
   struct group* result = NULL;
   struct group grp;
   char* buf = NULL;
   int buf_len = 0;
   int rc = 0;
   
   memset( &grp, 0, sizeof(struct group) );
   
   buf_len = sysconf( _SC_GETGR_R_SIZE_MAX );
   if( buf_len <= 0 ) {
      buf_len = 65536;
   }
   
   buf = VDEV_CALLOC( char, buf_len );
   if( buf == NULL ) {
      return -ENOMEM;
   }
   
   rc = getgrgid_r( gid, &grp, buf, buf_len, &result );
   
   if( result == NULL ) {
      
      if( rc == 0 ) {
         free( buf );
         return -ENOENT;
      }
      else {
         rc = -errno;
         free( buf );
         
         vdev_error("getgrgid_r(%" PRIu64 ") errno = %d\n", gid, rc);
         return rc;
      }
   }
   
   if( gid != grp.gr_gid ) {
      rc = -EINVAL;
   }
   
   vdev_free_group( &grp, buf );
   
   return rc;
}

// parse the ACL mode 
static int vdev_acl_parse_mode( mode_t* mode, char const* mode_str ) {
   
   char* tmp = NULL;
   
   *mode = ((mode_t)strtol( mode_str, &tmp, 8 )) & 0777;
   
   if( tmp == NULL ) {
      return -EINVAL;
   }
   else {
      return 0;
   }
}


// parse a regex and append it to a list of regexes and strings
static int vdev_acl_append_regex( char*** strings, regex_t** regexes, size_t len, char const* next ) {
   
   // verify that this is a valid regex 
   regex_t reg;
   int rc = 0;
   
   rc = regcomp( &reg, next, REG_EXTENDED | REG_NEWLINE );
   if( rc != 0 ) {
      
      vdev_error("regcomp(%s) rc = %d\n", next, rc );
      return -EINVAL;
   }
   
   char** new_strings = (char**)realloc( *strings, sizeof(char**) * (len + 2) );
   regex_t* new_regexes = (regex_t*)realloc( *regexes, sizeof(regex_t) * (len + 1) );
   
   if( new_strings == NULL || new_regexes == NULL ) {
      return -ENOMEM;
   }
   
   new_strings[len] = vdev_strdup_or_null( next );
   new_strings[len + 1] = NULL;
   
   new_regexes[len] = reg;
   
   *strings = new_strings;
   *regexes = new_regexes;
   
   return 0;
}


// convert a printable sha256 to a binary sha256 of length SHA256_DIGEST_LENGTH
// return 0 on success and fill in *sha256_bin to point to an allocated buffer
// return negative on error
static int vdev_sha256_to_bin( char const* sha256_printable, unsigned char** sha256_bin ) {
   
   unsigned char* ret = NULL;
   
   // sanity check 
   if( strlen(sha256_printable) != 2 * SHA256_DIGEST_LENGTH ) {
      return -EINVAL;
   }
   
   // sanity check
   for( int i = 0; i < 2 * SHA256_DIGEST_LENGTH; i++ ) {
      
      int c = sha256_printable[i];
      
      // must be a hex number 
      if( ! ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) ) {
         return -EINVAL;
      }
   }
   
   ret = VDEV_CALLOC( unsigned char, SHA256_DIGEST_LENGTH );
   
   if( ret == NULL ) {
      return -ENOMEM;
   }
   
   for( int i = 0; i < SHA256_DIGEST_LENGTH; i++ ) {
      
      char buf[2];
      
      memcpy( buf, sha256_printable + (2 * i), 2 );
      
      ret[i] = (unsigned char) strtol( buf, NULL, 16 );
   }
   
   *sha256_bin = ret;
   
   return 0;
}



// callback from inih to parse an acl 
static int vdev_acl_ini_parser( void* userdata, char const* section, char const* name, char const* value ) {
   
   struct vdev_acl* acl = (struct vdev_acl*)userdata;
   int rc = 0;
   
   // verify this is an acl section 
   if( strcmp(section, VDEV_ACL_NAME) != 0 ) {
      
      fprintf(stderr, "Invalid section '%s'\n", section);
      return 0;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_UID ) == 0 ) {
      
      // parse user or UID 
      uid_t uid = 0;
      int rc = vdev_acl_parse_uid( value, &uid );
      
      if( rc != 0 ) {
         vdev_error("vdev_acl_parse_uid(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid user/UID '%s'\n", value );
         return 0;
      }
      
      acl->has_uid = true;
      acl->uid = uid;
      return 1;
   }
      
   if( strcmp(name, VDEV_ACL_NAME_SETUID ) == 0 ) {
      
      // parse user or UID 
      uid_t uid = 0;
      int rc = vdev_acl_parse_uid( value, &uid );
      
      if( rc != 0 ) {
         vdev_error("vdev_acl_parse_uid(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid user/UID '%s'\n", value );
         return 0;
      }
      
      acl->has_setuid = true;
      acl->setuid = uid;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_GID ) == 0 ) {
      
      // parse group or GID 
      gid_t gid = 0;
      int rc = vdev_acl_parse_gid( value, &gid );
      
      if( rc != 0 ) {
         vdev_error("vdev_acl_parse_gid(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid group/GID '%s'\n", value );
         return 0;
      }
      
      acl->has_gid = true;
      acl->gid = gid;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_SETGID ) == 0 ) {
      
      // parse group or GID 
      gid_t gid = 0;
      int rc = vdev_acl_parse_gid( value, &gid );
      
      if( rc != 0 ) {
         vdev_error("vdev_acl_parse_gid(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid group/GID '%s'\n", value );
         return 0;
      }
      
      acl->has_setgid = true;
      acl->setgid = gid;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_SETMODE ) == 0 ) {
      
      // parse mode 
      mode_t mode = 0;
      int rc = vdev_acl_parse_mode( &mode, value );
      
      if( rc != 0 ) {
         vdev_error("vdev_acl_parse_mode(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid mode '%s'\n", value );
         return 0;
      }
      
      acl->has_setmode = true;
      acl->setmode = mode;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_PROC_PATH ) == 0 ) {
      
      // parse and preserve this value 
      int rc = vdev_acl_append_regex( &acl->paths, &acl->regexes, acl->num_paths, value );
      
      if( rc != 0 ) {
         vdev_error("vdev_acl_append_regex(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid regex '%s'\n", value );
         return 0;
      }
      
      acl->num_paths++;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_PROC_PATH ) == 0 ) {
      
      // preserve this value 
      if( acl->proc_path != NULL ) {
         
         fprintf(stderr, "Duplicate process path '%s'\n", value );
         return 0;
      }
      
      acl->proc_path = strdup(value);
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_PROC_SHA256 ) == 0 ) {
      
      // preserve this value 
      if( acl->proc_sha256 != NULL ) {
         
         fprintf(stderr, "Duplicate process SHA256 '%s'\n", value );
         return 0;
      }
      
      rc = vdev_sha256_to_bin( value, &acl->proc_sha256 );
      
      if( rc != 0 ) {
         fprintf(stderr, "Failed to parse '%s' as a SHA256 hash\n", value );
         return 0;
      }
      
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_PROC_PIDLIST ) == 0 ) {
      
      // preserve this value 
      if( acl->proc_pidlist_cmd != NULL ) {
         
         fprintf(stderr, "Duplicate process PID list command '%s'\n", value );
         return 0;
      }
      
      acl->proc_pidlist_cmd = strdup(value);
      return 1;
   }
   
   fprintf(stderr, "Unrecognized field '%s'\n", name);
   return 0;
}


// initialize an acl 
int vdev_acl_init( struct vdev_acl* acl ) {
   memset( acl, 0, sizeof(struct vdev_acl) );
   return 0;
}

// free regex-related data 
static int vdev_acl_regexes_free( char** regex_strs, regex_t* regexes, size_t len ) {
   
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

// free an acl 
int vdev_acl_free( struct vdev_acl* acl ) {
   
   vdev_acl_regexes_free( acl->paths, acl->regexes, acl->num_paths );
   
   if( acl->proc_path != NULL ) {
      free( acl->proc_path );
      acl->proc_path = NULL;
   }
   
   if( acl->proc_sha256 != NULL ) {
      free( acl->proc_sha256 );
      acl->proc_sha256 = NULL;
   }
   
   return 0;
}


// load an ACL from a file handle
int vdev_acl_load_file( FILE* file, struct vdev_acl* acl ) {
   
   int rc = 0;
   
   vdev_acl_init( acl );
   
   rc = ini_parse_file( file, vdev_acl_ini_parser, acl );
   if( rc != 0 ) {
      vdev_error("ini_parse_file rc = %d\n", rc );
   }
   
   return rc;
}


// load an ACL from a file, given the path
int vdev_acl_load( char const* path, struct vdev_acl* acl ) {
   
   int rc = 0;
   FILE* f = NULL;
   
   f = fopen( path, "r" );
   if( f == NULL ) {
      
      rc = -errno;
      vdev_error("fopen(%s) errno = %d\n", rc );
      return rc;
   }
   
   rc = vdev_acl_load_file( f, acl );
   
   fclose( f );
   
   if( rc != 0 ) {
      vdev_error("vdev_acl_load_file(%s) rc = %d\n", path, rc );
   }
   
   return rc;
}

// free a list of dirents 
static void vdev_acl_dirents_free( struct dirent** dirents, int num_entries ) {
   
   for( int i = 0; i < num_entries; i++ ) {
      
      if( dirents[i] == NULL ) {
         continue;
      }
      
      free( dirents[i] );
      dirents[i] = NULL;
   }
   
   free( dirents );
}

// free a vector of acls
static void vdev_acl_free_vector( vector<struct vdev_acl*>* acls ) {
   
   for( unsigned int i = 0; i < acls->size(); i++ ) {
      
      if( acls->at(i) != NULL ) {
         
         vdev_acl_free( acls->at(i) );
      }
   }
   
   acls->clear();
}

// free a C-style list of acls (including the list itself)
void vdev_acl_free_all( struct vdev_acl* acl_list, size_t num_acls ) {
   
   for( unsigned int i = 0; i < num_acls; i++ ) {
      
      vdev_acl_free( &acl_list[i] );
   }
   
   free( acl_list );
}


// load all ACLs from a directory, in lexographic order 
// return 0 on success, negative on error
int vdev_acl_load_all( char const* dir_path, vdev_acl_list_t* ret_acls ) {
   
   struct dirent** dirents = NULL;
   int num_entries = 0;
   int rc = 0;
   
   num_entries = scandir( dir_path, &dirents, NULL, alphasort );
   if( num_entries < 0 ) {
      
      vdev_error("scandir(%s) rc = %d\n", dir_path, num_entries );
      return num_entries;
   }
   
   vector<struct vdev_acl*> acls;
   
   for( int i = 0; i < num_entries; i++ ) {
      
      // full path...
      char* fp = fskit_fullpath( dir_path, dirents[i]->d_name, NULL );
      if( fp == NULL ) {
         
         vdev_acl_dirents_free( dirents, num_entries );
         return -ENOMEM;
      }
      
      // load this acl 
      struct vdev_acl* acl = VDEV_CALLOC( struct vdev_acl, 1 );
      if( acl == NULL ) {
         
         free( fp );
         vdev_acl_dirents_free( dirents, num_entries );
         vdev_acl_free_vector( &acls );
         return -ENOMEM;
      }
      
      rc = vdev_acl_load( fp, acl );
      if( rc != 0 ) {
         
         vdev_error("vdev_acl_load(%s) rc = %d\n", fp, rc );
         
         free( fp );
         vdev_acl_dirents_free( dirents, num_entries );
         vdev_acl_free_vector( &acls );
         return rc;
      }
      
      // save this acl 
      try {
         acls.push_back( acl );
      }
      catch( bad_alloc& ba ) {
         
         free( fp );
         vdev_acl_dirents_free( dirents, num_entries );
         vdev_acl_free_vector( &acls );
         return -ENOMEM;
      }
      
      free( fp );
   }
   
   vdev_acl_dirents_free( dirents, num_entries );
   
   // extract values
   ret_acls->swap( acls );
   
   return 0;
}


// does a path match a regex?
// return 1 if so, 0 if not, negative on error 
int vdev_acl_regex_match( char const* path, regex_t* regex ) {
   
   regmatch_t matches[1];
   int rc = 0;
   
   rc = regexec( regex, path, 1, matches, REG_EXTENDED | REG_NEWLINE );
   
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
int vdev_acl_regex_match_any( char const* path, regex_t* regexes, size_t num_regexes ) {
   
   int matched = 0;
   
   for( unsigned int i = 0; i < num_regexes; i++ ) {
      
      matched = vdev_acl_regex_match( path, &regexes[i] );
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

// given a list of access control lists, find the index of the first one that applies to the given path 
// return >= 0 with the index
// return num_acls if not found
// return negative on error
int vdev_acl_find_in_list( char const* path, struct vdev_acl** acls, size_t num_acls ) {
   
   int matched = 0;
   bool found = false;
   int idx = 0;
   
   for( unsigned int i = 0; i < num_acls; i++ ) {
      
      matched = vdev_acl_regex_match_any( path, acls[i]->regexes, acls[i]->num_paths );
      
      if( matched >= (signed)acls[i]->num_paths ) {
         // no match
         continue;
      }
      
      if( matched < 0 ) {
         vdev_error("vdev_acl_regex_match_any(%s) rc = %d\n", path );
         return matched;
      }
      
      found = true;
      idx = i;
      break;
   }
   
   if( found ) {
      return idx;
   }
   else {
      if( matched >= 0 ) {
         return num_acls;
      }
      else {
         return matched;
      }
   }
}

// modify a stat buffer to apply a user access control list
int vdev_acl_set_user( char const* path, struct vdev_acl* acl, uid_t caller_uid, struct stat* sb ) {
   
   if( acl->has_setuid && acl->has_uid ) {
      
      // change the UID of this path
      if( acl->uid == caller_uid ) {
         sb->st_uid = acl->setuid;
      }
   }
   
   return 0;
}

// modify a stat buffer to apply a group access control list 
int vdev_acl_set_group( char const* path, struct vdev_acl* acl, gid_t caller_gid, struct stat* sb ) {
   
   if( acl->has_setgid && acl->has_gid ) {
      
      // change the GID of this path
      if( acl->gid == caller_gid ) {
         sb->st_gid = acl->setgid;
      }
   }
   
   return 0;
}

// modify a stat buffer to apply the mode
int vdev_acl_set_mode( char const* path, struct vdev_acl* acl, struct stat* sb ) {
   
   if( acl->has_setmode ) {
      
      // clear permission bits 
      sb->st_mode &= ~(0777);
      
      // set permission bits 
      sb->st_mode |= acl->setmode;
   }
   
   return 0;
}


// parse the list of PIDs
// return 0 on success, and allocate pids and set the number of pids in num_pids
// return negative on error
int vdev_parse_pids( char const* pid_list_str, size_t pid_list_len, pid_t** pids, int* num_pids ) {
   
   char* tok_tmp = NULL;
   char* strtoll_tmp = NULL;
   char* pid_list_str_dup = NULL;
   char* pid_list_str_in = NULL;
   char* tok = NULL;
   vector<pid_t> parsed_pids;
   int rc = 0;
   
   // make a copy of the pid list, so we can destroy it
   pid_list_str_dup = VDEV_CALLOC( char, pid_list_len );
   if( pid_list_str_dup == NULL ) {
      return -ENOMEM;
   }
   
   memcpy( pid_list_str_dup, pid_list_str, pid_list_len );
   pid_list_str_in = pid_list_str_dup;
   
   while( true ) {
      
      tok = strtok_r( pid_list_str_in, " \n\r\t", &tok_tmp );
      pid_list_str_in = NULL;
      
      if( tok == NULL ) {
         // out of tokens
         break;
      }
      
      // is this a pid?
      pid_t next_pid = (pid_t)strtoll( tok, &strtoll_tmp, 10 );
      if( strtoll_tmp == NULL ) {
         
         // not valid 
         vdev_error("Invalid PID '%s'\n", tok );
         rc = -EINVAL;
         break;
      }
      
      parsed_pids.push_back( next_pid );
   }
   
   free( pid_list_str_dup );
   
   if( rc == 0 ) {
      
      *pids = VDEV_CALLOC( pid_t, parsed_pids.size() );
      if( *pids == NULL ) {
         return -ENOMEM;
      }
      
      // NOTE: vectors are guaranteed to be laid out contiguously in memory...
      memcpy( *pids, &parsed_pids[0], sizeof(pid_t) * parsed_pids.size() );
   }
   
   return 0;
}


// check that the caller PID is matched by the given ACL.
// every process match criterion must be satisfied.
// return 1 if all ACL criteria match
// return 0 if at least one ACL criterion does not match 
// return negative on error
int vdev_acl_match_process( struct vdev_acl* acl, pid_t caller_pid ) {
   
   int rc = 0;
   int flags = 0;
   
   struct pstat ps;
   
   if( acl->proc_path != NULL || acl->proc_sha256 != NULL || acl->has_proc_inode ) {
         
      // filter on the PID's SHA256?
      if( acl->proc_sha256 != NULL ) {
         flags |= PSTAT_HASH;
      }
      
      rc = pstat( caller_pid, &ps, flags );
      
      if( rc != 0 ) {
         vdev_error("pstat(%d) rc = %d\n", caller_pid, rc );
         return rc;
      }
      
      if( acl->proc_path != NULL ) {
         if( strcmp( acl->proc_path, ps.path ) != 0 ) {
            
            // doesn't match 
            return 0;
         }
      }
      
      if( acl->proc_sha256 != NULL ) {
         if( memcmp( acl->proc_sha256, ps.sha256, SHA256_DIGEST_LENGTH ) != 0 ) {
            
            // doesn't match 
            return 0;
         }
      }
      
      if( acl->has_proc_inode ) {
         if( acl->proc_inode != ps.bin_stat.st_ino ) {
            
            // doesn't match 
            return 0;
         }
      }
   }
   
   // filter on a given PID list generator?
   if( acl->proc_pidlist_cmd != NULL ) {
      
      // get the list of PIDs from the shell command 
      char* pid_list_str = NULL;
      size_t pid_list_maxlen = VDEV_ACL_PROC_BUFLEN;
      pid_t* pids = NULL;
      int num_pids = 0;
      bool found = false;
      
      rc = vdev_subprocess( acl->proc_pidlist_cmd, &pid_list_str, pid_list_maxlen );
      
      if( rc != 0 ) {
         
         vdev_error("vdev_subprocess(%s) rc = %d\n", acl->proc_pidlist_cmd, rc );
         return rc;
      }
      
      // lex the pids
      rc = vdev_parse_pids( pid_list_str, pid_list_maxlen, &pids, &num_pids );
      
      free( pid_list_str );
      
      if( rc != 0 ) {
         
         vdev_error("vdev_parse_pids rc = %d\n", rc );
         return rc;
      }
      
      // is the caller in the list?
      for( int i = 0; i < num_pids; i++ ) {
         
         if( caller_pid == pids[i] ) {
            found = true;
            break;
         }
      }
      
      free( pids );
      
      if( !found ) {
         
         return 0;
      }
   }
   
   // all checks match
   return 1;
}


// see if the given caller matches the ACL
// return 1 if matched
// return 0 if not matched
// return negative on error 
int vdev_acl_match( char const* path, struct vdev_acl* acl, uid_t caller_t, gid_t caller_gid, pid_t caller_pid ) {
   
   // TODO 
   
   return 0;
}

// modify a stat buffer to apply an access control list's effect.
// path should match one of the given acl's regexes
// return 0 on success
// return 1 if the file should be filtered outright 
// return negative on error 
int vdev_acl_apply( char const* path, struct vdev_acl* acl, uid_t caller_uid, gid_t caller_gid, pid_t caller_pid, struct stat* sb ) {
   
   int rc = 0;
   int matched = 0;
   
   // TODO
   
   
   return rc;
}
