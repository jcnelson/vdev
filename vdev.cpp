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

#include "vdev.h"

// get running state
struct vdev_state* vdev_get_state() {
   return (struct vdev_state*)fuse_get_context()->private_data;
}

int vdev_getattr(const char *path, struct stat *statbuf) {
   return 0;
}


int vdev_readlink(const char *path, char *link, size_t size) {
   return 0;
}


int vdev_mknod(const char *path, mode_t mode, dev_t dev) {
   return 0;
}

int vdev_mkdir(const char *path, mode_t mode) {
   return 0;
}

int vdev_unlink(const char *path) {
   return 0;
}

int vdev_rmdir(const char *path) {
   return 0;
}

int vdev_symlink(const char *path, const char *link) {
   return 0;
}

int vdev_rename(const char *path, const char *newpath) {
   return 0;
}

int vdev_link(const char *path, const char *newpath) {
   return 0;
}

int vdev_chmod(const char *path, mode_t mode) {
   return 0;
}

int vdev_chown(const char *path, uid_t uid, gid_t gid) {
   return 0;
}

int vdev_truncate(const char *path, off_t newsize) {
   return 0;
}

int vdev_utime(const char *path, struct utimbuf *ubuf) {
   return 0;
}

int vdev_open(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int vdev_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   return 0;
}

int vdev_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   return 0;
}

int vdev_statfs(const char *path, struct statvfs *statv) {
   return 0;
}

int vdev_flush(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int vdev_release(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int vdev_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
   return 0;
}

int vdev_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
   return 0;
}

int vdev_getxattr(const char *path, const char *name, char *value, size_t size) {
   return 0;
}

int vdev_listxattr(const char *path, char *list, size_t size) {
   return 0;
}

int vdev_removexattr(const char *path, const char *name) {
   return 0;
}

int vdev_opendir(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int vdev_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
   return 0;
}

int vdev_releasedir(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int vdev_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
   return 0;
}

int vdev_access(const char *path, int mask) {
   return 0;
}

int vdev_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
   return 0;
}

int vdev_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
   return 0;
}

int vdev_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi) {
   return 0;
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
