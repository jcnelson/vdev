/*
   vdev: a FUSE filesystem for accessing privileged device nodes
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

#include "vdev.h"

// get running state
struct vdev_state* vdev_get_state() {
   return (struct vdev_state*)fuse_get_context()->private_data;
}

// get caller UID 
uint64_t vdev_get_uid() {
   return fuse_get_context()->uid;
}

// get caller GID 
uint64_t vdev_get_gid() {
   return fuse_get_context()->gid;
}

// get caller PID 
pid_t vdev_get_pid() {
   return fuse_get_context()->pid;
}

// get caller umask 
mode_t vdev_get_umask() {
   return fuse_get_context()->umask;
}

// make a FUSE file info for a file handle
struct vdev_file_info* vdev_make_file_handle( struct fskit_file_handle* fh ) {
   
   struct vdev_file_info* ffi = CALLOC_LIST( struct vdev_file_info, 1 );
   if( ffi == NULL ) {
      return NULL;
   }
   
   ffi->type = FSKIT_ENTRY_TYPE_FILE;
   ffi->handle.fh = fh;
   
   return ffi;
}

// make a FUSE file info for a dir handle
struct vdev_file_info* vdev_make_dir_handle( struct fskit_dir_handle* dh ) {
   
   struct vdev_file_info* ffi = CALLOC_LIST( struct vdev_file_info, 1 );
   if( ffi == NULL ) {
      return NULL;
   }
   
   ffi->type = FSKIT_ENTRY_TYPE_DIR;
   ffi->handle.dh = dh;
   
   return ffi;
}

int vdev_getattr(const char *path, struct stat *statbuf) {
   
   dbprintf("getattr(%s, %p)\n", path, statbuf );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_stat( state->core, path, uid, gid, statbuf );
   
   dbprintf("getattr(%s, %p) rc = %d\n", path, statbuf, rc );
   return rc;
}


int vdev_readlink(const char *path, char *link, size_t size) {
   
   // not supported by fskit, so not supported by vdev
   return -ENOSYS;
}


int vdev_mknod(const char *path, mode_t mode, dev_t dev) {
   
   dbprintf("mknod(%s, %o, %d, %d)\n", path, mode, major(dev), minor(dev) );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_mknod( state->core, path, mode, dev, uid, gid );
   
   dbprintf("mknod(%s, %o, %d, %d) rc = %d\n", path, mode, major(dev), minor(dev), rc );
   
   return rc;
}

int vdev_mkdir(const char *path, mode_t mode) {
   
   dbprintf("mkdir(%s, %o)\n", path, mode );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_mkdir( state->core, path, mode, uid, gid );
   
   dbprintf("mkdir(%s, %o) rc = %d\n", path, mode, rc );
   
   return rc;
}

int vdev_unlink(const char *path) {
   
   dbprintf("unlink(%s)\n", path );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_unlink( state->core, path, uid, gid );
   
   dbprintf("unlink(%s) rc = %d\n", path, rc );
   
   return rc;
}

int vdev_rmdir(const char *path) {
   
   dbprintf("rmdir(%s)\n", path );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_rmdir( state->core, path, uid, gid );
   
   dbprintf("rmdir(%s) rc = %d\n", path, rc );
   
   return rc;
}

int vdev_symlink(const char *path, const char *link) {
   
   dbprintf("symlink(%s, %s)\n", path, link );
   
   // not supported by fskit
   dbprintf("symlink(%s, %s) rc = %d\n", path, link, -ENOSYS );
   return -ENOSYS;
}

int vdev_rename(const char *path, const char *newpath) {
   
   dbprintf("rename(%s, %s)\n", path, newpath );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_rename( state->core, path, newpath, uid, gid );
   
   dbprintf("rename(%s, %s) rc = %d\n", path, newpath, rc );
   
   return rc;
}

int vdev_link(const char *path, const char *newpath) {
   
   // not supported by fskit, so not supported by vdev
   return -ENOSYS;
}

int vdev_chmod(const char *path, mode_t mode) {
   
   dbprintf("chmod(%s, %o)\n", path, mode );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_chmod( state->core, path, uid, gid, mode );
   
   dbprintf("chmod(%s, %o) rc = %d\n", path, mode, rc );
   
   return rc;
}

int vdev_chown(const char *path, uid_t new_uid, gid_t new_gid) {
   
   dbprintf("chown(%s, %d, %d)\n", path, new_uid, new_gid );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_chown( state->core, path, uid, gid, new_uid, new_gid );
   
   dbprintf("chown(%s, %d, %d) rc = %d\n", path, new_uid, new_gid, rc );
   
   return rc;
}

int vdev_truncate(const char *path, off_t newsize) {
   
   dbprintf("truncate(%s, %jd)\n", path, newsize );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_trunc( state->core, path, uid, gid, newsize );
   
   dbprintf("truncate(%s, %jd) rc = %d\n", path, newsize, rc );
   
   return rc;
}

int vdev_utime(const char *path, struct utimbuf *ubuf) {
   
   dbprintf("utime(%s, %ld.%ld)\n", path, ubuf->actime, ubuf->modtime );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_utime( state->core, path, uid, gid, ubuf );
   
   dbprintf("utime(%s, %ld.%ld) rc = %d\n", path, ubuf->actime, ubuf->modtime, rc );
   
   return rc;
}

int vdev_open(const char *path, struct fuse_file_info *fi) {
   
   // vdev does not support regular files 
   return -ENOSYS;
}

int vdev_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   
   // vdev does not support regular files 
   return -ENOSYS;
}

int vdev_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   
   // vdev does not support regular files 
   return -ENOSYS;
}

int vdev_statfs(const char *path, struct statvfs *statv) {
   
   dbprintf("statfs(%s, %p)\n", path, statv );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_statvfs( state->core, path, uid, gid, statv );
   
   dbprintf("statfs(%s, %p) rc = %d\n", path, statv, rc );
   
   return rc;
}

int vdev_flush(const char *path, struct fuse_file_info *fi) {
   
   // vdev does not support regular files 
   return 0;
}

int vdev_release(const char *path, struct fuse_file_info *fi) {
   
   // vdev does not support regular files 
   return 0;
}

int vdev_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
   
   // vdev does not support regular files 
   return 0;
}

int vdev_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
   
   dbprintf("setxattr(%s, %s, %p, %zu, %X)\n", path, name, value, size, flags );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_setxattr( state->core, path, uid, gid, name, value, size, flags );
   
   dbprintf("setxattr(%s, %s, %p, %zu, %X) rc = %d\n", path, name, value, size, flags, rc );
   return rc;
}

int vdev_getxattr(const char *path, const char *name, char *value, size_t size) {
   
   dbprintf("getxattr(%s, %s, %p, %zu)\n", path, name, value, size );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_getxattr( state->core, path, uid, gid, name, value, size );
   
   dbprintf("getxattr(%s, %s, %p, %zu) rc = %d\n", path, name, value, size, rc );
   return rc;
}

int vdev_listxattr(const char *path, char *list, size_t size) {
   
   dbprintf("listxattr(%s, %p, %zu)\n", path, list, size );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_listxattr( state->core, path, uid, gid, list, size );
   
   dbprintf("listxattr(%s, %p, %zu) rc = %d\n", path, list, size, rc );
   return rc;
}

int vdev_removexattr(const char *path, const char *name) {
   
   dbprintf("removexattr(%s, %s)\n", path, name );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_removexattr( state->core, path, uid, gid, name );
   
   dbprintf("removexattr(%s, %s) rc = %d\n", path, name, rc );
   return rc;
}

int vdev_opendir(const char *path, struct fuse_file_info *fi) {
   
   dbprintf("opendir(%s, %p)\n", path, fi );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   struct vdev_file_info* ffi = NULL;
   int rc = 0;
   
   struct fskit_dir_handle* dh = fskit_opendir( state->core, path, uid, gid, &rc );
   
   if( rc != 0 ) {
      
      dbprintf("opendir(%s, %p) rc = %d\n", path, fi, rc );
      return rc;
   }
   
   ffi = vdev_make_dir_handle( dh );
   
   if( ffi == NULL ) {
      fskit_closedir( state->core, dh );
      
      dbprintf("opendir(%s, %p) rc = %d\n", path, fi, -ENOMEM );
      return -ENOMEM;
   }
   
   fi->fh = (uint64_t)ffi; 
   
   dbprintf("opendir(%s, %p) rc = %d\n", path, fi, 0 );
   return 0;
}

int vdev_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
   
   dbprintf("readdir(%s, %jd, %p, %p)\n", path, offset, buf, fi );
   
   struct vdev_state* state = vdev_get_state();
   struct fskit_dir_handle* fdh = NULL;
   struct vdev_file_info* ffi = NULL;
   int rc = 0;
   uint64_t num_read = 0;
   
   ffi = (struct vdev_file_info*)fi->fh;
   fdh = ffi->handle.dh;
   
   struct fskit_dir_entry** dirents = fskit_listdir( state->core, fdh, &num_read, &rc );
   
   if( dirents == NULL || rc != 0 ) {
      dbprintf("readdir(%s, %jd, %p, %p) rc = %d\n", path, offset, buf, fi, rc );
      return rc;
   }
   
   for( uint64_t i = 0; i < num_read; i++ ) {
      
      rc = filler( buf, dirents[i]->name, NULL, 0 );
      if( rc != 0 ) {
         rc = -ENOMEM;
         break;
      }
   }
   
   fskit_dir_entry_free_list( dirents );
   
   dbprintf("readdir(%s, %jd, %p, %p) rc = %d\n", path, offset, buf, fi, rc );
   return rc;
}

int vdev_releasedir(const char *path, struct fuse_file_info *fi) {
   
   dbprintf("releasedir(%s, %p)\n", path, fi );
   
   struct vdev_state* state = vdev_get_state();
   struct vdev_file_info* ffi = NULL;
   int rc = 0;
   
   ffi = (struct vdev_file_info*)fi->fh;
   
   rc = fskit_closedir( state->core, ffi->handle.dh );
   
   if( rc == 0 ) {
      safe_free( ffi );
   }
   
   dbprintf("releasedir(%s, %p) rc = %d\n", path, fi, rc );
   
   return rc;
}

int vdev_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
   
   // vdev does not support fsyncdir
   return 0;
}

int vdev_access(const char *path, int mask) {
   
   dbprintf("access(%s, %X)\n", path, mask );
   
   struct vdev_state* state = vdev_get_state();
   uint64_t uid = vdev_get_uid();
   uint64_t gid = vdev_get_gid();
   
   int rc = fskit_access( state->core, path, uid, gid, mask );
   
   dbprintf("access(%s, %X) rc = %d\n", path, mask, rc );
   
   return rc;
}

int vdev_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
   
   // vdev does not support regular files 
   return -ENOSYS;
}

int vdev_ftruncate(const char *path, off_t new_size, struct fuse_file_info *fi) {
   
   // vdev does not support regular files 
   return -ENOSYS;
}

int vdev_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi) {
   
   dbprintf("fgetattr(%s, %p, %p)\n", path, statbuf, fi );
   
   struct vdev_state* state = vdev_get_state();
   struct vdev_file_info* ffi = NULL;
   
   ffi = (struct vdev_file_info*)fi->fh;
   
   int rc = 0;
   
   if( ffi->type == FSKIT_ENTRY_TYPE_FILE ) {
      rc = fskit_fstat( state->core, ffi->handle.fh->fent, statbuf );
   }
   else {
      rc = fskit_fstat( state->core, ffi->handle.dh->dent, statbuf );
   }
   
   dbprintf("fgetattr(%s, %p, %p) rc = %d\n", path, statbuf, fi, rc );
   
   return rc;
}

void *vdev_fuse_init(struct fuse_conn_info *conn) {
   return vdev_get_state();
}

void vdev_destroy(void *userdata) {
   return;
}

struct fuse_operations vdev_get_opers() {
   struct fuse_operations fo;
   memset(&fo, 0, sizeof(fo));

   fo.getattr = vdev_getattr;
   fo.readlink = vdev_readlink;
   fo.mknod = vdev_mknod;
   fo.mkdir = vdev_mkdir;
   fo.unlink = vdev_unlink;
   fo.rmdir = vdev_rmdir;
   fo.symlink = vdev_symlink;
   fo.rename = vdev_rename;
   fo.link = vdev_link;
   fo.chmod = vdev_chmod;
   fo.chown = vdev_chown;
   fo.truncate = vdev_truncate;
   fo.utime = vdev_utime;
   fo.open = vdev_open;
   fo.read = vdev_read;
   fo.write = vdev_write;
   fo.statfs = vdev_statfs;
   fo.flush = vdev_flush;
   fo.release = vdev_release;
   fo.fsync = vdev_fsync;
   fo.setxattr = vdev_setxattr;
   fo.getxattr = vdev_getxattr;
   fo.listxattr = vdev_listxattr;
   fo.removexattr = vdev_removexattr;
   fo.opendir = vdev_opendir;
   fo.readdir = vdev_readdir;
   fo.releasedir = vdev_releasedir;
   fo.fsyncdir = vdev_fsyncdir;
   fo.init = vdev_fuse_init;
   fo.access = vdev_access;
   fo.create = vdev_create;
   fo.ftruncate = vdev_ftruncate;
   fo.fgetattr = vdev_fgetattr;

   return fo;
}
