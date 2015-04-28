/*
 * common - common files for the stat_* binaries
 * 
 * Contains code forked from systemd/src/udev/ on January 7, 2015.
 * (original from https://github.com/systemd/systemd/blob/master/src/udev/)
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/* 
 * Copyright (C) 2005-2008 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2009 Lennart Poettering <lennart@poettering.net>
 * Copyright (C) 2009-2010 David Zeuthen <zeuthen@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Parts of this file are based on the GLIB utf8 validation functions. The
 * original license text follows. */

/* gutf8.c - Operations on UTF-8 strings.
 *
 * Copyright (C) 1999 Tom Tromey
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "common.h"

int vdev_util_replace_whitespace(const char *str, char *to, size_t len) {
   size_t i, j;

   /* strip trailing whitespace */
   len = strnlen(str, len);
   while (len && isspace(str[len-1])) {
      len--;
   }
   
   /* strip leading whitespace */
   i = 0;
   while (isspace(str[i]) && (i < len)) {
      i++;
   }

   j = 0;
   while (i < len) {
      /* substitute multiple whitespace with a single '_' */
      if (isspace(str[i])) {
         while (isspace(str[i])) {
            i++;
         }
         to[j++] = '_';
      }
      to[j++] = str[i++];
   }
   to[j] = '\0';
   return 0;
}


int vdev_whitelisted_char_for_devnode(char c, const char *white) {
   if ((c >= '0' && c <= '9') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= 'a' && c <= 'z') ||
      strchr("#+-.:=@_", c) != NULL ||
      (white != NULL && strchr(white, c) != NULL)) {
            return 1;
   }
   return 0;
}


static inline bool vdev_is_unicode_valid(uint32_t ch) {

   if (ch >= 0x110000) {
      /* End of unicode space */
      return false;
   }
   
   if ((ch & 0xFFFFF800) == 0xD800) {
      /* Reserved area for UTF-16 */
      return false;
   }
   if ((ch >= 0xFDD0) && (ch <= 0xFDEF)) {
      /* Reserved */
      return false;
   }
   
   if ((ch & 0xFFFE) == 0xFFFE) {
      /* BOM (Byte Order Mark) */
      return false;
   }
   return true;
}

/* count of characters used to encode one unicode char */
static int vdev_utf8_encoded_expected_len(const char *str) {
   unsigned char c;
   
   c = (unsigned char) str[0];
   if (c < 0x80) {
      return 1;
   }
   
   if ((c & 0xe0) == 0xc0) {
      return 2;
   }
   if ((c & 0xf0) == 0xe0) {
      return 3;
   }
   if ((c & 0xf8) == 0xf0) {
      return 4;
   }
   if ((c & 0xfc) == 0xf8) {
      return 5;
   }
   if ((c & 0xfe) == 0xfc) {
      return 6;
   }

   return 0;
}

/* decode one unicode char */
int vdev_utf8_encoded_to_unichar(const char *str) {
   int unichar, len, i;

   len = vdev_utf8_encoded_expected_len(str);

   switch (len) {
   case 1:
      return (int)str[0];
   case 2:
      unichar = str[0] & 0x1f;
      break;
   case 3:
      unichar = (int)str[0] & 0x0f;
      break;
   case 4:
      unichar = (int)str[0] & 0x07;
      break;
   case 5:
      unichar = (int)str[0] & 0x03;
      break;
   case 6:
      unichar = (int)str[0] & 0x01;
      break;
   default:
      return -EINVAL;
   }

   for (i = 1; i < len; i++) {
      if (((int)str[i] & 0xc0) != 0x80) {
            return -EINVAL;
      }
      unichar <<= 6;
      unichar |= (int)str[i] & 0x3f;
   }

   return unichar;
}

/* expected size used to encode one unicode char */
static int vdev_utf8_unichar_to_encoded_len(int unichar) {

   if (unichar < 0x80) {
      return 1;
   }
   if (unichar < 0x800) {
      return 2;
   }
   if (unichar < 0x10000) {
      return 3;
   }
   if (unichar < 0x200000) {
      return 4;
   }
   if (unichar < 0x4000000) {
      return 5;
   }

   return 6;
}

/* validate one encoded unicode char and return its length */
int vdev_utf8_encoded_valid_unichar(const char *str) {
   int len, unichar, i;

   len = vdev_utf8_encoded_expected_len(str);
   if (len == 0) {
      return -EINVAL;
   }

   /* ascii is valid */
   if (len == 1) {
      return 1;
   }

   /* check if expected encoded chars are available */
   for (i = 0; i < len; i++) {
      if ((str[i] & 0x80) != 0x80) {
         return -EINVAL;
      }
   }

   unichar = vdev_utf8_encoded_to_unichar(str);

   /* check if encoded length matches encoded value */
   if (vdev_utf8_unichar_to_encoded_len(unichar) != len) {
      return -EINVAL;
   }

   /* check if value has valid range */
   if (!vdev_is_unicode_valid(unichar)) {
      return -EINVAL;
   }

   return len;
}

/* allow chars in whitelist, plain ascii, hex-escaping and valid utf8 */
int vdev_util_replace_chars(char *str, const char *white) {
   size_t i = 0;
   int replaced = 0;

   while (str[i] != '\0') {
      int len;

      if (vdev_whitelisted_char_for_devnode(str[i], white)) {
         i++;
         continue;
      }

      /* accept hex encoding */
      if (str[i] == '\\' && str[i+1] == 'x') {
         i += 2;
         continue;
      }

      /* accept valid utf8 */
      len = vdev_utf8_encoded_valid_unichar(&str[i]);
      if (len > 1) {
         i += len;
         continue;
      }

      /* if space is allowed, replace whitespace with ordinary space */
      if (isspace(str[i]) && white != NULL && strchr(white, ' ') != NULL) {
         str[i] = ' ';
         i++;
         replaced++;
         continue;
      }

      /* everything else is replaced with '_' */
      str[i] = '_';
      i++;
      replaced++;
   }
   return replaced;
}


/**
 * vdev_util_encode_string:
 * @str: input string to be encoded
 * @str_enc: output string to store the encoded input string
 * @len: maximum size of the output string, which may be
 *       four times as long as the input string
 *
 * Encode all potentially unsafe characters of a string to the
 * corresponding 2 char hex value prefixed by '\x'.
 *
 * Returns: 0 if the entire string was copied, non-zero otherwise.
 **/
int vdev_util_encode_string(const char *str, char *str_enc, size_t len) {
        
   size_t i, j;

   if (str == NULL || str_enc == NULL) {
      return -EINVAL;
   }

   for (i = 0, j = 0; str[i] != '\0'; i++) {
      int seqlen;

      seqlen = vdev_utf8_encoded_valid_unichar(&str[i]);
      if (seqlen > 1) {
         
         if (len-j < (size_t)seqlen) {
            goto err;
         }
         memcpy(&str_enc[j], &str[i], seqlen);
         j += seqlen;
         i += (seqlen-1);
      }
      else if (str[i] == '\\' || !vdev_whitelisted_char_for_devnode(str[i], NULL)) {
         
         if (len-j < 4) {
            goto err; 
         }
         sprintf(&str_enc[j], "\\x%02x", (unsigned char) str[i]);
         j += 4;
      }
      else {
         
         if (len-j < 1) {
            goto err;
         }
         
         str_enc[j] = str[i];
         j++;
      }
   }
   if (len-j < 1) {
      goto err;
   }
   
   str_enc[j] = '\0';
   return 0;
err:
   return -EINVAL;
}

// remove trailing whitespace
int vdev_util_rstrip( char* str ) {
   
   size_t i = strlen(str);
   
   if( i == 0 ) {
      return 0;
   }
   
   i--;
   
   while( i >= 0 ) {
      
      if( isspace(str[i]) ) {
         str[i] = 0;
         i--;
         continue;
      }
      else {
         // not space 
         break;
      }
   }
   
   return 0;
}

// global list of properties discovered
static struct vdev_property* vdev_property_head = NULL;
static struct vdev_property* vdev_property_tail = NULL;

// append to a linked list of properties discovered
// return 0 on success 
// return -ENOMEM if we run out of memory
int vdev_property_add( char const* name, char const* value ) {
   
   struct vdev_property* prop = (struct vdev_property*)calloc( sizeof(struct vdev_property), 1 );
   
   if( prop == NULL ) {
      return -ENOMEM;
   }
   
   prop->name = strdup( name );
   if( prop->name == NULL ) {
      
      free( prop );
      return -ENOMEM;
   }
   
   prop->value = strdup( value );
   if( prop->value == NULL ) {
      
      free( prop->name );
      free( prop );
      return -ENOMEM;
   }
   
   prop->next = NULL;
   
   if( vdev_property_head == NULL ) {
      vdev_property_head = prop;
      vdev_property_tail = prop;
   }
   
   else {
      vdev_property_tail->next = prop;
      vdev_property_tail = prop;
   }
   
   return 0;
}

// print all properties as sourceable environmet variables
// always succeeds
int vdev_property_print( void ) {
   
   if( vdev_property_head != NULL ) {
      
      for( struct vdev_property* itr = vdev_property_head; itr != NULL; itr = itr->next ) {
         printf("%s=%s\n", itr->name, itr->value );
      }
   }
   
   return 0;
}


// free all properties 
// always succeeds
int vdev_property_free_all( void ) {
   
   if( vdev_property_head != NULL ) {
      
      for( struct vdev_property* itr = vdev_property_head; itr != NULL; ) {
         
         struct vdev_property* itr_next = itr->next;
         
         free( itr->name );
         free( itr->value );
         free( itr );
         
         itr = itr_next;
      }
   }
   
   vdev_property_head = NULL;
   vdev_property_tail = NULL;
   
   return 0;
}


// read a file, masking EINTR
// return the number of bytes read on success
// return -errno on failure
ssize_t vdev_read_uninterrupted( int fd, char* buf, size_t len ) {
   
   ssize_t num_read = 0;
   
   if( buf == NULL ) {
      return -EINVAL;
   }
   
   while( (unsigned)num_read < len ) {
      ssize_t nr = read( fd, buf + num_read, len - num_read );
      if( nr < 0 ) {
         
         int errsv = -errno;
         if( errsv == -EINTR ) {
            continue;
         }
         
         return errsv;
      }
      if( nr == 0 ) {
         break;
      }
      
      num_read += nr;
   }
   
   return num_read;
}



// read a sysfs attribute 
int vdev_sysfs_read_attr( char const* sysfs_device_path, char const* attr_name, char** value, size_t* value_len ) {
   
   int rc = 0;
   int fd = 0;
   struct stat sb;
   char attr_path[4097];
   
   sprintf(attr_path, "%s/%s", sysfs_device_path, attr_name );
   
   rc = stat( attr_path, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      return rc;
   }
   
   // regular file? return the contents 
   if( S_ISREG( sb.st_mode ) ) {
      
      fd = open( attr_path, O_RDONLY );
      if( fd < 0 ) {
         
         rc = -errno;
         
         fprintf(stderr, "WARN: open('%s') errno = %d\n", attr_path, rc );
         return rc;
      }
      
      char* ret_value = (char*)calloc( sb.st_size + 1, 1 );
      if( ret_value == NULL ) {
         
         close( fd );
         return -ENOMEM;
      }
      
      rc = vdev_read_uninterrupted( fd, ret_value, sb.st_size );
      if( rc < 0 ) {
         
         free( ret_value );
         close( fd );
         return rc;
      }
      
      close( fd );
      
      *value = ret_value;
      *value_len = sb.st_size;
      
      return 0;
   }
   
   // symlink? return the actual link 
   else if( S_ISLNK( sb.st_mode ) ) {
      
      char* ret_value = (char*)calloc( sb.st_size + 1, 1 );
      if( ret_value == NULL ) {
         
         return -ENOMEM;
      }
      
      rc = readlink( attr_path, ret_value, sb.st_size );
      if( rc < 0 ) {
         
         rc = -errno;
         fprintf(stderr, "readlink('%s') errno = %d\n", attr_path, rc );
         
         free( ret_value );
         return rc;
      }
      
      *value = ret_value;
      *value_len = sb.st_size + 1;
      
      return 0;
   }
   
   // not sure what to do here 
   else {
      
      return -EINVAL;
   }
}


// find a field value from a uevent 
// return 0 on success
// return -ENOENT if the key is not found 
// return -ENOMEM if oom.
int vdev_sysfs_uevent_get_key( char const* uevent_buf, size_t uevent_buflen, char const* key, char** value, size_t* value_len ) {
   
   char* tmp = NULL;
   char* tok = NULL;
   char* buf_ptr = NULL;
   char* delim = NULL;
   int rc = 0;
   
   // NOTE: zero-terminate
   char* buf_dup = (char*)calloc( uevent_buflen + 1, 1 );
   if( buf_dup == NULL ) {
      return -ENOMEM;
   }
   
   memcpy( buf_dup, uevent_buf, uevent_buflen );
   buf_ptr = buf_dup;
   
   while( 1 ) {
      
      // next line 
      tok = strtok_r( buf_ptr, "\n", &tmp );
      buf_ptr = NULL;
      
      if( tok == NULL ) {
         rc = -ENOENT;
         break;
      }
      
      // read key 
      delim = index( tok, '=' );
      if( delim == NULL ) {
         continue;
      }
      
      *delim = '\0';
      
      // match key?
      if( strcmp( tok, key ) == 0 ) {
         
         // get value
         *value_len = strlen( delim + 1 );
         *value = (char*)calloc( *value_len + 1, 1 );
         
         if( *value == NULL ) {
            rc = -ENOMEM;
            break;
         }
         
         strcpy( *value, delim + 1 );
         break;
      }
      
      *delim = '=';
   }
   
   free( buf_dup );
   return rc;
}

// read a whole file into RAM
// return 0 on success, and set *file_buf and *file_buf_len 
// return negative on error
int vdev_read_file( char const* path, char** file_buf, size_t* file_buf_len ) {
   
   int fd = 0;
   struct stat sb;
   char* buf = NULL;
   int rc = 0;
   
   // get size 
   rc = stat( path, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      return rc;
   }
   
   // read the uevent file itself 
   buf = (char*)calloc( sb.st_size, 1 );
   if( buf == NULL ) {
      
      return -ENOMEM;
   }
   
   fd = open( path, O_RDONLY );
   if( fd < 0 ) {
      
      rc = -errno;
      free( buf );
      return rc;
   }
   
   rc = vdev_read_uninterrupted( fd, buf, sb.st_size );
   if( rc < 0 ) {
      
      close( fd );
      free( buf );
      return rc;
   }
   
   close( fd );
   
   *file_buf = buf;
   *file_buf_len = rc;
   
   return 0;
}


// find a field in the uevent buffer, using vdev_read_file and vdev_sysfs_uevent_get_key
// return 0 on success, and set *value and *value_len 
// return negative on error
int vdev_sysfs_uevent_read_key( char const* sysfs_device_path, char const* uevent_key, char** uevent_value, size_t* uevent_value_len ) {
   
   int rc = 0;
   
   char* uevent_buf = NULL;
   size_t uevent_len = 0;
   
   char* uevent_path = (char*)calloc( strlen(sysfs_device_path) + strlen("/uevent") + 1, 1 );
   if( uevent_path == NULL ) {
      
      return -ENOMEM;
   }
   
   sprintf( uevent_path, "%s/uevent", sysfs_device_path );
   
   // get uevent 
   rc = vdev_read_file( uevent_path, &uevent_buf, &uevent_len );
   if( rc < 0 ) {
      
      if( DEBUG ) {
         fprintf(stderr, "WARN: vdev_read_file('%s') rc = %d\n", uevent_path, rc );
      }
      return rc;
   }
   
   // get devtype 
   rc = vdev_sysfs_uevent_get_key( uevent_buf, uevent_len, uevent_key, uevent_value, uevent_value_len );
   
   free( uevent_buf );
   free( uevent_path );
   
   return rc;
}


// walk up a sysfs device path to find the ancestor device with the given subsystem and devtype
// if devtype_name is NULL, match any devtype (only match the subsystem)
// return 0 on success
// return -ENOMEM on OOM 
// return -ENOENT if a subsystem or uevent was not found, when one was expected
int vdev_sysfs_get_parent_with_subsystem_devtype( char const* sysfs_device_path, char const* subsystem_name, char const* devtype_name, char** devpath, size_t* devpath_len ) {
   
   char cur_dir[4097];
   char subsystem_path[4097];
   char subsystem_link[4097];
   char uevent_path[4097];
   
   memset( subsystem_path, 0, 4097 );
   memset( subsystem_link, 0, 4097 );
   
   char* tmp = NULL;
   int rc = 0;
   char* uevent_buf = NULL;
   size_t uevent_len = 0;
   char* devtype = NULL;
   size_t devtype_len = 0;
   char* parent_device = NULL;
   size_t parent_device_len = 0;
   
   strcpy( cur_dir, sysfs_device_path );
   
   while( 1 ) {
      
      // get parent device 
      rc = vdev_sysfs_get_parent_device( cur_dir, &parent_device, &parent_device_len );
      if( rc != 0 ) {
         break;
      }
      
      // subsystem?
      sprintf( subsystem_path, "%s/subsystem", parent_device );
      
      memset( subsystem_link, 0, 4096 );
      
      rc = readlink( subsystem_path, subsystem_link, 4096 );
      if( rc < 0 ) {
         
         rc = -errno;
         if( rc != -ENOENT ) {
            fprintf(stderr, "WARN: readlink('%s') errno = %d\n", subsystem_path, rc );
         }
         
         free( parent_device );
         parent_device = NULL;
         return rc;
      }
      
      // get subsystem name...
      tmp = rindex( subsystem_link, '/' );
      
      if( tmp != NULL && strcmp( tmp + 1, subsystem_name ) != 0 ) {
         
         // subsystem does not match
         // crawl up to the parent
         strcpy( cur_dir, parent_device );
         
         free( parent_device );
         parent_device = NULL;
         continue;
      }
      
      // subsystem matches...
      *tmp = 0;
      
      if( devtype_name != NULL ) {
         
         // get uevent
         // get DEVTYPE from uevent 
         // make uevent path 
         sprintf( uevent_path, "%s/uevent", parent_device );
         
         // get uevent 
         rc = vdev_read_file( uevent_path, &uevent_buf, &uevent_len );
         if( rc < 0 ) {
            
            fprintf(stderr, "WARN: vdev_read_file('%s') rc = %d\n", uevent_path, rc );
            
            free( parent_device );
            parent_device = NULL;
            return rc;
         }
         
         // get devtype 
         rc = vdev_sysfs_uevent_get_key( uevent_buf, uevent_len, "DEVTYPE", &devtype, &devtype_len );
         free( uevent_buf );
         
         if( rc != 0 ) {
            
            if( rc == -ENOENT ) {
               
               // not found 
               // next parent 
               strcpy( cur_dir, parent_device );
               
               free( parent_device );
               parent_device = NULL;
               
               continue;
            }
            else {
               
               free( parent_device );
               parent_device = NULL;
               
               return rc;
            }
         }
         
         // matches?
         if( strcmp( devtype, devtype_name ) == 0 ) {
            
            free( devtype );
            
            // this is the path to the device 
            *devpath = parent_device;
            *devpath_len = strlen( parent_device );
            
            // found!
            rc = 0;
            break;
         }
         else {
            // no match.
            // next device 
            free( devtype );
            
            strcpy( cur_dir, parent_device );
            
            free( parent_device );
            parent_device = NULL;
            continue;
         }
      }
      else {
         
         // match only on subsystem 
         *devpath = parent_device;
         *devpath_len = strlen(parent_device);
         
         rc = 0;
         break;
      }
   }
   
   return rc;
}


// make a device path using the symlink in the contained 'device' entry 
int vdev_sysfs_read_device_path( char const* sysfs_dir, char** devpath, size_t* devpath_len ) {
   
   int rc = 0;
   struct stat sb;
   
   char path_buf[8193];
   
   sprintf( path_buf, "%s/device", sysfs_dir );
   
   // is there a link here?
   rc = stat( path_buf, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      return rc;
   }
   
   // must be a link 
   if( !S_ISLNK( sb.st_mode ) ) {
      
      rc = -EINVAL;
      return rc;
   }
   
   // follow that link!
   *devpath = realpath( path_buf, NULL );
   if( *devpath == NULL ) {
      return -ENOMEM;
   }
   
   *devpath_len = strlen(*devpath);
   
   return 0;
}


// search sysfs for the device path corresponding to a given subsystem and sysname.
// fill in devpath if found.
// heavily inspired by udev_device_new_from_subsystem_sysname
int vdev_sysfs_device_path_from_subsystem_sysname( char const* sysfs_mount, char const* subsystem, char const* sysname, char** devpath, size_t* devpath_len ) {
   
   int rc = 0;
   struct stat sb;
   char path_buf[8193];
   
   memset( path_buf, 0, 8193 );
   
   if( strcmp( subsystem, "subsystem" ) == 0 ) {
      
      sprintf( path_buf, "%s/subsystem/%s/", sysfs_mount, sysname );
      rc = stat( path_buf, &sb );
      
      if( rc == 0 ) {
         return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
      }
      
      sprintf(path_buf, "%s/bus/%s/", sysfs_mount, sysname );
      rc = stat( path_buf, &sb );
      
      if( rc == 0 ) {
         return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
      }
      
      sprintf(path_buf, "%s/class/%s/", sysfs_mount, sysname );
      rc = stat( path_buf, &sb );
      
      if( rc == 0 ) {
         return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
      }
   }
   
   if( strcmp( subsystem, "module" ) == 0 ) {
      
      sprintf( path_buf, "%s/module/%s/", sysfs_mount, sysname );
      rc = stat( path_buf, &sb );
      
      if( rc == 0 ) {
         return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
      }
   }
   
   if( strcmp( subsystem, "drivers" ) == 0 ) {
      
      char driver_prefix[4096];
      char* driver = NULL;
      
      sprintf( driver_prefix, sysname );
      driver = strchr( driver_prefix, ':' );
      
      if( driver != NULL ) {
         
         // trim header
         *driver = '\0';
         driver++;
         
         sprintf( path_buf, "%s/subsystem/%s/drivers/%s/", sysfs_mount, driver_prefix, driver );
         rc = stat( path_buf, &sb );
         
         if( rc == 0 ) {
            return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
         }
         
         sprintf( path_buf, "%s/bus/%s/drivers/%s/", sysfs_mount, driver_prefix, driver );
         rc = stat( path_buf, &sb );
         
         if( rc == 0 ) {
            return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
         }
      }
      else {
         
         return -EINVAL;
      }
   }
   
   sprintf( path_buf, "%s/subsystem/%s/devices/%s/", sysfs_mount, subsystem, sysname );
   rc = stat( path_buf, &sb );
   
   if( rc == 0 ) {
      return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
   }
   
   sprintf( path_buf, "%s/bus/%s/devices/%s/", sysfs_mount, subsystem, sysname );
   rc = stat( path_buf, &sb );
   
   if( rc == 0 ) {
      return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
   }
   
   sprintf( path_buf, "%s/class/%s/%s", sysfs_mount, subsystem, sysname );
   rc = stat( path_buf, &sb );
   
   if( rc == 0 ) {
      
      return vdev_sysfs_read_device_path( path_buf, devpath, devpath_len );
   }
   
   return -ENOENT;
}


// get the parent device of a given device, using sysfs.
// not all devices have parents; those that do will have a uevent file.
// walk up the path until we reach /
// return 0 on success
// return -EINVAL if the dev path has no '/'
// return -ENOENT if this dev has no parent
// return -errno if stat'ing the parent path fails 
// return -ENOMEM if OOM
int vdev_sysfs_get_parent_device( char const* dev_path, char** ret_parent_device, size_t* ret_parent_device_len ) {
   
   char* parent_path = NULL;
   size_t parent_path_len = 0;
   struct stat sb;
   int rc = 0;
   
   char* delim = NULL;
   
   parent_path = (char*)calloc( strlen(dev_path) + 1 + strlen("/uevent"), 1 );
   if( parent_path == NULL ) {
      return -ENOMEM;
   }
   
   strcpy( parent_path, dev_path );
   
   while( strlen(parent_path) > 0 ) {
      
      // lop off the child 
      delim = rindex( parent_path, '/' );
      if( delim == NULL ) {
         
         // invalid 
         free( parent_path );
         return -EINVAL;
      }
      
      if( delim == parent_path ) {
         
         // reached /
         free( parent_path );
         return -ENOENT;
      }
      
      while( *delim == '/' && delim != parent_path ) {
         *delim = '\0';
         delim--;
      }
      
      parent_path_len = strlen( parent_path );
      
      // verify that this is a device...
      strcat( parent_path, "/uevent" );
      
      rc = stat( parent_path, &sb );
      if( rc != 0 ) {
         
         // not a device
         // remove /uevent 
         parent_path[ parent_path_len ] = '\0';
         continue;
      }
      else {
         
         // device!
         break;
      }
   }
   
   // lop off /uevent 
   parent_path[ parent_path_len ] = '\0';
   
   *ret_parent_device = parent_path;
   *ret_parent_device_len = parent_path_len;
   
   return 0;
}

// get the subsystem from a device path
int vdev_sysfs_read_subsystem( char const* devpath, char** ret_subsystem, size_t* ret_subsystem_len ) {
   
   int rc = 0;
   char linkpath[ 4097 ];
   size_t linkpath_len = 4097;
   char* subsystem_path = NULL;
   char* subsystem = NULL;
   
   subsystem_path = (char*)calloc( strlen(devpath) + 1 + strlen("/subsystem") + 1, 1 );
   if( subsystem_path == NULL ) {
      return -ENOMEM;
   }
   
   sprintf( subsystem_path, "%s/subsystem", devpath );
   
   memset( linkpath, 0, 4097 );
   
   rc = readlink( subsystem_path, linkpath, linkpath_len );
   if( rc < 0 ) {
      
      rc = -errno;
      log_error("readlink('%s') rc = %d\n", subsystem_path, rc );
      free( subsystem_path );
      return rc;
   }
   
   free( subsystem_path );
   
   subsystem = rindex( linkpath, '/' );
   if( subsystem == NULL ) {
      return -EINVAL;
   }
   
   *ret_subsystem = strdup( subsystem + 1 );
   *ret_subsystem_len = strlen( subsystem + 1 );
   
   if( *ret_subsystem == NULL ) {
      return -ENOMEM;
   }
   
   return 0;
}


// get the sysname from the device path
int vdev_sysfs_get_sysname( char const* devpath, char** ret_sysname, size_t* ret_sysname_len ) {
   
   char const* delim = NULL;
   char* sysname = NULL;
   size_t len = 0;
   
   delim = rindex( devpath, '/' );
   if( delim == NULL ) {
      
      return -EINVAL;
   }
   
   sysname = strdup( delim + 1 );
   
   if( sysname == NULL ) {
      return -ENOMEM;
   }
   
    /* some devices have '!' in their name, change that to '/' */
    while( sysname[len] != '\0' ) {
       
       if( sysname[len] == '!' ) {
          sysname[len] = '/';
       }
       
       len++;
    }
    
    *ret_sysname = sysname;
    *ret_sysname_len = len;
    
   return 0;
}


// get the sysnum of a device from the device path 
int vdev_sysfs_get_sysnum( char const* devpath, int* sysnum ) {
   
   int rc = 0;
   char* sysname = NULL;
   size_t sysname_len = 0;
   int i = 0;
   
   // last digits at the end of the sysname 
   rc = vdev_sysfs_get_sysname( devpath, &sysname, &sysname_len );
   if( rc != 0 ) {
      return rc;
   }
   
   while( isdigit( sysname[ sysname_len - i ] ) ) {
      i++;
   }
   i++;
   
   *sysnum = strtol( sysname + sysname_len - i, NULL, 10 );
   
   free( sysname );
   
   return 0;
}


// get the absolute sysfs device path from a major/minor pair and device type 
// return 0 on success, and set *syspath and *syspath_len 
// return negative on error 
int vdev_sysfs_get_syspath_from_device( char const* sysfs_mountpoint, mode_t mode, unsigned int major, unsigned int minor, char** syspath, size_t* syspath_len ) {
   
   char* devpath = (char*)calloc( strlen(sysfs_mountpoint) + 4097, 1 );
   char linkbuf[4097];
   
   if( devpath == NULL ) {
      return -ENOMEM;
   }
   
   int rc = 0;
   
   memset( linkbuf, 0, 4097 );
   
   if( S_ISCHR( mode ) ) {
      
      // character device 
      sprintf(devpath, "%s/dev/char/%u:%u", sysfs_mountpoint, major, minor );
   }
   else if( S_ISBLK( mode ) ) {
      
      // block device 
      sprintf(devpath, "%s/dev/block/%u:%u", sysfs_mountpoint, major, minor );
   }
   else {
      
      free( devpath );
      return -EINVAL;
   }
   
   rc = readlink( devpath, linkbuf, 4097 );
   if( rc < 0 ) {
      
      rc = -errno;
      free( devpath );
      return rc;
   }
   
   // link should start with "../devices"
   if( strncmp( linkbuf, "../../devices", strlen("../../devices") ) != 0 ) {
      
      // sysfs structure changed???
      free( devpath );
      return -EIO;
   }
   
   sprintf( devpath, "%s/%s", sysfs_mountpoint, linkbuf + strlen("../../") );
   
   *syspath = devpath;
   *syspath_len = strlen(devpath);
   
   return 0;
}



