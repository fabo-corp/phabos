#include <config.h>
#include <phabos/hashtable.h>
#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/fs.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#define RAMFS_MAX_FILENAME_SIZE     CONFIG_RAMFS_MAX_FILENAME_SIZE
#define RAMFS_DATA_BLOCK_SIZE       CONFIG_RAMFS_DATA_BLOCK_SIZE

struct ramfs_data_block {
    uint8_t data[RAMFS_DATA_BLOCK_SIZE];
};

struct ramfs_inode {
    char *name;

    struct inode inode;
    struct hashtable *files;
    struct hashtable *blocks;
};

struct ramfs_inode *ramfs_create_inode(const char *name)
{
    struct ramfs_inode *inode;
    size_t name_length = MAX(RAMFS_MAX_FILENAME_SIZE, strlen(name)) + 1;

    inode = zalloc(sizeof(*inode));
    RET_IF_FAIL(inode, NULL);

    inode->name = zalloc(name_length);
    GOTO_IF_FAIL(inode->name, error_filename_alloc);

    memcpy(inode->name, name, name_length -1);
    inode->name[name_length - 1] = '\0';

    inode_init(&inode->inode);
    inode->inode.inode = inode;

    inode->blocks = hashtable_create_uint();
    inode->files = hashtable_create_string();

    return inode;

error_filename_alloc:
    kfree(inode);
    return NULL;
}

int ramfs_mkdir(struct inode *cwd, const char *name, mode_t mode)
{
    struct ramfs_inode *cwd_inode = cwd->inode;
    struct ramfs_inode *ramfs_inode;

    RET_IF_FAIL(cwd, -EINVAL);
    RET_IF_FAIL(is_directory(cwd), -EINVAL);
    RET_IF_FAIL(name, -EINVAL);

    ramfs_inode = hashtable_get(cwd_inode->files, (void*) name);
    RET_IF_FAIL(!ramfs_inode, -EEXIST);

    ramfs_inode = ramfs_create_inode(name);
    RET_IF_FAIL(ramfs_inode, -ENOMEM);

    ramfs_inode->inode.fs = cwd->fs;
    ramfs_inode->inode.flags = S_IFDIR;

    hashtable_add(cwd_inode->files, ramfs_inode->name, &ramfs_inode->inode);

    return 0;
}

int ramfs_mount(struct inode *cwd)
{
    RET_IF_FAIL(cwd, -EINVAL);
    RET_IF_FAIL(is_directory(cwd), -EINVAL);

    cwd->inode = ramfs_create_inode(".");
    RET_IF_FAIL(cwd->inode, -ENOMEM);

    return 0;
}

struct inode *ramfs_lookup(struct inode *cwd, const char *name)
{
    struct ramfs_inode *inode = cwd->inode;

    RET_IF_FAIL(cwd, NULL);
    RET_IF_FAIL(name, NULL);

    // TODO: check name size and truncate if necessary
    return hashtable_get(inode->files, (void*) name);
}

int ramfs_getdents(struct file *file, struct phabos_dirent *dirp, size_t count)
{
    int nread = 0;
    struct hashtable_iterator iter = {0};
    struct ramfs_inode *inode;

    RET_IF_FAIL(dirp, -EINVAL);
    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(file->inode, -EINVAL);

    inode = file->inode->inode;
    iter.i = file->offset;

    while (hashtable_iterate(inode->files, &iter)) {
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
        else if (is_char_device(iter.value))
            *d_type = DT_CHR;

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

int ramfs_mknod(struct inode *cwd, const char *name, mode_t mode, dev_t dev)
{
    struct ramfs_inode *cwd_inode = cwd->inode;
    struct ramfs_inode *ramfs_inode;
    int retval;

    RET_IF_FAIL(cwd, -EINVAL);
    RET_IF_FAIL(is_directory(cwd), -EINVAL);
    RET_IF_FAIL(name, -EINVAL);

    ramfs_inode = hashtable_get(cwd_inode->files, (void*) name);
    RET_IF_FAIL(!ramfs_inode, -EEXIST);

    ramfs_inode = ramfs_create_inode(name);
    RET_IF_FAIL(ramfs_inode, -ENOMEM);

    ramfs_inode->inode.fs = cwd->fs;

    switch (mode & S_IFMT) {
    case S_IFREG:
        ramfs_inode->inode.flags = S_IFREG;
        break;

    case S_IFCHR:
        ramfs_inode->inode.flags = S_IFCHR;
        ramfs_inode->inode.dev = dev;
        break;

    case S_IFBLK:
        ramfs_inode->inode.flags = S_IFBLK;
        break;

    default:
        retval = -EINVAL;
        goto error;
    }

    hashtable_add(cwd_inode->files, ramfs_inode->name, &ramfs_inode->inode);

    return 0;

error:
    kfree(ramfs_inode->name);
    kfree(ramfs_inode);
    return retval;
}

ssize_t ramfs_write(struct file *file, const void *buf, size_t count)
{
    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(file->inode, -EINVAL);
    RET_IF_FAIL(file->inode->inode, -EINVAL);

    struct ramfs_inode *inode = file->inode->inode;
    struct ramfs_data_block *block;

    int block_id = file->offset / RAMFS_DATA_BLOCK_SIZE;
    off_t offset = file->offset % RAMFS_DATA_BLOCK_SIZE;
    size_t wcount = 0;

    while (count) {
        size_t transfer_size = MIN(RAMFS_DATA_BLOCK_SIZE - offset, count);

        block = hashtable_get(inode->blocks, (void*) block_id);
        if (!block) {
            block = zalloc(sizeof(*block));
            RET_IF_FAIL(block, wcount == 0 ? -ENOSPC : wcount);

            hashtable_add(inode->blocks, (void*) block_id, block);
        }

        memcpy(&block->data, buf, transfer_size);

        count -= transfer_size;
        wcount += transfer_size;
        file->offset += transfer_size;
        offset = 0;
        block_id++;
    }

    return wcount;
}

ssize_t ramfs_read(struct file *file, void *buf, size_t count)
{
    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(file->inode, -EINVAL);
    RET_IF_FAIL(file->inode->inode, -EINVAL);

    struct ramfs_inode *inode = file->inode->inode;
    struct ramfs_data_block *block;

    int block_id = file->offset / RAMFS_DATA_BLOCK_SIZE;
    off_t offset = file->offset % RAMFS_DATA_BLOCK_SIZE;
    size_t rcount = 0;

    while (count) {
        size_t transfer_size = MIN(RAMFS_DATA_BLOCK_SIZE - offset, count);

        block = hashtable_get(inode->blocks, (void*) block_id);

        /**
         * FIXME
         *
         * This is not the expected behaviour:
         *  * if the block does not exist because the block is a hole, then read
         *    zeros.
         *  * if the block does not exist because there is nothing to be read,
         *    then block until there is something to read.
         *  * if half the data to read is a hole, and the other half there is
         *    not nothing to read, then return at the end of the hole, then
         *    on the next call block
         */

        if (block)
            memcpy(buf, &block->data, transfer_size);
        else
            memset(buf, 0, transfer_size);

        count -= transfer_size;
        rcount += transfer_size;
        file->offset += transfer_size;
        offset = 0;
        block_id++;
    }

    return rcount;
}

struct fs ramfs_fs = {
    .name = "ramfs",

    .file_ops = {
        .getdents = ramfs_getdents,
        .write = ramfs_write,
        .read = ramfs_read,
    },

    .inode_ops = {
        .mount = ramfs_mount,
        .mkdir = ramfs_mkdir,
        .lookup = ramfs_lookup,
        .mknod = ramfs_mknod,
    },
};
