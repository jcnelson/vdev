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

#ifdef _VDEV_OS_LINUX

#include "linux.h"

#include "workqueue.h"

// parse a uevent action 
vdev_device_request_t vdev_linux_parse_device_request_type( char const* type ) {
   
   if( strcmp(type, "add") == 0 ) {
      return VDEV_DEVICE_ADD;
   }
   else if( strcmp(type, "remove") == 0 ) {
      return VDEV_DEVICE_REMOVE;
   }
   else if( strcmp(type, "change") == 0 ) {
      return VDEV_DEVICE_CHANGE;
   }
   else if( strcmp(type, "move") == 0 ) {
      return VDEV_DEVICE_MOVE;
   }
   else if( strcmp(type, "online") == 0 ) {
      return VDEV_DEVICE_ONLINE;
   }
   else if( strcmp(type, "offline") == 0 ) {
      return VDEV_DEVICE_OFFLINE;
   }
   
   return VDEV_DEVICE_INVALID;
}


// parse a uevent, and use the information to fill in a device request.
// NOTE: This *modifies* buf
// return 0 on success
static int vdev_linux_parse_request( struct vdev_device_request* vreq, char* nlbuf, ssize_t buflen ) {
   
   
   char* buf = nlbuf;
   char* key = NULL;
   char* value = NULL;
   int offset = 0;
   int rc = 0;
   
   vdev_device_request_t reqtype = VDEV_DEVICE_INVALID;
   
   // sanity check: first line is $action@$devpath
   if( strchr(buf, '@') == NULL ) { 
      
      return -EINVAL;
   }
   
   // advance to the next line
   offset += strlen(buf) + 1;
   buf += strlen(buf) + 1;
   
   // get key/value pairs
   while( offset < buflen ) {
      
      // sanity check: must have '='
      key = buf;
      value = strchr(buf, '=');
      offset += strlen(buf);
      
      if( value == NULL ) {
         // invalid line 
         vdev_error("Invalid line '%s'\n", buf);
         
         return -EINVAL;
      }
      
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
      }
   }
   
   if( reqtype == VDEV_DEVICE_INVALID ) {
      
      vdev_error("%s", "No ACTION given\n");
      
      return -EINVAL;
   }
   
   vdev_device_request_set_type( vreq, reqtype );
   
   return 0;
}


// uevent listening thread 
static void* vdev_linux_uevent_thread_main( void* cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   char buf[VDEV_LINUX_NETLINK_BUF_MAX];
   ssize_t len = 0;
   
   while( ctx->running ) {
      
      // next event (wait forever)
      // NOTE: this is a cancellation point!
      rc = poll( &ctx->pfd, 1, -1 );
      
      if( rc != 0 ) {
         
         rc = -errno;
         
         if( rc == -EINTR ) {
            // try again 
            continue;
         }
         
         vdev_error("poll(%d) rc = %d\n", ctx->pfd.fd, rc );
         
         break;
      }
      
      // get the event 
      len = recv( ctx->pfd.fd, buf, VDEV_LINUX_NETLINK_BUF_MAX, MSG_DONTWAIT );
      
      if( len < 0 ) {
         
         rc = -errno;
         vdev_error("recv(%d) rc = %d\n", ctx->pfd.fd, rc );
         
         break;
      }
      
      // make a device request
      struct vdev_device_request* vreq = VDEV_CALLOC( struct vdev_device_request, 1 );
      
      if( vreq == NULL ) {
         // OOM
         break;
      }
      
      rc = vdev_device_request_init( vreq, VDEV_DEVICE_INVALID );
      if( rc != 0 ) {
         
         free( vreq );
         
         vdev_error("vdev_device_request_init rc = %d\n", rc );
         break;
      }
      
      // parse the event buffer
      rc = vdev_linux_parse_request( vreq, buf, len );
      
      if( rc != 0 ) {
         
         vdev_device_request_free( vreq );
         free( vreq );
         
         vdev_error("vdev_linux_parse_request rc = %d\n", rc );
         
         continue;
      }
      
      // post the event to the device work queue
      rc = vdev_device_request_add( ctx->state->device_wq, vreq );
      
      if( rc != 0 ) {
         
         vdev_device_request_free( vreq );
         free( vreq );
         
         vdev_error("vdev_device_request_add rc = %d\n", rc );
         
         continue;
      }
   }
   
   return NULL;
}


// find sysfs mountpoint in /proc/mounts
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
      vdev_error("Failed to open /proc/mounts, rc = %d\n", rc );
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
            vdev_error("getline(/proc/mounts) rc = %d\n", rc );
            break;
         }
      }
      
      // sysfs?
      if( strcmp( line_buf, "sysfs" ) == 0 ) {
         
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
      return -ENOSYS;
   }
   
   return rc;
}


// seed device-add requests from sysfs
// walk through sysfs (if mounted), and write to every device's uevent to get the kernel to send the "add" message
static int vdev_linux_seed_requests_from_sysfs( struct vdev_linux_context* ctx ) {
   
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
static int vdev_linux_context_init( struct vdev_state* state, struct vdev_linux_context* ctx ) {
   
   int rc = 0;
   pthread_attr_t attrs;
   
   memset( &attrs, 0, sizeof(pthread_attr_t) );
   memset( &ctx, 0, sizeof(struct vdev_linux_context) );
   
   ctx->state = state;
   
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
   
   rc = bind( ctx->pfd.fd, (struct sockaddr*)&ctx->nl_addr, sizeof(struct sockaddr_nl) );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("bind(%d) rc = %d\n", ctx->pfd.fd, rc );
      
      close( ctx->pfd.fd );
      return rc;
   }
   
   rc = vdev_linux_find_sysfs_mountpoint( ctx->sysfs_mountpoint, PATH_MAX );
   if( rc != 0 ) {
      
      vdev_error("vdev_linux_find_sysfs_mountpoint rc = %d\n", rc );
      
      close( ctx->pfd.fd );
      return rc;
   }
      
   ctx->running = true;
   rc = pthread_create( &ctx->thread, &attrs, vdev_linux_uevent_thread_main, ctx );
   
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("pthread_create rc = %d\n", rc );
      
      close( ctx->pfd.fd );
      return rc;
   }
   
   return 0;
}


// stop listening 
static int vdev_linux_context_shutdown( struct vdev_linux_context* ctx ) {
   
   // cancel and join with the listener
   ctx->running = false;
   pthread_cancel( ctx->thread );
   pthread_join( ctx->thread, NULL );
   
   // shut down 
   close( ctx->pfd.fd );
   
   return 0;
}


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
   fskit_fullpath( ctx->state->config->firmware_dir, firmware_name, fw_path );
   
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


// set up Linux-specific vdev state
int vdev_os_init( struct vdev_state* state, void** cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = NULL;
   
   ctx = VDEV_CALLOC( struct vdev_linux_context, 1 );
   if( ctx == NULL ) {
      return -ENOMEM;
   }
   
   rc = vdev_linux_context_init( state, ctx );
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


// add a device
// return 0 on success
// return negative on error 
int vdev_os_add_device( struct vdev_device_request* request, void* cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   
   return rc;
}

// remove a device 
// return 0 on success
// return negative on error 
int vdev_os_remove_device( struct vdev_device_request* request, void* cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   
   return rc;
}


// change a device 
// return 0 on success
// return negative on error 
int vdev_os_change_device( struct vdev_device_request* request, void* cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   
   return rc;
}


// move a device 
// return 0 on success
// return negative on error 
int vdev_os_move_device( struct vdev_device_request* request, void* cls )  {
   
   int rc = 0;
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   
   return rc;
}

// put a device online
// return 0 on success
// return negative on error 
int vdev_os_online_device( struct vdev_device_request* request, void* cls )  {
   
   int rc = 0;
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   
   return rc;
}

// put a device offline 
// return 0 on success
// return negative on error 
int vdev_os_offline_device( struct vdev_device_request* request, void* cls ) {
   
   int rc = 0;
   struct vdev_linux_context* ctx = (struct vdev_linux_context*)cls;
   
   return rc;
}

#endif