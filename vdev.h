/*
   vdev: a FUSE filesystem to allow unprivileged users to access privileged files on UNIX-like systems.
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

#ifndef _VDEV_H_
#define _VDEV_H_

#include "debug.h"

#define FUSE_USE_VERSION 28
#include <fuse.h>

#include <map>
#include <string>
#include <set>

using namespace std;

struct vdev_state {
   
};

extern "C" {
   
// init/shutdown 
int vdev_init( struct vdev_state* state );
int vdev_start( struct vdev_state* state );
int vdev_stop( struct vdev_state* state );
int vdev_free( struct vdev_state* state );

// access
struct vdev_state* vdev_get_state();
   
// fs methods
int vdev_getattr(const char *path, struct stat *statbuf);
int vdev_readlink(const char *path, char *link, size_t size);
int vdev_mknod(const char *path, mode_t mode, dev_t dev);
int vdev_mkdir(const char *path, mode_t mode);
int vdev_unlink(const char *path);
int vdev_rmdir(const char *path);
int vdev_symlink(const char *path, const char *link);
int vdev_rename(const char *path, const char *newpath);
int vdev_link(const char *path, const char *newpath);
int vdev_chmod(const char *path, mode_t mode);
int vdev_chown(const char *path, uid_t uid, gid_t gid);
int vdev_truncate(const char *path, off_t newsize);
int vdev_utime(const char *path, struct utimbuf *ubuf);
int vdev_open(const char *path, struct fuse_file_info *fi);
int vdev_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int vdev_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int vdev_statfs(const char *path, struct statvfs *statv);
int vdev_flush(const char *path, struct fuse_file_info *fi);
int vdev_release(const char *path, struct fuse_file_info *fi);
int vdev_fsync(const char *path, int datasync, struct fuse_file_info *fi);
int vdev_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int vdev_getxattr(const char *path, const char *name, char *value, size_t size);
int vdev_listxattr(const char *path, char *list, size_t size);
int vdev_removexattr(const char *path, const char *name);
int vdev_opendir(const char *path, struct fuse_file_info *fi);
int vdev_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int vdev_releasedir(const char *path, struct fuse_file_info *fi);
int vdev_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi);
int vdev_access(const char *path, int mask);
int vdev_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int vdev_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi);
int vdev_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi);
void *vdev_fuse_init(struct fuse_conn_info *conn);
void vdev_destroy(void *userdata);

struct fuse_operations vdev_get_opers();
   
}

#endif
