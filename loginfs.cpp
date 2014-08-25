/*
   License goes here
*/

#include "loginfs.h"

// get running state
struct loginfs_state* loginfs_get_state() {
   return (struct loginfs_state*)fuse_get_context()->private_data;
}

int loginfs_getattr(const char *path, struct stat *statbuf) {
   return 0;
}


int loginfs_readlink(const char *path, char *link, size_t size) {
   return 0;
}


int loginfs_mknod(const char *path, mode_t mode, dev_t dev) {
   return 0;
}

int loginfs_mkdir(const char *path, mode_t mode) {
   return 0;
}

int loginfs_unlink(const char *path) {
   return 0;
}

int loginfs_rmdir(const char *path) {
   return 0;
}

int loginfs_symlink(const char *path, const char *link) {
   return 0;
}

int loginfs_rename(const char *path, const char *newpath) {
   return 0;
}

int loginfs_link(const char *path, const char *newpath) {
   return 0;
}

int loginfs_chmod(const char *path, mode_t mode) {
   return 0;
}

int loginfs_chown(const char *path, uid_t uid, gid_t gid) {
   return 0;
}

int loginfs_truncate(const char *path, off_t newsize) {
   return 0;
}

int loginfs_utime(const char *path, struct utimbuf *ubuf) {
   return 0;
}

int loginfs_open(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_statfs(const char *path, struct statvfs *statv) {
   return 0;
}

int loginfs_flush(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_release(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
   return 0;
}

int loginfs_getxattr(const char *path, const char *name, char *value, size_t size) {
   return 0;
}

int loginfs_listxattr(const char *path, char *list, size_t size) {
   return 0;
}

int loginfs_removexattr(const char *path, const char *name) {
   return 0;
}

int loginfs_opendir(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_releasedir(const char *path, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_access(const char *path, int mask) {
   return 0;
}

int loginfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
   return 0;
}

int loginfs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi) {
   return 0;
}

void *loginfs_init(struct fuse_conn_info *conn) {
   return loginfs_get_state();
}

void loginfs_destroy(void *userdata) {
   return;
}

struct fuse_operations loginfs_get_opers() {
   struct fuse_operations fo;
   memset(&fo, 0, sizeof(fo));

   fo.getattr = loginfs_getattr;
   fo.readlink = loginfs_readlink;
   fo.mknod = loginfs_mknod;
   fo.mkdir = loginfs_mkdir;
   fo.unlink = loginfs_unlink;
   fo.rmdir = loginfs_rmdir;
   fo.symlink = loginfs_symlink;
   fo.rename = loginfs_rename;
   fo.link = loginfs_link;
   fo.chmod = loginfs_chmod;
   fo.chown = loginfs_chown;
   fo.truncate = loginfs_truncate;
   fo.utime = loginfs_utime;
   fo.open = loginfs_open;
   fo.read = loginfs_read;
   fo.write = loginfs_write;
   fo.statfs = loginfs_statfs;
   fo.flush = loginfs_flush;
   fo.release = loginfs_release;
   fo.fsync = loginfs_fsync;
   fo.setxattr = loginfs_setxattr;
   fo.getxattr = loginfs_getxattr;
   fo.listxattr = loginfs_listxattr;
   fo.removexattr = loginfs_removexattr;
   fo.opendir = loginfs_opendir;
   fo.readdir = loginfs_readdir;
   fo.releasedir = loginfs_releasedir;
   fo.fsyncdir = loginfs_fsyncdir;
   fo.init = loginfs_init;
   fo.access = loginfs_access;
   fo.create = loginfs_create;
   fo.ftruncate = loginfs_ftruncate;
   fo.fgetattr = loginfs_fgetattr;

   return fo;
}

}