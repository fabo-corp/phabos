#ifndef __RAMFS_H__
#define __RAMFS_H__

struct ramfs_inode;

int ramfs_getdents(struct file *file, struct phabos_dirent *dirp, size_t count);
ssize_t ramfs_write(struct file *file, const void *buf, size_t count);
ssize_t ramfs_read(struct file *file, void *buf, size_t count);
int ramfs_mount(struct inode *cwd);
int ramfs_mkdir(struct inode *cwd, const char *name, mode_t mode);
int ramfs_mknod(struct inode *cwd, const char *name, mode_t, dev_t dev);
struct inode *ramfs_lookup(struct inode *cwd, const char *name);

#endif /* __RAMFS_H__ */

