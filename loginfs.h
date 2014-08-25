/*
   License goes here
*/

#ifndef _LOGINFS_H_
#define _LOGINFS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <fuse.h>

#include <map>
#include <string>
#include <set>

using namespace std;

struct loginfs_state {
   
};

extern "C" {
   
// init/shutdown 
int loginfs_init( struct loginfs_state* state );
int loginfs_start( struct loginfs_state* state );
int loginfs_stop( struct loginfs_state* state );
int loginfs_free( struct loginfs_state* state );

// access
struct loginfs_state* loginfs_get_state();
   
// fs methods
int loginfs_getattr(const char *path, struct stat *statbuf);
int loginfs_readlink(const char *path, char *link, size_t size);
int loginfs_mknod(const char *path, mode_t mode, dev_t dev);
int loginfs_mkdir(const char *path, mode_t mode);
int loginfs_unlink(const char *path);
int loginfs_rmdir(const char *path);
int loginfs_symlink(const char *path, const char *link);
int loginfs_rename(const char *path, const char *newpath);
int loginfs_link(const char *path, const char *newpath);
int loginfs_chmod(const char *path, mode_t mode);
int loginfs_chown(const char *path, uid_t uid, gid_t gid);
int loginfs_truncate(const char *path, off_t newsize);
int loginfs_utime(const char *path, struct utimbuf *ubuf);
int loginfs_open(const char *path, struct fuse_file_info *fi);
int loginfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int loginfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int loginfs_statfs(const char *path, struct statvfs *statv);
int loginfs_flush(const char *path, struct fuse_file_info *fi);
int loginfs_release(const char *path, struct fuse_file_info *fi);
int loginfs_fsync(const char *path, int datasync, struct fuse_file_info *fi);
int loginfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int loginfs_getxattr(const char *path, const char *name, char *value, size_t size);
int loginfs_listxattr(const char *path, char *list, size_t size);
int loginfs_removexattr(const char *path, const char *name);
int loginfs_opendir(const char *path, struct fuse_file_info *fi);
int loginfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int loginfs_releasedir(const char *path, struct fuse_file_info *fi);
int loginfs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi);
int loginfs_access(const char *path, int mask);
int loginfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int loginfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi);
int loginfs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi);
void *loginfs_init(struct fuse_conn_info *conn);
void loginfs_destroy(void *userdata);

struct fuse_operations loginfs_get_opers();
   
}

#endif