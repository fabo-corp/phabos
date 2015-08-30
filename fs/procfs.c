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

static struct procfs_directory procfs_root = {
    .name = ".",

    .inode = {
        .flags = S_IFDIR,
    },
};

int procfs_add_file(struct procfs_file *file)
{
    if (!file || !file->name)
        return -EINVAL;

    hashtable_add(procfs_root.files, file->name, &file->inode);

    return 0;
}

static int procfs_mount(struct inode *cwd)
{
    RET_IF_FAIL(cwd, -EINVAL);
    RET_IF_FAIL(is_directory(cwd), -EINVAL);

    cwd->inode = &procfs_root.inode;

    return 0;
}

struct inode *procfs_lookup(struct inode *cwd, const char *name)
{
    struct procfs_directory *dir = cwd->inode;

    RET_IF_FAIL(cwd, NULL);
    RET_IF_FAIL(name, NULL);
    RET_IF_FAIL(is_directory(&dir->inode), NULL);

    return hashtable_get(dir->files, name);
}

static ssize_t procfs_read(struct file *file, void *buf, size_t count)
{
    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(file->inode, -EINVAL);
    RET_IF_FAIL(file->inode->inode, -EINVAL);

    struct procfs_inode *inode = file->inode->inode;
    return 0;
}

static int procfs_getdents(struct file *file, struct phabos_dirent *dirp,
                          size_t count)
{
    int nread = 0;
    struct hashtable_iterator iter = {0};
    struct procfs_directory *dir;

    RET_IF_FAIL(dirp, -EINVAL);
    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(file->inode, -EINVAL);
    RET_IF_FAIL(is_directory(file->inode), -EINVAL);

    iter.i = file->offset;
    dir = containerof(file->inode->inode, struct procfs_directory, inode);

    while (hashtable_iterate(dir->files, &iter)) {
        size_t name_length = strlen(iter.key) + 1;
        size_t dirent_length = sizeof(*dirp) + name_length + 2;
        char *d_type;

        dirent_length += sizeof(void*) - 1;
        dirent_length &= ~(sizeof(void*) - 1);

        if (dirent_length > count) {
            if (nread)
                goto exit;
            return -EINVAL;
        }

        d_type = (char*) dirp + dirent_length - 1;
        *d_type = DT_UNKNOWN;
        if (is_directory(iter.value))
            *d_type = DT_DIR;
        else if (is_regular_file(iter.value))
            *d_type = DT_REG;

        file->offset = dirp->d_off = iter.i;
        memcpy(dirp->d_name, iter.key, name_length);
        nread += dirent_length;
        dirp->d_reclen = dirent_length;

        dirp = (struct phabos_dirent *) ((char*) dirp + dirent_length);
        count -= dirent_length;
    }

exit:
    return nread;
}

static struct fs procfs_fs = {
    .name = "procfs",

    .file_ops = {
        .getdents = procfs_getdents,
        .read = procfs_read,
    },

    .inode_ops = {
        .mount = procfs_mount,
        .lookup = procfs_lookup,
    },
};

static int procfs_init(struct driver *driver)
{
    procfs_root.files = hashtable_create_string();
    if (!procfs_root.files)
        return -ENOMEM;

    return fs_register(&procfs_fs);
}

__driver__ struct driver procfs_driver = {
    .name = "procfs",
    .init = procfs_init,
};
