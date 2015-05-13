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

#ifdef _VDEV_OS_LINUX

#include "linux.h"
#include "workqueue.h"
#include "libvdev/sglib.h"

// parse a uevent action 
static vdev_device_request_t vdev_linux_parse_device_request_type( char const* type ) {
   
   if( strcmp(type, "add") == 0 ) {
      return VDEV_DEVICE_ADD;
   }
   else if( strcmp(type, "remove") == 0 ) {
      return VDEV_DEVICE_REMOVE;
   }
   else if( strcmp(type, "change") == 0 ) {
      return VDEV_DEVICE_CHANGE;
   }
   
   return VDEV_DEVICE_INVALID;
}


// make the full sysfs path from the dev path, plus an additional path 
// return NULL on OOM
static char* vdev_linux_sysfs_fullpath( char const* sysfs_mountpoint, char const* devpath, char const* attr_path ) {
   
   char* tmp = NULL;
   char* ret = NULL;
   
   tmp = vdev_fullpath( sysfs_mountpoint, devpath, NULL );
   if( tmp == NULL ) {
      return NULL;
   }
   
   ret = vdev_fullpath( tmp, attr_path, NULL );
   free( tmp );
   
   return ret;
}


// parse a device number pair 
// return 0 on success, and set *major and *minor
// return -EINVAL if we failed to parse
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
// return 0 on success, and set *major and *minor 
// return -ENOMEM on OOM 
// return -errno on failure to open or read
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
      
      if( rc != -ENOENT ) {
         vdev_error("open('%s') rc = %d\n", full_devpath, rc );
      }
      
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
// return 0 on success, and set *subsystem
// return -ENOMEM on OOM 
// return negative on readlink failure
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
   
   *subsystem = vdev_basename( linkpath, NULL );
   if( *subsystem == NULL ) {
      
      return -ENOMEM;
   }
   
   return 0;
}


// print a uevent, either with debugging or error loglevels
static int vdev_linux_log_uevent( char const* uevent_buf, size_t uevent_buf_len, bool debug ) {
      
   for( unsigned int i = 0; i < uevent_buf_len; ) {
      
      if( debug ) {
         vdev_debug("uevent '%s'\n", uevent_buf + i );
      }
      else {
         vdev_error("uevent '%s'\n", uevent_buf + i );
      }
      
      i += strlen(uevent_buf + i) + 1;
   }
   
   return 0;
}


#define vdev_linux_debug_uevent( uevent_buf, uevent_buf_len ) vdev_linux_log_uevent( uevent_buf, uevent_buf_len, true )
#define vdev_linux_error_uevent( uevent_buf, uevent_buf_len ) vdev_linux_log_uevent( uevent_buf, uevent_buf_len, false )


// parse a uevent, and use the information to fill in a device request.
// nlbuf must be a contiguous concatenation of null-terminated KEY=VALUE strings.
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
   bool have_minor = false;
   mode_t dev_mode = 0;
   int line_count = 0;
   bool not_param = false;      // if set to true, add as an OS-specific parameter to the vreq
   
   char* devpath = NULL;        // sysfs devpath 
   char* subsystem = NULL;      // sysfs subsystem 
   char* devname = (char*)VDEV_DEVICE_PATH_UNKNOWN;        // DEVNAME from uevent
   
   vdev_device_request_t reqtype = VDEV_DEVICE_INVALID;
   
   vdev_debug("%p: uevent buffer\n", vreq );
   vdev_linux_debug_uevent( nlbuf, buflen );
   
   // sanity check: if the first line is $action@$devpath, then skip it (since the information 
   // contained in the uevent will encode the very same bits of information)
   if( strchr(buf, '@') != NULL ) { 
         
      // advance to the next line
      offset += strlen(buf) + 1;
   }
   
   // get key/value pairs
   while( offset < buflen ) {
      
      line_count++;
      not_param = false;
      
      rc = vdev_keyvalue_next( buf + offset, &key, &value );
      
      if( rc < 0 ) {
         
         vdev_error("Invalid line %d (byte %d): '%s'\n", line_count, offset, buf + offset );
         vdev_linux_error_uevent( nlbuf, buflen );
         
         return -EINVAL;
      }
      
      offset += rc + 1;         // count the \0 at the end
      rc = 0;
      
      // is this the action to take?
      if( strcmp(key, "ACTION") == 0 ) {
         
         reqtype = vdev_linux_parse_device_request_type( value );
         
         if( reqtype == VDEV_DEVICE_INVALID ) {
            
            vdev_error("Invalid ACTION '%s'\n", value );
            vdev_linux_error_uevent( nlbuf, buflen );
            
            return -EINVAL;
         }
         
         vdev_device_request_set_type( vreq, reqtype );
         
         not_param = true;
      }
      
      // is this the sysfs device path?
      else if( strcmp(key, "DEVPATH") == 0 ) {
         
         devpath = value;
      }
      
      // is this the devname?
      else if( strcmp(key, "DEVNAME") == 0 ) {
         
         devname = value;
      }
      
      // subsystem given?
      else if( strcmp(key, "SUBSYSTEM") == 0 ) {
         
         subsystem = vdev_strdup_or_null( value );
      }
      
      // is this the major device number?
      else if( strcmp(key, "MAJOR") == 0 && !have_major ) {
         
         char* tmp = NULL;
         major = (int)strtol( value, &tmp, 10 );
         
         if( *tmp != '\0' ) {
            
            vdev_error("Invalid 'MAJOR' value '%s'\n", value);
            vdev_linux_error_uevent( nlbuf, buflen );
            
            return -EINVAL;
         }
         
         have_major = true;
         not_param = true;
      }
      
      // is this the minor device number?
      else if( strcmp(key, "MINOR") == 0 && !have_minor ) {
         
         char* tmp = NULL;
         minor = (int)strtol( value, &tmp, 10 ) ;
         
         if( *tmp != '\0' ) {
            
            vdev_error("Invalid 'MINOR' value '%s'\n", value );
            vdev_linux_error_uevent( nlbuf, buflen );
            
            return -EINVAL;
         }
         
         have_minor = true;
         not_param = true;
      }
      
      if( !not_param ) {
         
         // add to OS params 
         rc = vdev_device_request_add_param( vreq, key, value );
         if( rc != 0 ) {
            
            // could be OOM 
            if( subsystem != NULL ) {
               free( subsystem );
            }
            
            return rc;
         }
      }
   }
   
   if( reqtype == VDEV_DEVICE_INVALID ) {
      
      vdev_error("%s", "No ACTION given\n");
      vdev_linux_error_uevent( nlbuf, buflen );
      
      if( subsystem != NULL ) {
         free( subsystem );
      }
      
      return -EINVAL;
   }
   
   if( (!have_major && have_minor) || (have_major && !have_minor) ) {
      
      vdev_error("Missing device information: major=%d, minor=%d\n", have_major, have_minor );
      vdev_linux_error_uevent( nlbuf, buflen );
      
      if( subsystem != NULL ) {
         free( subsystem );
      }
      
      return -EINVAL;
   }
   
   if( have_major && have_minor ) {
      
      // explicit major and minor device numbers given 
      vdev_device_request_set_dev( vreq, makedev(major, minor) );
   }
   
   if( devname != NULL ) {
      
      // use this as the device's path 
      vdev_device_request_set_path( vreq, devname );
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
         else {
            
            // it's okay to not have dev numbers
            rc = 0;
         }
      }
      
      // subsystem?
      if( subsystem == NULL ) {
         
         // see if we have a subsystem 
         rc = vdev_linux_sysfs_read_subsystem( ctx, devpath, &subsystem );
         
         if( rc == 0 ) {
            
            // yup!
            rc = vdev_device_request_add_param( vreq, "SUBSYSTEM", subsystem );
            if( rc != 0 ) {
               
               // OOM
               free( subsystem );
               return rc;
            }
         }
         else if( rc != -ENOMEM ) {
            
            // this is weird...
            vdev_warn("no subsystem found for '%s'\n", devpath );
            rc = 0;
         }
      }
   }
   
   if( have_major && have_minor ) {
      
      if( subsystem != NULL && strcasecmp(subsystem, "block") == 0 ) {
         
         // this is a block 
         dev_mode = S_IFBLK;
      }
      
      else {
         
         // this is a character device--we have major/minor numbers
         dev_mode = S_IFCHR;
      }
   
      vdev_device_request_set_mode( vreq, dev_mode );
   }
   
   vdev_debug("subsystem = '%s', have_major=%d, major = %u, have_minor=%d, minor = %u, mode = %o\n", subsystem, have_major, major, have_minor, minor, dev_mode );
   
   if( subsystem != NULL ) {
      free( subsystem );
   }
   
   // tell helpers where /sys is mounted 
   rc = vdev_device_request_add_param( vreq, "SYSFS_MOUNTPOINT", ctx->sysfs_mountpoint );
   
   return rc;
}


// yield the next device event
// return 0 on success 
// return 1 if there are no more devices
// return -EAGAIN if vdev should try to get this device again 
// return -errno on failure to poll for devices or read the next device packet.
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
   
   pthread_mutex_lock( &ctx->initial_requests_lock );
   
   // do we have initial requests?
   if( ctx->initial_requests != NULL ) {
      
      // next request
      struct vdev_device_request* req = ctx->initial_requests;
      
      // consume 
      ctx->initial_requests = ctx->initial_requests->next;
      
      memcpy( vreq, req, sizeof(struct vdev_device_request) );
      free( req );
      
      pthread_mutex_unlock( &ctx->initial_requests_lock );
      
      // was that the last of them?
      if( ctx->initial_requests == NULL ) {
         
         // tell vdevd that we've flushed all pending requests 
         vdev_os_context_signal_flushed( ctx->os_ctx );
      }
      
      return 0;
   }
   else if( ctx->os_ctx->state->once ) {
      
      // out of requests; die 
      pthread_mutex_unlock( &ctx->initial_requests_lock );
      return 1;
   }
   else {
      
      pthread_mutex_unlock( &ctx->initial_requests_lock );
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
      
      vdev_error("FATAL: poll(%d) rc = %d\n", ctx->pfd.fd, rc );
      
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
      vdev_error("FATAL: recvmsg(%d) rc = %d\n", ctx->pfd.fd, rc );
      
      return rc;
   }
   
   // big enough?
   if( len < 32 || len >= VDEV_LINUX_NETLINK_BUF_MAX ) {
      
      vdev_error("Netlink message is %zd bytes; ignoring...\n", len );
      return -EAGAIN;
   }
   
   // control message, for credentials
   chdr = CMSG_FIRSTHDR( &hdr );
   if( chdr == NULL || chdr->cmsg_type != SCM_CREDENTIALS ) {
      
      vdev_error("%s", "Netlink message has no credentials\n");
      return -EAGAIN;
   }
   
   // get the credentials
   cred = (struct ucred *)CMSG_DATA(chdr);
   
   // if not root, ignore 
   if( cred->uid != 0 ) {
      
      vdev_error("Ignoring message from non-root ID %d\n", cred->uid );
      return -EAGAIN;
   }
   
   // if udev, ignore 
   if( memcmp( buf, VDEV_LINUX_NETLINK_UDEV_HEADER, VDEV_LINUX_NETLINK_UDEV_HEADER_LEN ) == 0 ) {
      
      // message from udev; ignore 
      vdev_warn("%s", "Ignoring libudev message\n");
      return -EAGAIN;
   }
   
   // kernel messages don't come from userspace 
   if( cnls.nl_pid > 0 ) {
      
      // from userspace???
      vdev_warn("Ignoring message from PID %d\n", (int)cnls.nl_pid );
      return -EAGAIN;
   }
   
   // parse the event buffer
   vdev_debug("%p from netlink\n", vreq );
   rc = vdev_linux_parse_request( ctx, vreq, buf, len );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_parse_request rc = %d\n", rc );
      
      return -EAGAIN;
   }
   
   return 0;
}


// find sysfs mountpoint in /proc/mounts
// this is apparently superfluous (it *should* be mounted at /sys), but you never know.
// mountpoint must be big enough (PATH_MAX will do)
// return 0 on success
// return -ENOSYS if sysfs is not mounted
// return -ENOMEM if the buffer isn't big enough 
// return -EINVAL if somehow we failed to parse a mount entry
// return negative for some other errors (like access permission failures, or /proc not mounted)
static int vdev_linux_find_sysfs_mountpoint( char* mountpoint, size_t mountpoint_len ) {
   
   FILE* f = NULL;
   int rc = 0;
   
   char mntbuf[4096];
   struct mntent ment_buf;
   struct mntent* ment_ptr = NULL;
   int ent_count = 1;
   bool found = false;
   
   f = fopen( "/proc/mounts", "r" );
   if( f == NULL ) {
      
      rc = -errno;
      fprintf(stderr, "Failed to open /proc/mounts, rc = %d\n", rc );
      return rc;
   }
   
   // scan for sysfs mount type
   while( 1 ) {
      
      ment_ptr = getmntent_r( f, &ment_buf, mntbuf, 4096 );
      if( ment_ptr == NULL ) {
         
         vdev_error("Failed on processing entry #%d of /proc/mounts\n", ent_count );
         rc = -EINVAL;
         break;
      }
      
      if( strcmp( ment_ptr->mnt_type, "sysfs" ) == 0 ) {
         
         // found!
         strncpy( mountpoint, ment_ptr->mnt_dir, mountpoint_len - 1 );
         found = true;
         rc = 0;
         break;
      }
   }
   
   fclose( f );
   
   if( rc == 0 && !found ) {
      fprintf(stderr, "Failed to find mounted sysfs filesystem\n");
      return -ENOSYS;
   }
   
   return rc;
}

// get a uevent from a uevent file 
// replace newlines with '\0', making the uevent look like it came from the netlink socket
// (i.e. so it can be parsed by vdev_linux_parse_request)
// return 0 on success
// return -ENOMEM on OOM
// return -errno on failure to stat or read
static int vdev_linux_sysfs_read_uevent( char const* fp_uevent, char** ret_uevent_buf, size_t* ret_uevent_len ) {
   
   int rc = 0;
   struct stat sb;
   char* uevent_buf = NULL;
   size_t uevent_buf_len = 0;
   size_t uevent_len = 0;
   
   // get uevent size  
   rc = stat( fp_uevent, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      
      vdev_error("stat('%s') rc = %d\n", fp_uevent, rc );
      
      return rc;
   }
   else {
      
      uevent_buf_len = sb.st_size;
   }
   
   // read the uevent
   if( fp_uevent != NULL ) {
      
      uevent_buf = VDEV_CALLOC( char, uevent_buf_len );
      if( uevent_buf == NULL ) {
         
         return -ENOMEM;
      }
      
      rc = vdev_read_file( fp_uevent, uevent_buf, uevent_buf_len );
      if( rc != 0 ) {
         
         // failed in this 
         vdev_error("vdev_read_file('%s') rc = %d\n", fp_uevent, rc );
         free( uevent_buf );
      }
      else {
         
         for( unsigned int i = 0; i < uevent_buf_len; i++ ) {
            
            if( uevent_buf[i] == '\n' ) {
               
               uevent_buf[i] = '\0';
            }
         }
         
         // NOTE: the stat size is an upper-bound.  Find the exact number of bytes.
         for( uevent_len = 0; uevent_len < uevent_buf_len; ) {
            
            if( *(uevent_buf + uevent_len) == '\0' ) {
               break;
            }
            
            uevent_len += strlen( uevent_buf + uevent_len ) + 1;
         }
          
         *ret_uevent_buf = uevent_buf;
         *ret_uevent_len = uevent_len;
      }
   }
   
   return rc;
}


// append a key/value pair to a uevent buffer
// return 0 on success
// return -ENOMEM on OOM
static int vdev_linux_uevent_append( char** ret_uevent_buf, size_t* ret_uevent_buf_len, char const* key, char const* value ) {
   
   char* tmp = NULL;
   char* uevent_buf = *ret_uevent_buf;
   size_t uevent_buf_len = *ret_uevent_buf_len;
   
   // add it to the uevent buffer, so we can parse it like a normal uevent
   tmp = (char*)realloc( uevent_buf, uevent_buf_len + 1 + strlen(key) + 1 + strlen(value) + 1 );
   if( tmp == NULL ) {
      
      return -ENOMEM;
   }
   
   uevent_buf = tmp;
   
   // add key
   memcpy( uevent_buf + uevent_buf_len, key, strlen(key) );
   uevent_buf_len += strlen(key);
   
   // add '='
   *(uevent_buf + uevent_buf_len) = '=';
   uevent_buf_len ++;
   
   // add value
   memcpy( uevent_buf + uevent_buf_len, value, strlen(value) );
   uevent_buf_len += strlen(value);
   
   // NULL-terminate 
   *(uevent_buf + uevent_buf_len) = '\0';
   uevent_buf_len ++;
   
   *ret_uevent_buf = uevent_buf;
   *ret_uevent_buf_len = uevent_buf_len;
   
   return 0;
}


// register a device from sysfs, given the path to its uevent file.
// that is, read the uevent file, generate a device request, and add it to the initial_requests list in the ctx.
// NOTE: it is assumed that fp_uevent--the full path to the uevent file--lives in /sys/devices
// return 0 on success
// return -ENOMEM on OOM
// return negative on error.
static int vdev_linux_sysfs_register_device( struct vdev_linux_context* ctx, char const* fp_uevent ) {
   
   int rc = 0;
   struct stat sb;
   char* uevent_buf = NULL;
   size_t uevent_buf_len = 0;
   char* full_devpath = NULL;
   char* devpath = NULL;
   char* devname = NULL;
   char* delim = NULL;
   
   // extract the devpath from the uevent path 
   full_devpath = vdev_dirname( fp_uevent, NULL );
   if( full_devpath == NULL ) {
      return -ENOMEM;
   }
   
   // get uevent
   rc = vdev_linux_sysfs_read_uevent( fp_uevent, &uevent_buf, &uevent_buf_len );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_sysfs_read_uevent('%s') rc = %d\n", fp_uevent, rc );
      
      free( full_devpath );
      return rc;
   }
   
   if( uevent_buf_len == 0 ) {
      
      // no messages to be had
      vdev_debug("Empty uevent file at '%s'\n", fp_uevent );
      
      free( full_devpath );
      free( uevent_buf );
      return 0;
   }
   
   // truncate to /devices
   devpath = full_devpath + strlen( ctx->sysfs_mountpoint );
   
   // we're adding this, so make ACTION=add
   rc = vdev_linux_uevent_append( &uevent_buf, &uevent_buf_len, "ACTION", "add" );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_uevent_append('%s=%s') rc = %d\n", "ACTION", "add", rc );
      
      free( uevent_buf );
      free( full_devpath );
      
      return rc;
   }
   
   
   // see if the uevent has a devname.  Use that over our devname, if need be 
   devname = strstr( uevent_buf, "DEVNAME=" );
   if( devname != NULL ) {
      
      // have a devname!
      devname += strlen("DEVNAME=") + 1;
      
      size_t devname_len = strcspn( devname, "\n\0" );
      if( devname_len > 0 ) {
         
         char* tmp = VDEV_CALLOC( char, devname_len + 1 );
         if( tmp == NULL ) {
            
            free( uevent_buf );
            free( full_devpath );
            
            return -ENOMEM;
         }
         
         strncpy( tmp, devname, devname_len );
         
         devname = tmp;
      }
   } 
   
   // include the device path
   rc = vdev_linux_uevent_append( &uevent_buf, &uevent_buf_len, "DEVPATH", devpath );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_uevent_append('%s=%s') rc = %d\n", "DEVPATH", devpath, rc );
      
      free( uevent_buf );
      free( full_devpath );
      free( devname );
      return rc;
   }
   
   
   // make the device request
   struct vdev_device_request* vreq = VDEV_CALLOC( struct vdev_device_request, 1 );
   if( vreq == NULL ) {
      
      free( full_devpath );
      free( uevent_buf );
      
      return -ENOMEM;
   }
   
   // build up the request
   vdev_device_request_init( vreq, ctx->os_ctx->state, VDEV_DEVICE_INVALID, devname );
   
   // parse from our uevent
   rc = vdev_linux_parse_request( ctx, vreq, uevent_buf, uevent_buf_len );
   
   free( uevent_buf );
   uevent_buf = NULL;
   
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_parse_request('%s') rc = %d\n", fp_uevent, rc );
      
      free( full_devpath );
      free( devname );
   
      return rc;
   }
   
   free( full_devpath );
   free( devname );
   
   pthread_mutex_lock( &ctx->initial_requests_lock );
   
   // append 
   if( ctx->initial_requests == NULL ) {
      
      ctx->initial_requests = vreq;
      ctx->initial_requests_tail = vreq;
   }
   else {
      
      ctx->initial_requests_tail->next = vreq;
      ctx->initial_requests_tail = vreq;
   }
   
   vreq->next = NULL;
   
   pthread_mutex_unlock( &ctx->initial_requests_lock );
   
   return rc;
}


// scan structure for finding new device directories, and signalling whether or not 
// the given directory has a uevent.
struct vdev_linux_sysfs_scan_context {
   
   char* uevent_path;
   struct sglib_cstr_vector* device_frontier;
};

// populate a /sys/devices directory with its child directories
// return 0 on success
// return -ENOMEM on OOM
// return -errno on failure to stat
static int vdev_linux_sysfs_scan_device_directory( char const* fp, void* cls ) {
   
   struct vdev_linux_sysfs_scan_context* scan_ctx = (struct vdev_linux_sysfs_scan_context*)cls;
   
   struct sglib_cstr_vector* device_frontier = scan_ctx->device_frontier;
   
   struct stat sb;
   int rc = 0;
   char* fp_base = NULL;
   char* fp_dup = NULL;
   
   fp_base = rindex( fp, '/' );
   
   if( fp_base == NULL ) {
      return 0;
   }
   
   // skip . and .. 
   if( strcmp( fp_base, "/." ) == 0 || strcmp( fp_base, "/.." ) == 0 ) {
      return 0;
   }
   
   // add directories
   rc = lstat( fp, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("lstat('%s') rc = %d\n", fp, rc );
      return rc;
   }
   
   if( !S_ISDIR( sb.st_mode ) && strcmp( fp_base, "/uevent" ) != 0 ) {
      
      // not a directory, and not a uevent
      return 0;
   }
   
   fp_dup = vdev_strdup_or_null( fp );
   if( fp_dup == NULL ) {
      
      return -ENOMEM;
   }
   
   if( S_ISDIR( sb.st_mode ) ) {
   
      vdev_debug("Search '%s'\n", fp_dup );
      
      rc = sglib_cstr_vector_push_back( device_frontier, fp_dup );
      if( rc != 0 ) {
      
         free( fp_dup );
         return rc;
      }
   }
   else {
      
      // this is a uevent; this directory is a device 
      vdev_debug("Uevent '%s'\n", fp_dup );
      scan_ctx->uevent_path = fp_dup;
   }
   
   return 0;
}


// read all devices from sysfs, and put their uevent paths into the given uevent_paths
// return 0 on success
// return negative on error
static int vdev_linux_sysfs_find_devices( struct vdev_linux_context* ctx, struct sglib_cstr_vector* uevent_paths ) {
   
   int rc = 0;
   
   // find all directories that have a 'uevent' file in them 
   struct sglib_cstr_vector device_frontier;
   sglib_cstr_vector_init( &device_frontier );
   
   struct vdev_linux_sysfs_scan_context scan_context;
   memset( &scan_context, 0, sizeof(struct vdev_linux_sysfs_scan_context) );
   
   scan_context.device_frontier = &device_frontier;
   
   char* device_root = NULL;
   
   // scan /sys/devices 
   device_root = vdev_fullpath( ctx->sysfs_mountpoint, "/devices", NULL );
   if( device_root == NULL ) {
      
      return -ENOMEM;
   }
   
   rc = vdev_load_all( device_root, vdev_linux_sysfs_scan_device_directory, &scan_context );
   if( rc != 0 ) {
      
      vdev_error("vdev_load_all('%s') rc = %d\n", device_root, rc );
      
      free( device_root );
      
      sglib_cstr_vector_free( &device_frontier );
      
      return rc;
   }
   
   free( device_root );
   
   while( 1 ) {
      
      unsigned long len = sglib_cstr_vector_size( &device_frontier );
      
      if( len == 0 ) {
         break;
      }
      
      device_root = sglib_cstr_vector_at( &device_frontier, len - 1 );
      sglib_cstr_vector_set( &device_frontier, NULL, len - 1 );
      
      sglib_cstr_vector_pop_back( &device_frontier );
      
      // scan for more devices 
      rc = vdev_load_all( device_root, vdev_linux_sysfs_scan_device_directory, &scan_context );
      if( rc != 0 ) {
         
         vdev_error("vdev_load_all('%s') rc = %d\n", device_root, rc );
         free( device_root );
         break;
      }
      
      // is one of them a uevent?
      if( scan_context.uevent_path != NULL ) {
         
         // yup--remember it
         char* uevent_path = vdev_strdup_or_null( scan_context.uevent_path );
         
         if( uevent_path == NULL ) {
            
            free( device_root );
            rc = -ENOMEM;
            break;
         }
         
         sglib_cstr_vector_push_back( uevent_paths, uevent_path );
         
         free( scan_context.uevent_path );
         scan_context.uevent_path = NULL;
      }
      
      free( device_root );
   }
   
   sglib_cstr_vector_free( &device_frontier );
   
   return rc;
}


// free a list of cstr vectors
// always succeeds
static int vdev_cstr_vector_free_all( struct sglib_cstr_vector* vec ) {
   
   // free all strings
   for( unsigned long i = 0; i < sglib_cstr_vector_size( vec ); i++ ) {
      
      if( sglib_cstr_vector_at( vec, i ) != NULL ) {
         
         free( sglib_cstr_vector_at( vec, i ) );
         sglib_cstr_vector_set( vec, NULL, i );
      }
   }
   
   return 0;
}
 
// register all devices, given a vector to their uevent files
// return 0 on success
// return negative on error
static int vdev_linux_sysfs_register_devices( struct vdev_linux_context* ctx ) {
   
   int rc = 0;
   struct sglib_cstr_vector uevent_paths;
   
   sglib_cstr_vector_init( &uevent_paths );
   
   // scan devices 
   rc = vdev_linux_sysfs_find_devices( ctx, &uevent_paths );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_sysfs_find_devices() rc = %d\n", rc );
      
      vdev_cstr_vector_free_all( &uevent_paths );
      sglib_cstr_vector_free( &uevent_paths );
      return rc;
   }
   
   // process all devices
   for( unsigned long i = 0; i < sglib_cstr_vector_size( &uevent_paths ); i++ ) {
      
      char* uevent_path = sglib_cstr_vector_at( &uevent_paths, i );
      
      // skip filtered entries
      if( uevent_path == NULL ) {
         continue;
      }
      
      vdev_debug("Register device '%s'\n", uevent_path );
      rc = vdev_linux_sysfs_register_device( ctx, uevent_path );
      if( rc != 0 ) {
         
         vdev_error("vdev_linux_sysfs_register_device('%s') rc = %d\n", uevent_path, rc );
         continue;
      }
   }
   
   // free all paths
   vdev_cstr_vector_free_all( &uevent_paths );
   sglib_cstr_vector_free( &uevent_paths );
   
   return rc;
}

// start listening for kernel events via netlink
// only do so if we're *NOT* going to run once (check the config)
// return 0 on success
// return -errno on error (failure to socket(2), setsockopt(2), bind(2), or walk sysfs)
static int vdev_linux_context_init( struct vdev_os_context* os_ctx, struct vdev_linux_context* ctx ) {
   
   int rc = 0;
   size_t slen = VDEV_LINUX_NETLINK_RECV_BUF_MAX;
   
   memset( ctx, 0, sizeof(struct vdev_linux_context) );
   
   ctx->os_ctx = os_ctx;
   
   // if we're just running once, don't set up the netlink socket 
   if( !os_ctx->state->config->once ) {
      
      ctx->nl_addr.nl_family = AF_NETLINK;
      ctx->nl_addr.nl_pid = getpid();
      ctx->nl_addr.nl_groups = NETLINK_KOBJECT_UEVENT;
      
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
   }
   else {
      
      ctx->pfd.fd = -1;
   }
   
   // lookup sysfs mountpoint
   rc = vdev_linux_find_sysfs_mountpoint( ctx->sysfs_mountpoint, PATH_MAX );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_find_sysfs_mountpoint rc = %d\n", rc );
      
      close( ctx->pfd.fd );
      return rc;
   }
   
   pthread_mutex_init( &ctx->initial_requests_lock, NULL );
   
   // seed devices from sysfs 
   rc = vdev_linux_sysfs_register_devices( ctx );
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
      
      if( ctx->pfd.fd >= 0 ) {
         close( ctx->pfd.fd );
         ctx->pfd.fd = -1;
      }
      
      if( ctx->initial_requests != NULL ) {
         
         struct vdev_device_request* itr = ctx->initial_requests;
         struct vdev_device_request* next = NULL;
         
         while( itr != NULL ) {
            
            next = itr->next;
            
            vdev_device_request_free( itr );
            free( itr );
            
            itr = next;
         }
         
         ctx->initial_requests = NULL;
         ctx->initial_requests_tail = NULL;
      }
   }
   
   return 0;
}


// are we using devtmpfs on this mountpoint?
// return 1 if so
// return 0 if not 
// return negative on error 
static int vdev_linux_mountpoint_on_devtmpfs( char const* mountpoint ) {

   // NOTE: You would think that we can just statfs(2) the 
   // mountpoint and get the underlying filesystem type.
   // However, devtmpfs and tmpfs have the same f_type value.
   // This makes this strategy a no-go, since vdevd will need
   // to behave differently on devtmpfs but not tmpfs.
   // So, we have to parse /proc/mounts instead.
   
   int rc = 0;
   
   FILE* f = NULL;
   
   char* line_buf = NULL;
   size_t line_len = 0;
   ssize_t num_read = 0;
   
   bool found = false;
   char devtmpfs_mountpoint[PATH_MAX+1];
   
   char realpath_mountpoint[PATH_MAX+1];
   char devtmpfs_realpath_mountpoint[PATH_MAX+1];
   
   size_t realpath_mountpoint_len = 0;
   size_t devtmpfs_realpath_mountpoint_len = 0;
   
   memset( devtmpfs_mountpoint, 0, PATH_MAX+1 );
   
   f = fopen( "/proc/mounts", "r" );
   if( f == NULL ) {
      
      rc = -errno;
      fprintf(stderr, "Failed to open /proc/mounts, rc = %d\n", rc );
      return rc;
   }
   
   // scan for devtmpfs 
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
      
      // devtmpfs?
      if( strstr( line_buf, "devtmpfs" ) != NULL ) {
         
         // mountpoint?
         char devtmpfs_buf[20];
         rc = sscanf( line_buf, "%s %s", devtmpfs_buf, devtmpfs_mountpoint );
         
         if( rc != 2 ) {
            
            // couldn't scan 
            vdev_error("WARN: sscanf(\"%s\") for devtmpfs mountpoint rc = %d\n", line_buf, rc );
            continue;
         }
         else {
            
            // is the mountpoint contained in this path?
            char* tmp = NULL;
            
            memset( realpath_mountpoint, 0, PATH_MAX+1 );
            memset( devtmpfs_realpath_mountpoint, 0, PATH_MAX+1 );
            
            tmp = realpath( mountpoint, realpath_mountpoint );
            if( tmp == NULL ) {
               
               rc = -errno;
               fprintf(stderr, "Failed to get the real path of '%s', rc = %d\n", mountpoint, rc );
               break;
            }
            
            tmp = realpath( devtmpfs_mountpoint, devtmpfs_realpath_mountpoint );
            if( tmp == NULL ) {
               
               rc = -errno;
               fprintf(stderr, "Failed to get the real path of '%s', rc = %d\n", devtmpfs_mountpoint, rc );
               break;
            }
            
            vdev_debug("devtmpfs realpath is   '%s'\n", devtmpfs_realpath_mountpoint );
            vdev_debug("mountpoint realpath is '%s'\n", realpath_mountpoint );
            
            devtmpfs_realpath_mountpoint_len = strlen(devtmpfs_realpath_mountpoint);
            realpath_mountpoint_len = strlen(realpath_mountpoint);
            
            size_t min_len = MIN( devtmpfs_realpath_mountpoint_len, realpath_mountpoint_len );
            
            // is our mountpoint within the devtmpfs mountpoint?
            if( strncmp( devtmpfs_realpath_mountpoint, realpath_mountpoint, min_len ) == 0 &&   // devtmpfs mountpoint path is a substring of the mountpoint path and
                (strchr( realpath_mountpoint + min_len, '/' ) != NULL ||                        // either the mountpoint path continues at least one directory into devtmpfs path, or
                 strcmp( devtmpfs_realpath_mountpoint, realpath_mountpoint ) == 0 ) ) {         // the devtmpfs mountpoint path *is* the mountpoint path
                
               // contained!
               rc = 1;
               found = true;
               break;
            }
         }
      }
   }
   
   fclose( f );
   
   if( line_buf != NULL ) {
      free( line_buf );
   }
   
   return rc;
}

// set up Linux-specific vdev state.
// this is early initialization, so don't start anything yet
int vdev_os_init( struct vdev_os_context* os_ctx, void** cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = NULL;
   
   rc = vdev_linux_mountpoint_on_devtmpfs( os_ctx->state->mountpoint );
   if( rc < 0 ) {
      
      vdev_error("vdev_linux_mountpoint_on_devtmpfs('%s') rc = %d\n", os_ctx->state->mountpoint, rc );
      return rc;
   }
   
   if( rc > 0 ) {
      
      // using devtmpfs 
      vdev_info("'%s' is on devtmpfs\n", os_ctx->state->mountpoint );
      
      vdev_config_set_OS_quirk( os_ctx->state->config->OS_quirks, VDEV_OS_QUIRK_DEVICE_EXISTS );
   }
   else {
      
      vdev_info("'%s' is not on devtmpfs\n", os_ctx->state->mountpoint );
   }
   
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