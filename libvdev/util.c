/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

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
 
#include "util.h"

int _VDEV_DEBUG_MESSAGES = 0;
int _VDEV_INFO_MESSAGES = 0;
int _VDEV_WARN_MESSAGES = 1;
int _VDEV_ERROR_MESSAGES = 1;
int _VDEV_SYSLOG = 0;


// set debug level, by setting for info and debug messages
void vdev_set_debug_level( int d ) {
   
   if( d >= 1 ) {
      _VDEV_INFO_MESSAGES = 1;
   }
   if( d >= 2 ) {
      _VDEV_DEBUG_MESSAGES = d;
   }
}

// set error level, by setting for warning and error messages
void vdev_set_error_level( int e ) {
   
   if( e >= 1 ) {
      _VDEV_ERROR_MESSAGES = 1;
   }
   if( e >= 2 ) {
      _VDEV_WARN_MESSAGES = 1;
   }
}

// debug level is the sum of the levels enabled
int vdev_get_debug_level() {
   return _VDEV_DEBUG_MESSAGES + _VDEV_INFO_MESSAGES;
}

// error level is the sum of the errors enable
int vdev_get_error_level() {
   return _VDEV_ERROR_MESSAGES + _VDEV_WARN_MESSAGES;
}

// turn on syslog logging 
// always succeeds
int vdev_enable_syslog() {
   
   _VDEV_SYSLOG = 1;
   openlog( VDEV_SYSLOG_IDENT, LOG_CONS | LOG_NDELAY | LOG_PID, LOG_DAEMON );
   
   return 0;
}

// become a daemon: fork, setsid, fork, umask
// return 0 on child
// return 1 on parent 
// return -errno on failure to fork 
int vdev_daemonize(void) {
   
   int rc = 0;
   pid_t pid = 0;
   
   pid = fork();
   if( pid == 0 ) {
      
      // child--no tty
      // become process group leader
      rc = setsid();
      if( rc < 0 ) {
         
         rc = -errno;
         vdev_error("setsid() rc = %d\n", rc );
         return rc;
      }
      
      // fork again 
      // stop us from ever regaining the tty
      pid = fork();
      if( pid == 0 ) {
         
         // child
         // switch to /
         rc = chdir("/");
         if( rc != 0 ) {
            
            rc = -errno;
            vdev_error("chdir('/') rc = %d\n", rc );
            return rc;
         }
         
         // own everything we create 
         umask(0);
         
         // now a daemon!
         return 0;
      }
      else if( pid > 0 ) {
         
         // parent--die, but don't call userspace clean-up
         _exit(0);
      }
      else {
         
         // failed to fork 
         rc = -errno;
         vdev_error("fork( parent=%d ) rc = %d\n", getpid(), rc );
         return rc;
      }
   }
   else if( pid > 0 ) {
      
      // parent--we're done 
      return 1;
   }
   else {
      
      // error
      rc = -errno;
      vdev_error( "fork( parent=%d ) rc = %d\n", getpid(), rc );
      return rc;
   }
}


// redirect stdout and stderr to a logfile 
// return 0 on success 
int vdev_log_redirect( char const* logfile_path ) {
   
   int logfd = 0;
   int rc = 0;
   FILE* f = NULL;
   
   f = fopen( logfile_path, "a" );
   if( f == NULL ) {
      
      rc = -errno;
      vdev_error("fopen('%s') rc = %d\n", logfile_path, rc );
      return rc;
   }
   
   logfd = fileno(f);
   
   close( STDOUT_FILENO );
   close( STDERR_FILENO );
   
   rc = dup2( logfd, STDOUT_FILENO );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("dup2(STDOUT) rc = %d\n", rc );
      return rc;
   }
   
   rc = dup2( logfd, STDERR_FILENO );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("dup2(STDERR) rc = %d\n", rc );
      
      return rc;
   }
   
   fclose( f );
   logfd = -1;
   
   return 0;
}


// write a pidfile to a path 
int vdev_pidfile_write( char const* pidfile_path ) {
   
   char pidbuf[50];
   FILE* f = NULL;
   size_t nw = 0;
   int rc = 0;
   
   f = fopen( pidfile_path, "w+" );
   if( f == NULL ) {
      
      rc = -errno;
      vdev_error("fopen('%s') rc = %d\n", pidfile_path, rc );
      return rc;
   }
   
   memset( pidbuf, 0, 50 );
   sprintf( pidbuf, "%d\n", getpid() );
   
   rc = vdev_write_uninterrupted( fileno(f), pidbuf, strlen(pidbuf) + 1 );
   
   fclose( f );
   
   if( rc < 0 ) {
      
      vdev_error("vdev_write_uninterrupted('%d') rc = %d\n", getpid(), rc );
      return rc;
   }
   
   return 0;
}


// join two paths, writing the result to dest if dest is not NULL.
// otherwise, allocate and return a buffer containing the joined paths.
// return NULL on OOM 
char* vdev_fullpath( char const* root, char const* path, char* dest ) {

   char delim = 0;
   int path_off = 0;
   int len = 0;
   
   if( root == NULL || path == NULL ) {
      return NULL;
   }
   
   len = strlen(path) + strlen(root) + 2;

   if( strlen(root) > 0 ) {
      size_t root_delim_off = strlen(root) - 1;
      if( root[root_delim_off] != '/' && path[0] != '/' ) {
         len++;
         delim = '/';
      }
      else if( root[root_delim_off] == '/' && path[0] == '/' ) {
         path_off = 1;
      }
   }

   if( dest == NULL ) {
      dest = VDEV_CALLOC( char, len );
      if( dest == NULL ) {
         return NULL;
      }
   }

   memset(dest, 0, len);

   strcpy( dest, root );
   if( delim != 0 ) {
      dest[strlen(dest)] = '/';
   }
   strcat( dest, path + path_off );

   return dest;
}


// get the directory name of a path.
// put it into dest if dest is not null.
// otherwise, allocate a buffer and return it.
// return NULL on OOM 
char* vdev_dirname( char const* path, char* dest ) {
   
   if( path == NULL ) {
      return NULL;
   }
   
   if( dest == NULL ) {
      dest = VDEV_CALLOC( char, strlen(path) + 1 );
      if( dest == NULL ) {
         return NULL;
      }
   }

   // is this root?
   if( strlen(path) == 0 || strcmp( path, "/" ) == 0 ) {
      strcpy( dest, "/" );
      return dest;
   }

   int delim_i = strlen(path);
   if( path[delim_i] == '/' ) {
      delim_i--;
   }

   for( ; delim_i >= 0; delim_i-- ) {
      if( path[delim_i] == '/' ) {
         break;
      }
   }

   if( delim_i < 0 ) {
      delim_i = 0;
   }

   if( delim_i == 0 && path[0] == '/' ) {
      delim_i = 1;
   }

   strncpy( dest, path, delim_i );
   dest[delim_i+1] = '\0';
   return dest;
}

// determine how long the basename of a path is
// return 0 if the length could not be determined
size_t vdev_basename_len( char const* path ) {
   
   if( path == NULL ) {
      return 0;
   }
   
   int delim_i = strlen(path) - 1;
   if( delim_i <= 0 ) {
      return 0;
   }
   if( path[delim_i] == '/' ) {
      // this path ends with '/', so skip over it if it isn't /
      if( delim_i > 0 ) {
         delim_i--;
      }
   }
   for( ; delim_i >= 0; delim_i-- ) {
      if( path[delim_i] == '/' ) {
         break;
      }
   }
   delim_i++;

   return strlen(path) - delim_i;
}


// get the basename of a (non-directory) path.
// put it into dest, if dest is not null.
// otherwise, allocate a buffer with it and return the buffer
// return NULL on OOM, or if path is NULL
char* vdev_basename( char const* path, char* dest ) {

   size_t len = 0;
   
   if( path == NULL ) {
      return NULL;
   }
   
   len = vdev_basename_len( path );

   if( dest == NULL ) {
      
      dest = VDEV_CALLOC( char, len + 1 );
      if( dest == NULL ) {
         
         return NULL;
      }
   }
   else {
      memset( dest, 0, len + 1 );
   }

   strncpy( dest, path + strlen(path) - len, len );
   return dest;
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
   
   if( cmd == NULL ) {
      return -EINVAL;
   }
   
   // open the pipe 
   rc = pipe( p );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("pipe() rc = %d\n", rc );
      return rc;
   }
   
   max_fd = sysconf(_SC_OPEN_MAX);
   
   // fork the child 
   pid = fork();
   
   if( pid == 0 ) {
      
      // send stdout to p[1]
      close( p[0] );
      rc = dup2( p[1], STDOUT_FILENO );
      
      if( rc < 0 ) {
         
         rc = errno;
         close( p[1] );
         exit(rc);
      }
      
      // close everything else but stdout
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
         
         char** noenv = { NULL };
         rc = execle( "/bin/sh", "sh", "-c", cmd, (char*)0, noenv );
      }
      
      if( rc != 0 ) {
         
         rc = errno;
         exit(rc);
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
               close( p[0] );
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
         vdev_error("waitpid(%d) rc = %d\n", pid, rc );
         
         close( p[0] );
         
         if( alloced ) {
            
            free( *output );
            *output = NULL;
         }
         
         return rc;
      }
      
      if( WIFEXITED( status ) ) {
         
         *exit_status = WEXITSTATUS( status );
      }
      else if( WIFSIGNALED( status ) ) {
         
         vdev_error("WARN: command '%s' failed with signal %d\n", cmd, WTERMSIG( status ) );
         *exit_status = status;
      }
      else {
         
         // indicate start/stop
         *exit_status = -EPERM;
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
// return number of bytes written on success 
// return -errno on I/O error
ssize_t vdev_write_uninterrupted( int fd, char const* buf, size_t len ) {
   
   ssize_t num_written = 0;
   
   if( buf == NULL ) {
      return -EINVAL;
   }
   
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
// return number of bytes read on success 
// return -errno on I/O error
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


// read a whole file, masking EINTR 
// return 0 on success, filling buf with up to len bytes 
// return -errno on open or read errors
int vdev_read_file( char const* path, char* buf, size_t len ) {
   
   int fd = 0;
   int rc = 0;
   
   if( path == NULL || buf == NULL ) {
      return -EINVAL;
   }
   
   fd = open( path, O_RDONLY );
   if( fd < 0 ) {
      
      rc = -errno;
      vdev_error("open('%s') rc = %d\n", path, rc );
      return rc;
   }
   
   rc = vdev_read_uninterrupted( fd, buf, len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("vdev_read_uninterrupted('%s') rc = %d\n", path, rc );
      
      close( fd );
      return rc;
   }
   
   close( fd );
   return 0;
}

// write a whole file, masking EINTR 
// return 0 on success
// return -EINVAL if buf or path is null 
// return -errno on failiure to open or write or fsync 
int vdev_write_file( char const* path, char const* buf, size_t len, int flags, mode_t mode ) {
   
   int fd = 0;
   int rc = 0;
   
   if( path == NULL || buf == NULL ) {
      return -EINVAL;
   }
   
   fd = open( path, flags, mode );
   if( fd < 0 ) {
      
      rc = -errno;
      vdev_error("open('%s') rc = %d\n", path, rc );
      return rc;
   }
   
   rc = vdev_write_uninterrupted( fd, buf, len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("vdev_write_uninterrupted('%s') rc = %d\n", path, rc );
      
      close( fd );
      return rc;
   }
   
   // NOTE: not much we can do if close() fails...
   close( fd );
      
   return rc;
}


// free a list of dirents 
// always succeeds
static void vdev_dirents_free( struct dirent** dirents, int num_entries ) {
   
   if( dirents == NULL ) {
      return;
   }
   
   for( int i = 0; i < num_entries; i++ ) {
      
      if( dirents[i] == NULL ) {
         continue;
      }
      
      free( dirents[i] );
      dirents[i] = NULL;
   }
   
   free( dirents );
}


// portable scandirat() implementation--behaves as close to the Linux scandirat(3) interface as possible.
int vdev_portable_scandirat( int dirfd, char const* dirp, struct dirent ***namelist, int (*filter)( const struct dirent* ), int (*compar)( const struct dirent**, const struct dirent** ) ) {
   
   int rc = 0;
   
   DIR* dirh = NULL;
   struct dirent next;
   struct dirent* result = NULL;
   struct dirent* next_dup = NULL;
   int dirp_fd = -1;
   
   struct dirent** namelist_buf = NULL;
   struct dirent** namelist_tmp = NULL;
   ssize_t num_dirents = 0;
   ssize_t num_dirents_capacity = 0;
   
   dirp_fd = openat( dirfd, dirp, O_DIRECTORY );
   if( dirp_fd < 0 ) {
      
      rc = -errno;
      return rc;
   }
   
   // open the duplicated handle instead
   dirh = fdopendir( dirp_fd );
   if( dirh == NULL ) {
      
      rc = -errno;
      
      close( dirp_fd );
      return rc;
   }
   
   while( 1 ) {
      
      // next dirent 
      rc = readdir_r( dirh, &next, &result );
      
      // done?
      if( result == NULL ) {
         break;
      }
      
      // error?
      if( rc != 0 ) {
         
         rc = -rc;
         break;
      }
      
      // filter?
      if( filter != NULL ) {
         
         rc = (*filter)( &next );
         if( rc == 0 ) {
            
            // skip
            continue;
         }
      }
      
      // increase (double) space?
      if( num_dirents >= num_dirents_capacity ) {
         
         if( num_dirents_capacity == 0 ) {
            num_dirents_capacity = 1;
         }
         
         num_dirents_capacity <<= 1;
         
         namelist_tmp = (struct dirent**)realloc( namelist_buf, (num_dirents_capacity + 1) * sizeof( struct dirent ) );
         if( namelist_tmp == NULL ) {
            
            // OOM 
            rc = -ENOMEM;
            break;
         }
         
         namelist_buf = namelist_tmp;
      }
      
      // duplicate 
      next_dup = VDEV_CALLOC( struct dirent, 1 );
      if( next_dup == NULL ) {
         
         // OOM 
         rc = -ENOMEM;
         break;
      }
      
      memcpy( next_dup, &next, sizeof( struct dirent ) );
      
      // insert 
      namelist_buf[ num_dirents ] = next_dup;
      namelist_buf[ num_dirents+1 ] = NULL;
      num_dirents++;
   }
   
   closedir( dirh );
   
   if( rc < 0 ) {
      
      // error--clean up
      vdev_dirents_free( namelist_buf, num_dirents );
      return rc;
   }
   
   // sort?
   if( compar != NULL ) {
      
      qsort( (void*)namelist_buf, num_dirents, sizeof(struct dirent*), (int (*)(const void*, const void*))compar );
   }
   
   // finished!
   *namelist = namelist_buf;
   return num_dirents;
}


// process all files in a directory, given a descriptor to it.  Search in alphanumeric order
// return 0 on success 
// return -EINVAL if loader is NULL
// return -EBADF if dirfd is invalid 
// return non-zero of loader_at returns non-zero
int vdev_load_all_at( int dirfd, vdev_dirent_loader_at_t loader_at, void* cls ) {
   
   struct dirent** dirents = NULL;
   int num_entries = 0;
   int rc = 0;
   
   if( loader_at == NULL ) {
      return -EINVAL;
   }
   
   num_entries = vdev_portable_scandirat( dirfd, ".", &dirents, NULL, alphasort );
   if( num_entries < 0 ) {
      
      rc = -errno;
      vdev_error("scandirat(%d) rc = %d\n", dirfd, rc );
      return rc;
   }
   
   for( int i = 0; i < num_entries; i++ ) {
      
      // process this 
      rc = (*loader_at)( dirfd, dirents[i], cls );
      if( rc != 0 ) {
         
         vdev_error("loader_at(%d, '%s') rc = %d\n", dirfd, dirents[i]->d_name, rc );
         
         vdev_dirents_free( dirents, num_entries );
         return rc;
      }
   }
   
   vdev_dirents_free( dirents, num_entries );
   
   return rc;
}


// process all files in a directory with a loader, in alphanumeric order
// return 0 on success
// return -EINVAL if dir_path or loader are NULL 
// return -ENOMEM if OOM 
// return -errno on I/O error 
// return non-zero if loader fails
int vdev_load_all( char const* dir_path, vdev_dirent_loader_t loader, void* cls ) {
   
   struct dirent** dirents = NULL;
   int num_entries = 0;
   int rc = 0;
   
   if( dir_path == NULL || loader == NULL ) {
      return -EINVAL;
   }
     
   num_entries = scandir( dir_path, &dirents, NULL, alphasort );
   if( num_entries < 0 ) {
      
      rc = -errno;
      vdev_error("scandir('%s') rc = %d\n", dir_path, rc );
      return rc;
   }
   
   for( int i = 0; i < num_entries; i++ ) {
      
      // full path...
      char* fp = vdev_fullpath( dir_path, dirents[i]->d_name, NULL );
      if( fp == NULL ) {
         
         vdev_dirents_free( dirents, num_entries );
         return -ENOMEM;
      }
      
      // process this 
      rc = (*loader)( fp, cls );
      if( rc != 0 ) {
         
         vdev_error("loader('%s') rc = %d\n", fp, rc );
         
         free( fp );
         vdev_dirents_free( dirents, num_entries );
         return rc;
      }
      
      free( fp );
   }
   
   vdev_dirents_free( dirents, num_entries );
   
   return 0;
}


// make a string of directories, given the path dirp
// return 0 if the directory exists at the end of the call.
// return -EINVAL if dirp is NULL 
// return -ENOMEM if OOM 
// return negative if the directory could not be created.
int vdev_mkdirs( char const* dirp, int start, mode_t mode ) {
   
   unsigned int i = start;
   char* currdir = NULL;
   struct stat statbuf;
   int rc = 0;
   
   if( dirp == NULL ) {
      return -EINVAL;
   }
   
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


// try to remove a path of directories
// the must all be emtpy
// return 0 on success
// return negative on error
int vdev_rmdirs( char const* dirp ) {
   
   char* dirname = NULL;
   int rc = 0;
   
   if( dirp == NULL ) {
      return -EINVAL;
   }
   
   dirname = strdup( dirp );
   
   while( strlen(dirname) > 0 ) {
      
      rc = rmdir( dirname );
      
      if( rc != 0 ) {
         
         rc = -errno;
         break;
      }
      else {
         
         char* tmp = vdev_dirname( dirname, NULL );
         if( tmp == NULL ) {
            return -ENOMEM;
         }
         
         free( dirname );
         
         dirname = tmp;
      }
   }
   
   free( dirname );
   return rc;
}


// parse an unsigned 64-bit number 
// uint64_str must contain *only* the text of the number 
// return the number, and set *success to true if we succeeded
// return -EINVAL if any of hte arguments were NULL
// return 0, LLONG_MAX, or LLONG_MIN if we failed
uint64_t vdev_parse_uint64( char const* uint64_str, bool* success ) {
   
   if( uint64_str == NULL || success == NULL ) {
      return -EINVAL;
   }
   
   char* tmp = NULL;
   uint64_t uid = (uint64_t)strtoll( uint64_str, &tmp, 10 );
   
   if( tmp == uint64_str ) {
      *success = false;
   }
   else {
      *success = true;
   }
   
   return uid;
}


// given a null-terminated key=value string, chomp it into a null-terminated key and value.
// return the original length of the string.
// return -EINVAL if any argument is NULL
// return negative on error
int vdev_keyvalue_next( char* keyvalue, char** key, char** value ) {
   
   if( keyvalue == NULL || key == NULL || value == NULL ) {
      return -EINVAL;
   }
   
   int ret = strlen(keyvalue);
   char* tmp = NULL;
   
   // find the '='
   tmp = strchr(keyvalue, '=');
   if( tmp == NULL ) {
      
      return -EINVAL;
   }
   
   *key = keyvalue;
   *tmp = '\0';
   tmp++;
   
   *value = tmp;
   
   return ret;
}


// set up the library
// always succeeds
void vdev_setup_global(void) {
   
   // seed random from monotonic clock 
   struct timespec ts_mono;
   struct timespec ts_clock;
   int rc = 0;
   unsigned short seedv[3];
   int32_t lower_32;
   
   clock_gettime( CLOCK_MONOTONIC, &ts_mono );
   clock_gettime( CLOCK_REALTIME, &ts_clock );
   
   // NOTE: it's okay to use uninitialized memory for the random seed,
   // so we don't really care if clock_gettime fails 
   
   lower_32 = ts_mono.tv_nsec + ts_clock.tv_nsec;
   memcpy( seedv, &lower_32, 3 );
   
   seed48( seedv );
}


// portable cast pthread_t to uint64_t 
unsigned long long int vdev_pthread_self(void) {
   
   union {
      pthread_t t;
      uint64_t i;
   } vdev_pthread;

   vdev_pthread.i = 0;
   vdev_pthread.t = pthread_self();
   
   return vdev_pthread.i;
}
