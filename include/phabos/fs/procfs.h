#ifndef __FS_PROCFS_H__
#define __FS_PROCFS_H__

struct procfs_file {
    const char *name;
    struct inode inode;
};

struct procfs_directory {
    const char *name;
    struct inode inode;
    struct hashtable *files;
};

#endif /* __FS_PROCFS_H__ */

