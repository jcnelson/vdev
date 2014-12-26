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
 
#include "util.h"

#include "fskit/fskit.h"

int _DEBUG = 0;

int _DEBUG_MESSAGES = 0;
int _ERROR_MESSAGES = 1;


void vdev_set_debug_level( int d ) {
   _DEBUG_MESSAGES = d;
}

void vdev_set_error_level( int e ) {
   _ERROR_MESSAGES = e;
}

int vdev_get_debug_level() {
   return _DEBUG_MESSAGES;
}

int vdev_get_error_level() {
   return _ERROR_MESSAGES;
}

// run a subprocess in the system shell.
// gather the output into the output buffer (allocating it if needed).
// do not allow the output buffer to exceed max_output bytes.
// return 0 on success
// return 1 on truncate 
// return negative on error
// set the subprocess exit status in *exit_status
int vdev_subprocess( char const* cmd, char** output, size_t max_output, int* exit_status ) {
   
   FILE* pipe = NULL;
   int rc = 0;
   
   // read up to max_output
   if( output != NULL ) {
      if( *output == NULL && max_output > 0 ) {
         
         *output = VDEV_CALLOC( char, max_output );
         if( *output == NULL ) {
            return -ENOMEM;
         }
      }
   }
   
   pipe = popen( cmd, "r" );
   if( pipe == NULL ) {
      
      rc = -errno;
      vdev_error("popen(%s) rc = %d\n", cmd, rc );
      
      if( output != NULL && *output != NULL ) {
         free( *output );
         *output = NULL;
      }
      
      return rc;
   }
   
   // read up to max_output 
   size_t num_read = 0;
   bool truncate = false;
   bool eof = false;
   
   while( num_read < max_output ) {
      
      size_t nr = 0;
      size_t len = 4096;
      char static_buf[4096];
      char* buf = NULL;
      
      if( output != NULL && *output != NULL ) {
         buf = *output;
      }
      else {
         // dummy output 
         buf = static_buf;
      }
      
      if( num_read + len >= max_output ) {
         // truncate
         len = max_output - num_read;
      }
      
      nr = fread( buf, 1, len, pipe );
      
      num_read += nr;
      
      if( nr != len ) {
         
         // error or EOF?
         if( feof(pipe) ) {
            eof = true;
            break;
         }
         
         else if( ferror(pipe) ) {
            
            rc = -errno;
            vdev_error("fread(pipe) rc = %d, ferror = %d\n", rc, ferror(pipe) );
            
            break;
         }
      }
   }
   
   if( !eof && num_read >= max_output ) {
      truncate = true;
   }
   
   *exit_status = pclose( pipe );
   
   if( rc != 0 ) {
      
      // clean up 
      if( output != NULL && *output != NULL ) {
         free( *output );
         *output = NULL;
      }
      
      return rc;
   }
   else {
      if( truncate ) {
         return 1;
      }
      else {
         return 0;
      }
   }
}


// write, but mask EINTR
ssize_t vdev_write_uninterrupted( int fd, char const* buf, size_t len ) {
   
   ssize_t num_written = 0;
   while( (unsigned)num_written < len ) {
      ssize_t nw = write( fd, buf + num_written, len - num_written );
      if( nw < 0 ) {
         
         int errsv = -errno;
         if( errsv == -EINTR ) {
            continue;
         }
         
         return errsv;
      }
      if( nw == 0 ) {
         break;
      }
      
      num_written += nw;
   }
   
   return num_written;
}


// read, but mask EINTR
ssize_t vdev_read_uninterrupted( int fd, char* buf, size_t len ) {
   
   ssize_t num_read = 0;
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


// free a list of dirents 
static void vdev_dirents_free( struct dirent** dirents, int num_entries ) {
   
   for( int i = 0; i < num_entries; i++ ) {
      
      if( dirents[i] == NULL ) {
         continue;
      }
      
      free( dirents[i] );
      dirents[i] = NULL;
   }
   
   free( dirents );
}


// process all files in a directory
// return 0 on success, negative on error
int vdev_load_all( char const* dir_path, vdev_dirent_loader_t loader, void* cls ) {
   
   struct dirent** dirents = NULL;
   int num_entries = 0;
   int rc = 0;
   
   num_entries = scandir( dir_path, &dirents, NULL, alphasort );
   if( num_entries < 0 ) {
      
      vdev_error("scandir(%s) rc = %d\n", dir_path, num_entries );
      return num_entries;
   }
   
   for( int i = 0; i < num_entries; i++ ) {
      
      // full path...
      char* fp = fskit_fullpath( dir_path, dirents[i]->d_name, NULL );
      if( fp == NULL ) {
         
         vdev_dirents_free( dirents, num_entries );
         return -ENOMEM;
      }
      
      // process this 
      rc = (*loader)( fp, cls );
      if( rc != 0 ) {
         
         vdev_error("loader(%s) rc = %d\n", fp, rc );
         
         free( fp );
         vdev_dirents_free( dirents, num_entries );
         return rc;
      }
      
      free( fp );
   }
   
   vdev_dirents_free( dirents, num_entries );
   
   return 0;
}


// get a passwd struct for a user.
// on success, fill in pwd and *pwd_buf (the caller must free *pwd_buf, but not pwd).
// return 0 on success
int vdev_get_passwd( char const* username, struct passwd* pwd, char** pwd_buf ) {
   
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


// get a group struct for a group.
// on success, fill in grp and *grp_buf (the caller must free *grp_buf, but not grp).
// return 0 on success
int vdev_get_group( char const* groupname, struct group* grp, char** grp_buf ) {
   
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


