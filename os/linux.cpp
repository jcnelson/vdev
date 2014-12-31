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

#ifdef _VDEV_OS_LINUX

#include "linux.h"

#include "workqueue.h"

// parse a uevent action 
static vdev_device_request_t vdev_linux_parse_device_request_type( char const* type ) {
   
   if( strcmp(type, "add") == 0 ) {
      return VDEV_DEVICE_ADD;
   }
   else if( strcmp(type, "remove") == 0 ) {
      return VDEV_DEVICE_REMOVE;
   }
   
   return VDEV_DEVICE_INVALID;
}

// see if a device is a block or character device, based on sysfs.
// set *mode to S_IFBLK if block, S_IFCHR if character, or 0 if neither 
// return 0 on success
// return negative on error (failed result of stating /sys/dev/(block|char)/$major:$minor)
static int vdev_linux_sysfs_read_device_mode( struct vdev_linux_context* ctx, unsigned int major, unsigned int minor, mode_t* mode ) {
   
   int rc = 0;
   struct stat sb;
   
   char char_dev_buf[100];
   char block_dev_buf[100];
   
   char* sysfs_char_path = NULL;
   char* sysfs_block_path = NULL;
   
   sprintf( char_dev_buf, "/dev/char/%u:%u", major, minor );
   sprintf( block_dev_buf, "/dev/block/%u:%u", major, minor );
   
   sysfs_char_path = fskit_fullpath( ctx->sysfs_mountpoint, char_dev_buf, NULL );
   if( sysfs_char_path == NULL ) {
      
      return -ENOMEM;
   }
   
   sysfs_block_path = fskit_fullpath( ctx->sysfs_mountpoint, block_dev_buf, NULL );
   if( sysfs_block_path == NULL ) {
      
      free( sysfs_char_path );
      return -ENOMEM;
   }
   
   rc = stat( sysfs_char_path, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      if( rc != -ENOENT ) {
         
         // some other error 
         vdev_error("stat('%s') rc = %d\n", sysfs_char_path, rc );
      }
      else {
         
         // not found; maybe it's a block device 
         rc = stat( sysfs_block_path, &sb );
         
         if( rc != 0 ) {
            
            rc = -errno;
            if( rc != -ENOENT ) {
               
               // some other error 
               vdev_error("stat('%s') rc = %d\n", sysfs_block_path, rc );
            }
            else {
               
               // not found--neither a block nor a char 
               *mode = 0;
            }
         }
         else {
            
            // this is a block device 
            *mode = S_IFBLK;
         }
      }
   }
   else {
      
      // this is a character device 
      *mode = S_IFCHR;
   }
   
   free( sysfs_block_path );
   free( sysfs_char_path );
   
   return rc;
}


// make the full sysfs path from the dev path, plus an additional path 
static char* vdev_linux_sysfs_fullpath( char const* sysfs_mountpoint, char const* devpath, char const* attr_path ) {
   
   char* tmp = NULL;
   char* ret = NULL;
   
   tmp = fskit_fullpath( sysfs_mountpoint, devpath, NULL );
   if( tmp == NULL ) {
      return NULL;
   }
   
   ret = fskit_fullpath( tmp, attr_path, NULL );
   free( tmp );
   
   return ret;
}


// parse a device number pair 
static int vdev_linux_sysfs_parse_device_nums( char const* devbuf, unsigned int* major, unsigned int* minor ) {
   
   int rc = 0;
   
   // parse devpath 
   rc = sscanf( devbuf, "%u:%u", major, minor );
   
   if( rc != 2 ) {
      
      vdev_error("sscanf('%s') for major:minor rc = %d\n", devbuf, rc );
      rc = -EINVAL;
   }
   else {
      rc = 0;
   }

   return rc;
}

// read the device major and minor number, using the devpath 
static int vdev_linux_sysfs_read_dev_nums( struct vdev_linux_context* ctx, char const* devpath, unsigned int* major, unsigned int* minor ) {
   
   int rc = 0;
   int fd = 0;
   ssize_t nr = 0;
   char devbuf[101];
   
   memset( devbuf, 0, 101 );
   
   char* full_devpath = vdev_linux_sysfs_fullpath( ctx->sysfs_mountpoint, devpath, "dev" );
   if( full_devpath == NULL ) {
      return -ENOMEM;
   }
   
   // open device path 
   fd = open( full_devpath, O_RDONLY );
   if( fd < 0 ) {
      
      rc = -errno;
      vdev_error("open('%s') rc = %d\n", full_devpath, rc );
      
      free( full_devpath );
      return rc;
   }
   
   nr = vdev_read_uninterrupted( fd, devbuf, 100 );
   if( nr < 0 ) {
      
      rc = nr;
      vdev_error("read('%s') rc = %d\n", full_devpath, rc );
      
      free( full_devpath );
      close( fd );
      return rc;
   }
   
   close( fd );
   free( full_devpath );
   
   rc = vdev_linux_sysfs_parse_device_nums( devbuf, major, minor );
   
   if( rc != 0 ) {
      
      vdev_error("Failed to parse '%s'\n", devbuf );
      rc = -EIO;
   }
   
   return rc;
}

// read the kernel-given device subsystem from sysfs 
static int vdev_linux_sysfs_read_subsystem( struct vdev_linux_context* ctx, char const* devpath, char** subsystem ) {
   
   int rc = 0;
   char linkpath[PATH_MAX+1];
   size_t linkpath_len = PATH_MAX;
   char* subsystem_path = NULL;
   
   memset( linkpath, 0, PATH_MAX+1 );
   
   subsystem_path = vdev_linux_sysfs_fullpath( ctx->sysfs_mountpoint, devpath, "subsystem" );
   if( subsystem_path == NULL ) {
      return -ENOMEM;
   }
   
   rc = readlink( subsystem_path, linkpath, linkpath_len );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("readlink('%s') rc = %d\n", subsystem_path, rc );
      free( subsystem_path );
      return rc;
   }
   
   free( subsystem_path );
   
   *subsystem = fskit_basename( linkpath, NULL );
   return 0;
}


// parse a uevent, and use the information to fill in a device request.
// NOTE: This *modifies* buf
// return 0 on success
static int vdev_linux_parse_request( struct vdev_linux_context* ctx, struct vdev_device_request* vreq, char* nlbuf, ssize_t buflen ) {
   
   
   char* buf = nlbuf;
   char* key = NULL;
   char* value = NULL;
   int offset = 0;
   int rc = 0;
   unsigned int major = 0;
   unsigned int minor = 0;
   bool have_major = false;
   bool have_minor = true;
   mode_t dev_mode = 0;
   
   char* devpath = NULL;
   char* subsystem = NULL;
   
   vdev_device_request_t reqtype = VDEV_DEVICE_INVALID;
   
   for( int i = 0; i < buflen; ) {
      vdev_debug("Netlink %p: '%s'\n", vreq, buf + i );
      i += strlen(buf + i) + 1;
   }
   
   // sanity check: first line is $action@$devpath
   if( strchr(buf, '@') == NULL ) { 
      
      vdev_error("Invalid request header '%s'\n", buf);
      return -EINVAL;
   }
   
   // advance to the next line
   offset += strlen(buf) + 1;
   
   // get key/value pairs
   while( offset < buflen ) {
      
      // sanity check: must have '='
      key = buf + offset;
      value = strchr(key, '=');
      
      if( value == NULL ) {
         // invalid line 
         vdev_error("Invalid line '%s'\n", key);
         
         return -EINVAL;
      }
      
      // next line
      offset += strlen(key) + 1;
      
      // separate key and value 
      *value = '\0';
      value++;
      
      rc = vdev_device_request_add_param( vreq, key, value );
      if( rc != 0 ) {
         
         vdev_error("vdev_device_request_add_param( '%s' = '%s' ) rc = %d\n", key, value, rc );
         
         return -EINVAL;
      }
         
      // is this the action to take?
      if( strcmp(key, "ACTION") == 0 ) {
         
         reqtype = vdev_linux_parse_device_request_type( value );
         
         if( reqtype == VDEV_DEVICE_INVALID ) {
            
            vdev_error("Invalid ACTION '%s'\n", value );
            
            return -EINVAL;
         }
         
         vdev_device_request_set_type( vreq, reqtype );
      }
      
      // is this the sysfs device path?
      else if( strcmp(key, "DEVPATH") == 0 ) {
         
         devpath = value;
      }
      
      // subsystem given?
      else if( strcmp(key, "SUBSYSTEM") == 0 ) {
         
         subsystem = value;
         
         // early check--if this is a block, remember so 
         if( strcasecmp(value, "block") == 0 ) {
            
            dev_mode = S_IFBLK;
         }
      }
      
      // is this the major device number?
      else if( strcmp(key, "MAJOR") == 0 && !have_major ) {
         
         char* tmp = NULL;
         major = (int)strtol( value, &tmp, 10 );
         
         if( tmp == NULL ) {
            
            vdev_error("Invalid 'MAJOR' value '%s'\n", value);
            return -EINVAL;
         }
         
         have_major = true;
      }
      
      // is this the minor device number?
      else if( strcmp(key, "MINOR") == 0 && !have_minor ) {
         
         char* tmp = NULL;
         minor = (int)strtol( value, &tmp, 10 ) ;
         
         if( tmp == NULL ) {
            
            vdev_error("Invalid 'MINOR' value '%s'\n", value );
            return -EINVAL;
         }
         
         have_minor = true;
      }
   }
   
   if( reqtype == VDEV_DEVICE_INVALID ) {
      
      vdev_error("%s", "No ACTION given\n");
      
      return -EINVAL;
   }
   
   if( devpath != NULL ) {
      
      // get any remaining information from sysfs 
      // check major/minor?
      if( !have_major || !have_minor ) {
         
         // see if we have major/minor device numbers for this device...
         rc = vdev_linux_sysfs_read_dev_nums( ctx, devpath, &major, &minor );
         
         if( rc == 0 ) {
            
            // yup!
            vdev_device_request_set_dev( vreq, makedev(major, minor) );
            
            have_major = true;
            have_minor = true;
         }
      }
      
      // subsystem?
      if( subsystem == NULL ) {
         
         // see if we have a subsystem 
         rc = vdev_linux_sysfs_read_subsystem( ctx, devpath, &subsystem );
         
         if( rc == 0 ) {
            
            // yup!
            vdev_device_request_add_param( vreq, "SUBSYSTEM", subsystem );
            free( subsystem );
         }
      }
   }
   
   if( have_major && have_minor && dev_mode == 0 ) {
      
      // explicit major and minor device numbers given 
      vdev_device_request_set_dev( vreq, makedev(major, minor) );
      
      if( dev_mode == 0 ) {
         
         // don't know the mode yet 
         rc = vdev_linux_sysfs_read_device_mode( ctx, major, minor, &dev_mode );
      
         if( rc == 0 ) {
         
            // yup!
            vdev_device_request_set_mode( vreq, dev_mode );
         }
      }
      else {
         
         vdev_device_request_set_mode( vreq, dev_mode );
      }
   }
   return 0;
}


// yield the next device event
int vdev_os_next_device( struct vdev_device_request* vreq, void* cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   char buf[VDEV_LINUX_NETLINK_BUF_MAX];
   ssize_t len = 0;
   
   char cbuf[CMSG_SPACE(sizeof(struct ucred))];
   struct cmsghdr *chdr = NULL;
   struct ucred *cred = NULL;
   struct msghdr hdr;
   struct iovec iov;
   struct sockaddr_nl cnls;
   
   // do we have initial requests?
   if( ctx->initial_requests->size() > 0 ) {
      
      // next request
      struct vdev_device_request* req = ctx->initial_requests->front();
      ctx->initial_requests->erase( ctx->initial_requests->begin() );
      
      memcpy( vreq, req, sizeof(struct vdev_device_request) );
      free( req );
      
      return 0;
   }
   
   memset(&hdr, 0, sizeof(struct msghdr));
   
   // next event (wait forever)
   // NOTE: this is a cancellation point!
   rc = poll( &ctx->pfd, 1, -1 );
   
   if( rc < 0 ) {
      
      rc = -errno;
      
      if( rc == -EINTR ) {
         // try again 
         return -EAGAIN;
      }
      
      vdev_error("poll(%d) rc = %d\n", ctx->pfd.fd, rc );
      
      return rc;
   }
   
   // get the event 
   iov.iov_base = buf;
   iov.iov_len = VDEV_LINUX_NETLINK_BUF_MAX;
                
   hdr.msg_iov = &iov;
   hdr.msg_iovlen = 1;
   
   // get control-plane messages
   hdr.msg_control = cbuf;
   hdr.msg_controllen = sizeof(cbuf);
   
   hdr.msg_name = &cnls;
   hdr.msg_namelen = sizeof(cnls);

   // get the event 
   len = recvmsg( ctx->pfd.fd, &hdr, 0 );
   if( len < 0 ) {
      
      rc = -errno;
      vdev_error("recvmsg(%d) rc = %d\n", ctx->pfd.fd, rc );
      
      return rc;
   }
   
   // big enough?
   if( len < 32 || len >= VDEV_LINUX_NETLINK_BUF_MAX ) {
      
      vdev_error("Netlink message is %zd bytes; ignoring...\n", len );
      return 0;
   }
   
   // control message, for credentials
   chdr = CMSG_FIRSTHDR( &hdr );
   if( chdr == NULL || chdr->cmsg_type != SCM_CREDENTIALS ) {
      
      vdev_error("%s", "Netlink message has no credentials\n");
      return 0;
   }
   
   // get the credentials
   cred = (struct ucred *)CMSG_DATA(chdr);
   
   // if not root, ignore 
   if( cred->uid != 0 ) {
      
      vdev_error("Ignoring message from non-root ID %d\n", cred->uid );
      return 0;
   }
   
   // if udev, ignore 
   if( memcmp( buf, VDEV_LINUX_NETLINK_UDEV_HEADER, VDEV_LINUX_NETLINK_UDEV_HEADER_LEN ) == 0 ) {
      
      // message from udev; ignore 
      return 0;
   }
   
   // kernel messages don't come from userspace 
   if( cnls.nl_pid > 0 ) {
      
      // from userspace???
      return 0;
   }
   
   // parse the event buffer
   rc = vdev_linux_parse_request( ctx, vreq, buf, len );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_parse_request rc = %d\n", rc );
      
      return rc;
   }
   
   return 0;
}


// find sysfs mountpoint in /proc/mounts
// this is apparently superfluous (it *should* be mounted at /sys), but you never know.
// mountpoint must be big enough (PATH_MAX will do)
// return 0 on success
// return -ENOSYS if sysfs is not mounted
// return -ENOMEM if the buffer isn't big enough 
// return negative for some other errors (like access permission failures, or /proc not mounted)
static int vdev_linux_find_sysfs_mountpoint( char* mountpoint, size_t mountpoint_len ) {
   
   FILE* f = NULL;
   int rc = 0;
   
   char* line_buf = NULL;
   size_t line_len = 0;
   ssize_t num_read = 0;
   
   bool found = false;
   
   f = fopen( "/proc/mounts", "r" );
   if( f == NULL ) {
      
      rc = -errno;
      fprintf(stderr, "Failed to open /proc/mounts, rc = %d\n", rc );
      return rc;
   }
   
   // scan for sysfs 
   while( 1 ) {
      
      errno = 0;
      num_read = getline( &line_buf, &line_len, f );
      
      if( num_read < 0 ) {
         
         rc = -errno;
         if( rc == 0 ) {
            // EOF
            break;
         }
         else {
            // error 
            vdev_error("getline('/proc/mounts') rc = %d\n", rc );
            break;
         }
      }
      
      // sysfs?
      if( strncmp( line_buf, "sysfs ", 6 ) == 0 ) {
         
         // mountpoint?
         char sysfs_buf[10];
         rc = sscanf( line_buf, "%s %s", sysfs_buf, mountpoint );
         
         if( rc != 2 ) {
            
            // couldn't scan 
            vdev_error("WARN: sscanf(\"%s\") for sysfs mountpoint rc = %d\n", line_buf, rc );
            continue;
         }
         else {
            
            // got it!
            rc = 0;
            found = true;
            break;
         }
      }
   }
   
   if( line_buf != NULL ) {
      free( line_buf );
   }
   
   if( !found ) {
      fprintf(stderr, "Failed to find mounted sysfs\n");
      return -ENOSYS;
   }
   
   return rc;
}

// register a block/character device from sysfs
static int vdev_linux_sysfs_register_device( struct vdev_linux_context* ctx, char const* fp, mode_t mode ) {
   
   int fd = 0;
   int rc = 0;
   char devbuf[100];
   unsigned int major = 0;
   unsigned int minor = 0;
   char* subsystem = NULL;
   char name[NAME_MAX+1];
   struct stat sb;
   char* fp_parent = NULL;
   
   memset( devbuf, 0, 100 );
   
   // skip non-regular files
   rc = stat( fp, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("stat('%s') rc = %d\n", fp, rc );
      return rc;
   }
   
   if( !S_ISREG( sb.st_mode ) ) {
      
      // only care about regular files
      return 0;
   }
   
   // get parent path 
   fp_parent = fskit_dirname( fp, NULL );
   if( fp_parent == NULL ) {
      return -ENOMEM;
   }
   
   struct vdev_device_request* vreq = VDEV_CALLOC( struct vdev_device_request, 1 );
   if( vreq == NULL ) {
      return -ENOMEM;
   }
   
   // read this file 
   fd = open( fp, O_RDONLY );
   if( fd < 0 ) {
      
      rc = -errno;
      vdev_error("open('%s') rc = %d\n", fp, rc );
      
      free( fp_parent );
      free( vreq );
      return rc;
   }
   
   rc = vdev_read_uninterrupted( fd, devbuf, 100 );
   if( rc < 0 ) {
      
      vdev_error("vdev_read_uninterrupted('%s') rc = %d\n", fp, rc );
      close( fd );
      
      free( fp_parent );
      free( vreq );
      return rc;
   }
   
   close( fd );
   
   // get major/minor numbers
   rc = vdev_linux_sysfs_parse_device_nums( devbuf, &major, &minor );
   if( rc != 0 ) {
      
      vdev_error("Failed to parse '%s'\n", devbuf );
      
      free( fp_parent );
      free( vreq );
      return rc;
   }
   
   // get device name: basename of the parent directory 
   fskit_basename( fp_parent, name );
   
   // get subsystem 
   rc = vdev_linux_sysfs_read_subsystem( ctx, fp_parent + strlen(ctx->sysfs_mountpoint), &subsystem );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_sysfs_read_subsystem('%s') rc = %d\n", fp_parent, rc );
      free( fp_parent );
      free( vreq );
      return -ENODATA;
   }
   
   try {
      
      vdev_device_request_init( vreq, ctx->os_ctx->state, VDEV_DEVICE_ADD, name );
      vdev_device_request_set_dev( vreq, makedev(major, minor) );
      vdev_device_request_set_mode( vreq, mode );
      
      if( subsystem != NULL ) {
         vdev_device_request_add_param( vreq, "SUBSYSTEM", subsystem );
      }
      
      ctx->initial_requests->push_back( vreq );
   }
   catch( bad_alloc& ba ) {
      rc = -ENOMEM;
   }
   
   if( subsystem != NULL ) {
      free( subsystem );
   }
   
   free( fp_parent );
   
   return rc;
}


// get all block device paths from sysfs 
static int vdev_linux_sysfs_walk_block_devs( struct vdev_linux_context* ctx, glob_t* block_device_paths ) {
   
   int rc = 0;
   char sysfs_glob[ PATH_MAX+1 ];
   
   memset( sysfs_glob, 0, PATH_MAX+1 );
   
   // walk sysfs for block devices (in /sys/block/*/dev, /sys/block/*/*/dev)
   snprintf( sysfs_glob, PATH_MAX, "%s/block/*/dev", ctx->sysfs_mountpoint );
   
   rc = glob( sysfs_glob, 0, NULL, block_device_paths );
   
   if( rc != 0 ) {
      
      vdev_error("glob('%s') rc = %d\n", sysfs_glob, rc );
      return rc;
   }
   
   // append /sys/block/*/*/dev 
   snprintf( sysfs_glob, PATH_MAX, "%s/block/*/*/dev", ctx->sysfs_mountpoint );
   
   rc = glob( sysfs_glob, GLOB_APPEND, NULL, block_device_paths );
   
   if( rc != 0 ) {
      
      vdev_error("glob('%s') rc = %d\n", sysfs_glob, rc );
      return rc;
   }
   
   return 0;
}


// build up a glob of /sys/class/*/*/dev, excluding /sys/class/block
static int vdev_linux_sysfs_build_char_glob( char const* fp, void* cls ) {
   
   glob_t* char_device_paths = (glob_t*)cls;
   
   char sysfs_glob[ PATH_MAX+1 ];
   char name[NAME_MAX+1];
   int rc = 0;
   
   fskit_basename( fp, name );
   
   // skip . and ..
   if( strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ) {
      return 0;
   }
   
   if( strcmp(name, "block" ) != 0 ) {
      
      // scan this directory 
      snprintf(sysfs_glob, PATH_MAX, "%s/*/dev", fp );
      
      rc = glob( sysfs_glob, GLOB_APPEND, NULL, char_device_paths );
      if( rc != 0 && rc != GLOB_NOMATCH ) {
         
         vdev_error("glob('%s') rc = %d\n", sysfs_glob, rc );
         return rc;
      }
   }
   
   return 0;
}


// get all char device paths from sysfs 
static int vdev_linux_sysfs_walk_char_devs( struct vdev_linux_context* ctx, glob_t* char_device_paths ) {
   
   int rc = 0;
   char sysfs_glob[ PATH_MAX+1 ];
   
   memset( sysfs_glob, 0, PATH_MAX+1 );
   
   // character devices are in /sys/class/*/*/dev, /sys/bus/*/devices/*/dev.
   // skip /sys/class/block/*/dev
   
   snprintf( sysfs_glob, PATH_MAX, "%s/bus/*/devices/*/dev", ctx->sysfs_mountpoint );
   
   rc = glob( sysfs_glob, 0, NULL, char_device_paths );
   
   if( rc != 0 ) {
      
      vdev_error("glob('%s') rc = %d\n", sysfs_glob, rc );
      return rc;
   }
   
   snprintf( sysfs_glob, PATH_MAX, "%s/class/", ctx->sysfs_mountpoint );
   
   rc = vdev_load_all( sysfs_glob, vdev_linux_sysfs_build_char_glob, char_device_paths );
   if( rc != 0 ) {
      
      vdev_error("vdev_load_all('%s') rc = %d\n", sysfs_glob, rc );
      return rc;
   }
   
   
   return 0;
}


// read all (block|character) devices from sysfs 
static int vdev_linux_sysfs_walk_devs( struct vdev_linux_context* ctx ) {
   
   int rc = 0;
   glob_t char_device_paths;
   glob_t block_device_paths;
   
   // get block devices 
   rc = vdev_linux_sysfs_walk_block_devs( ctx, &block_device_paths );
   
   if( rc != 0 ) {
      
      vdev_error( "vdev_linux_sysfs_walk_block_devs rc = %d\n", rc );
      return rc;
   }
   
   // get character devices
   rc = vdev_linux_sysfs_walk_char_devs( ctx, &char_device_paths );
   
   if( rc != 0 ) {
      
      vdev_error( "vdev_linux_sysfs_walk_char_devs rc = %d\n", rc );
      return rc;
   }
   
   // populate block devices 
   for( unsigned int i = 0; block_device_paths.gl_pathv[i] != NULL; i++ ) {
      
      vdev_debug("Discovered block device '%s'\n", block_device_paths.gl_pathv[i] );
      
      rc = vdev_linux_sysfs_register_device( ctx, block_device_paths.gl_pathv[i], S_IFBLK );
      if( rc != 0 ) {
         
         vdev_error("vdev_linux_sysfs_register_device('%s', BLK) rc = %d\n", block_device_paths.gl_pathv[i], rc );
         break;
      }
   }
   
   if( rc != 0 ) {
      
      globfree( &char_device_paths );
      globfree( &block_device_paths );
      return rc;
   }
   
   // populate char devices 
   for( unsigned int i = 0; char_device_paths.gl_pathv[i] != NULL; i++ ) {
      
      vdev_debug("Discovered char device '%s'\n", char_device_paths.gl_pathv[i] );
      
      rc = vdev_linux_sysfs_register_device( ctx, char_device_paths.gl_pathv[i], S_IFCHR );
      if( rc != 0 ) {
         
         vdev_error("vdev_linux_sysfs_register_device('%s', CHR) rc = %d\n", char_device_paths.gl_pathv[i], rc );
         break;
      }
   }
   
   globfree( &char_device_paths );
   globfree( &block_device_paths );
   
   return rc;
}


// seed device-add events from sysfs
// walk through sysfs (if mounted), and write to every device's uevent to get the kernel to send the "add" message
static int vdev_linux_sysfs_trigger_events( struct vdev_linux_context* ctx ) {
   
   int rc = 0;
   char sysfs_glob[ PATH_MAX+1 ];
   glob_t uevents;
   int fd = 0;
   
   memset( sysfs_glob, 0, PATH_MAX+1 );
   
   // walk sysfs for uevents 
   snprintf( sysfs_glob, PATH_MAX, "%s/class/*/*/uevent", ctx->sysfs_mountpoint );
   
   rc = glob( sysfs_glob, 0, NULL, &uevents );
   
   if( rc != 0 ) {
      
      vdev_error("glob(\"%s\") rc = %d\n", sysfs_glob, rc );
      return rc;
   }
   
   // ask the kernel to re-send an "add" event for this device 
   for( unsigned int i = 0; i < uevents.gl_pathc; i++ ) {
      
      fd = open( uevents.gl_pathv[i], O_WRONLY );
      if( fd < 0 ) {
         
         rc = -errno;
         vdev_error("open(\"%s\") rc = %d\n", uevents.gl_pathv[i], rc );
         
         // soldier on 
         continue;
      }
      
      rc = vdev_write_uninterrupted( fd, "add", 4 );
      if( rc != 4 ) {
         
         rc = -errno;
         vdev_error("write(\"%s\", \"add\") rc = %d\n", uevents.gl_pathv[i], rc );
         
         // soldier on 
         close( fd );
         continue;
      }
      
      close( fd );
   }
   
   globfree( &uevents );
   
   return 0;
}


// start listening for kernel events via netlink
static int vdev_linux_context_init( struct vdev_os_context* os_ctx, struct vdev_linux_context* ctx ) {
   
   int rc = 0;
   size_t slen = VDEV_LINUX_NETLINK_RECV_BUF_MAX;
   
   memset( ctx, 0, sizeof(struct vdev_linux_context) );
   
   ctx->initial_requests = new (nothrow) vector<struct vdev_device_request*>();
   if( ctx->initial_requests == NULL ) {
      return -ENOMEM;
   }
   
   ctx->os_ctx = os_ctx;
   
   ctx->nl_addr.nl_family = AF_NETLINK;
   ctx->nl_addr.nl_pid = getpid();
   ctx->nl_addr.nl_groups = -1;
   
   ctx->pfd.fd = socket( PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT );
   if( ctx->pfd.fd < 0 ) {
      
      rc = -errno;
      vdev_error("socket(PF_NETLINK) rc = %d\n", rc);
      return rc;
   }
   
   ctx->pfd.events = POLLIN;
   
   // big receive buffer, if running as root 
   if( geteuid() == 0 ) {
      rc = setsockopt( ctx->pfd.fd, SOL_SOCKET, SO_RCVBUFFORCE, &slen, sizeof(slen) );
      if( rc < 0 ) {
         
         rc = -errno;
         vdev_error("setsockopt(SO_RCVBUFFORCE) rc = %d\n", rc);
         
         close( ctx->pfd.fd );
         return rc;
      }
   }
   
   // check credentials of message--only root should be able talk to us
   rc = setsockopt( ctx->pfd.fd, SOL_SOCKET, SO_PASSCRED, &slen, sizeof(slen) );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("setsockopt(SO_PASSCRED) rc = %d\n", rc );
      
      close( ctx->pfd.fd );
      return rc;
      
   }
        
   // bind to the address
   rc = bind( ctx->pfd.fd, (struct sockaddr*)&ctx->nl_addr, sizeof(struct sockaddr_nl) );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("bind(%d) rc = %d\n", ctx->pfd.fd, rc );
      
      close( ctx->pfd.fd );
      return rc;
   }
   
   // lookup sysfs mountpoint
   rc = vdev_linux_find_sysfs_mountpoint( ctx->sysfs_mountpoint, PATH_MAX );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_find_sysfs_mountpoint rc = %d\n", rc );
      
      close( ctx->pfd.fd );
      return rc;
   }
   
   // seed block/character devices from sysfs 
   rc = vdev_linux_sysfs_walk_devs( ctx );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_sysfs_walk_devs rc = %d\n", rc );
      return rc;
   }
   
   return 0;
}


// stop listening 
static int vdev_linux_context_shutdown( struct vdev_linux_context* ctx ) {
   
   // shut down 
   if( ctx != NULL ) {
      close( ctx->pfd.fd );
      
      if( ctx->initial_requests != NULL ) {
         
         for( unsigned int i = 0; i < ctx->initial_requests->size(); i++ ) {
            
            vdev_device_request_free( ctx->initial_requests->at(i) );
            free( ctx->initial_requests->at(i) );
         }
         
         delete ctx->initial_requests;
      }
   }
   
   return 0;
}

/*
// abort loading firmware 
static int vdev_linux_load_firmware_abort( char const* sysfs_ctl_path, int ctl_fd ) {
   
   int rc = 0;
   
   rc = vdev_write_uninterrupted( ctl_fd, "-1", 3 );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("write(\"%s\", \"1\") rc = %d\n", sysfs_ctl_path, rc );
      
      return rc;
   }
   
   return rc;
}

// load firmware through sysfs 
static int vdev_linux_load_firmware( struct vdev_linux_context* ctx, char const* devpath, char const* firmware_name ) {
   
   // firmware path 
   char fw_path[PATH_MAX+1];
   char sysfs_ctl_path[PATH_MAX+1];
   char sysfs_fw_path[PATH_MAX+1];
   
   char buf[65536];
   
   int rc = 0;
   int ctl_fd = 0;
   int fw_fd = 0;
   int sysfs_fw_fd = 0;
   ssize_t nr = 0;
   
   // build firmware path
   fskit_fullpath( ctx->os_ctx->state->config->firmware_dir, firmware_name, fw_path );
   
   // build sysfs firmware output path
   fskit_fullpath( ctx->sysfs_mountpoint, devpath, sysfs_fw_path );
   fskit_fullpath( sysfs_fw_path, "data", sysfs_fw_path );
   
   // build sysfs firmware loading control path 
   fskit_fullpath( ctx->sysfs_mountpoint, devpath, sysfs_ctl_path );
   fskit_fullpath( sysfs_ctl_path, "loading", sysfs_ctl_path );
   
   // get the firmware 
   fw_fd = open( fw_path, O_RDONLY );
   if( fw_fd < 0 ) {
      
      rc = -errno;
      vdev_error("open(\"%s\") rc = %d\n", fw_path, rc );
      
      return rc;
   }
   
   // get the firmware destination 
   sysfs_fw_fd = open( sysfs_fw_path, O_WRONLY );
   if( sysfs_fw_fd < 0 ) {
      
      rc = -errno;
      vdev_error("open(\"%s\") rc = %d\n", sysfs_fw_path, rc );
      
      close( fw_fd );
      return rc;
   }
   
   // start loading 
   ctl_fd = open( sysfs_ctl_path, O_WRONLY );
   if( ctl_fd < 0 ) {
      
      rc = -errno;
      vdev_error("open(\"%s\") rc = %d\n", sysfs_ctl_path, rc );
      
      close( fw_fd );
      close( sysfs_fw_fd );
      return rc;
   }
   
   rc = vdev_write_uninterrupted( ctl_fd, "1", 2 );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("write(\"%s\", \"1\") rc = %d\n", sysfs_ctl_path, rc );
      
      close( fw_fd );
      close( ctl_fd );
      close( sysfs_fw_fd );
      return rc;
   }
   
   // start reading 
   while( 1 ) {
      
      nr = read( fw_fd, buf, 65536 );
      if( nr < 0 ) {
         
         // failure 
         vdev_linux_load_firmware_abort( sysfs_ctl_path, ctl_fd );
         
         close( fw_fd );
         close( ctl_fd );
         close( sysfs_fw_fd );
         return rc;
      }
      
      if( nr == 0 ) {
         
         // done!
         break;
      }
      
      nr = vdev_write_uninterrupted( sysfs_fw_fd, buf, nr );
      if( nr < 0 ) {
         
         // failure 
         vdev_linux_load_firmware_abort( sysfs_ctl_path, ctl_fd );
         
         close( fw_fd );
         close( ctl_fd );
         close( sysfs_fw_fd );
         return rc;
      }
   }
   
   // signal success!
   rc = vdev_write_uninterrupted( ctl_fd, "0", 2 );
   if( rc < 0 ) {
      
      rc = -errno;
      vdev_error("write(\"%s\", \"1\") rc = %d\n", sysfs_ctl_path, rc );
   }
   
   close( fw_fd );
   close( ctl_fd );
   close( sysfs_fw_fd );
   
   return rc;
}
*/

// set up Linux-specific vdev state.
// this is early initialization, so don't start anything yet
int vdev_os_init( struct vdev_os_context* os_ctx, void** cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = NULL;
   
   ctx = VDEV_CALLOC( struct vdev_linux_context, 1 );
   if( ctx == NULL ) {
      return -ENOMEM;
   }
   
   rc = vdev_linux_context_init( os_ctx, ctx );
   if( rc != 0 ) {
      
      free( ctx );
      return rc;
   }
   
   *cls = ctx;
   return 0;
}


// shut down Linux-specific vdev state 
int vdev_os_shutdown( void* cls ) {
   
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   
   vdev_linux_context_shutdown( ctx );
   free( ctx );
   
   return 0;
}


#endif