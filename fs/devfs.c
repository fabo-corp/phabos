#include <phabos/driver.h>
#include <phabos/hashtable.h>
#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/fs.h>
#include "ramfs.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

static struct inode devfs_root = {
    .flags = S_IFDIR,
};

static int devfs_mount(struct inode *cwd)
{
    RET_IF_FAIL(cwd, -EINVAL);
    RET_IF_FAIL(is_directory(cwd), -EINVAL);

    cwd->inode = devfs_root.inode;

    return 0;
}

int devfs_init(void)
{
    return ramfs_mount(&devfs_root);
}

int devfs_mknod(const char *name, mode_t mode, dev_t dev)
{
    return ramfs_mknod(&devfs_root, name, mode, dev);
}

static struct fs devfs_fs = {
    .name = "devfs",

    .file_ops = {
        .getdents = ramfs_getdents,
        .write = ramfs_write,
        .read = ramfs_read,
    },

    .inode_ops = {
        .mount = devfs_mount,
        .mkdir = ramfs_mkdir,
        .lookup = ramfs_lookup,
        .mknod = ramfs_mknod,
    },
};

static int devfs_init_driver(struct driver *driver)
{
    return fs_register(&devfs_fs);
}

__driver__ struct driver devfs_driver = {
    .name = "devfs",
    .init = devfs_init_driver,
};
