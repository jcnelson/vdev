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

// run a subprocess in the system shell (/bin/sh)
// gather the output into the output buffer (allocating it if needed).
// return 0 on success
// return 1 on output truncate 
// return negative on error
// set the subprocess exit status in *exit_status
int vdev_subprocess( char const* cmd, char* const env[], char** output, size_t max_output, int* exit_status ) {
   
   int p[2];
   int rc = 0;
   pid_t pid = 0;
   int max_fd = 0;
   ssize_t nr = 0;
   size_t num_read = 0;
   int status = 0;
   bool alloced = false;
   
   // open the pipe 
   rc = pipe( p );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("pipe() rc = %d\n", rc );
      return rc;
   }
   
   // fork the child 
   pid = fork();
   
   if( pid == 0 ) {
      
      // send stdout to p[1]
      close( p[0] );
      rc = dup2( p[1], STDOUT_FILENO );
      
      if( rc < 0 ) {
         
         rc = -errno;
         vdev_error("dup2(%d, %d) rc = %d\n", p[1], STDOUT_FILENO, rc );
         
         close( p[1] );
         exit(1);
      }
      
      // close everything else but stdout
      max_fd = sysconf(_SC_OPEN_MAX);
      
      for( int i = 0; i < max_fd; i++ ) {
         
         if( i != STDOUT_FILENO ) {
            close( i );
         }
      }
      
      // run the command 
      if( env != NULL ) {
         rc = execle( "/bin/sh", "sh", "-c", cmd, (char*)0, env );
      }
      else {
         rc = execl( "/bin/sh", "sh", "-c", cmd, (char*)0 );
      }
      
      if( rc != 0 ) {
         
         rc = -errno;
         vdev_error("execl[e]() rc = %d\n", rc );
         exit(1);
      }
      else {
         // make gcc happy 
         exit(0);
      }
   }
   else if( pid > 0 ) {
      
      // parent 
      close(p[1]);
      
      // allocate output 
      if( output != NULL ) {
         if( *output == NULL && max_output > 0 ) {
            
            *output = VDEV_CALLOC( char, max_output );
            if( *output == NULL ) {
               
               // out of memory 
               return -ENOMEM;
            }
            
            alloced = true;
         }
      }
      
      // get output 
      if( output != NULL && *output != NULL && max_output > 0 ) {
         
         while( (unsigned)num_read < max_output ) {
            
            nr = vdev_read_uninterrupted( p[0], (*output) + num_read, max_output - num_read );
            if( nr < 0 ) {
               
               vdev_error("vdev_read_uninterrupted rc = %d\n", rc );
               close( p[0] );
               
               if( alloced ) {
                  
                  free( *output );
                  *output = NULL;
               }
               return rc;
            }
            
            if( nr == 0 ) {
               break;
            }
            
            num_read += nr;
         }
      }
      
      // wait for child
      rc = waitpid( pid, &status, 0 );
      if( rc < 0 ) {
         
         rc = -errno;
         vdev_error("waitpid(%d) rc = %d\n", pid );
         
         close( p[0] );
         
         if( alloced ) {
            
            free( *output );
            *output = NULL;
         }
         
         return rc;
      }
      
      if( WIFEXITED( status ) ) {
         
         *exit_status = WEXITSTATUS( *exit_status );
      }
      else if( WIFSIGNALED( status ) ) {
         
         vdev_error("WARN: command '%s' failed with signal %d\n", cmd, WTERMSIG( status ) );
         *exit_status = status;
      }
      else {
         
         // don't care about start/stop
         *exit_status = 0;
      }
      
      close( p[0] );
      return 0;
   }
   else {
      
      rc = -errno;
      vdev_error("fork() rc = %d\n", rc );
      return rc;
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


// make a string of directories, given the path dirp
// return 0 if the directory exists at the end of the call.
// return negative if the directory could not be created.
int vdev_mkdirs( char const* dirp, int start, mode_t mode ) {
   
   unsigned int i = start;
   char* currdir = NULL;
   struct stat statbuf;
   int rc = 0;
   
   currdir = VDEV_CALLOC( char, strlen(dirp) + 1 );
   if( currdir == NULL ) {
      return -ENOMEM;
   }
   
   while( i <= strlen(dirp) ) {
      
      // next '/'
      if( dirp[i] == '/' || i == strlen(dirp) ) {
         
         strncpy( currdir, dirp, i == 0 ? 1 : i );
         
         rc = stat( currdir, &statbuf );
         if( rc == 0 && !S_ISDIR( statbuf.st_mode ) ) {
            
            // something else exists here
            free( currdir );
            return -ENOTDIR;
         }
         
         if( rc != 0 ) {
            
            // try to make this directory
            rc = mkdir( currdir, mode );
            if( rc != 0 ) {
               
               rc = -errno;
               free(currdir);
               return rc;
            }
         }
      }
      
      i++;
   }
   
   free(currdir);
   return 0;
}

