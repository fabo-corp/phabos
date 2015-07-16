#include <config.h>
#include <phabos/hashtable.h>
#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/fs.h>
#include <phabos/fs/binfs.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

extern uint32_t _apps;
extern uint32_t _eapps;
struct fs binfs_fs;

static int binfs_mount(struct inode *cwd)
{
    return 0;
}

static struct inode *binfs_lookup(struct inode *cwd, const char *name)
{
    struct inode *inode;
    size_t entry_count = (size_t) ((char*) &_eapps - (char*) &_apps) /
                                   sizeof(struct bin_entry);
    struct bin_entry *bin = (struct bin_entry*) &_apps;

    RET_IF_FAIL(cwd, NULL);
    RET_IF_FAIL(name, NULL);

    for (unsigned int i = 0; i < entry_count; i++) {
        if (strcmp(name, bin[i].name))
            continue;

        inode = inode_alloc();
        RET_IF_FAIL(inode, NULL);

        inode->fs = &binfs_fs;
        inode->flags = S_IFREG;
        inode->inode = &bin[i];
        return inode;
    }

    return NULL;
}

static int binfs_getdents(struct file *file, struct phabos_dirent *dirp,
                          size_t count)
{
    int iter = 0;
    int nread = 0;
    char *binbuf;
    char *lastentry = (char*) &_eapps - sizeof(struct bin_entry);

    RET_IF_FAIL(dirp, -EINVAL);
    RET_IF_FAIL(file, -EINVAL);

    iter = file->offset;
    binbuf = (char*) &_apps + sizeof(struct bin_entry) * file->offset;

    while (binbuf <= lastentry) {
        struct bin_entry *bin = (struct bin_entry*) binbuf;
        size_t name_length = strlen(bin->name) + 1;
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
        *d_type = DT_REG;

        file->offset = dirp->d_off = ++iter;
        memcpy(dirp->d_name, bin->name, name_length);
        nread += dirent_length;
        dirp->d_reclen = dirent_length;

        dirp = (struct phabos_dirent *) ((char*) dirp + dirent_length);
        count -= dirent_length;

        binbuf += sizeof(*bin);
    }

exit:
    return nread;
}

static ssize_t binfs_read(struct file *file, void *buf, size_t count)
{
    struct bin_entry *bin;
    ssize_t nread;

    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(file->inode, -EINVAL);
    RET_IF_FAIL(file->inode->inode, -EINVAL);

    bin = file->inode->inode;

    if (file->offset >= *bin->size)
        return 0;

    nread = MIN(count, *bin->size - file->offset);
    memcpy(buf, bin->bin + file->offset, nread);

    file->offset += nread;
    return nread;
}

static void *binfs_mmap(struct file *file, void *addr, size_t length, int prot,
                        int flags, off_t offset)
{
    struct bin_entry *bin;

    RET_IF_FAIL(file, (void*) ~0);
    RET_IF_FAIL(file->inode, (void*) ~0);
    RET_IF_FAIL(file->inode->inode, (void*) ~0);

    bin = file->inode->inode;

    if (length + offset > *bin->size || offset >= *bin->size)
        return (void*) ~0;

    return (char*) bin->bin + offset;
}

static int binfs_fstat(struct file *file, struct stat *buf)
{
    struct bin_entry *bin;

    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(file->inode, -EINVAL);
    RET_IF_FAIL(file->inode->inode, -EINVAL);

    bin = file->inode->inode;

    buf->st_size = *bin->size;
    return 0;
}

struct fs binfs_fs = {
    .name = "binfs",

    .file_ops = {
        .getdents = binfs_getdents,
        .read = binfs_read,
        .mmap = binfs_mmap,
        .fstat = binfs_fstat,
    },

    .inode_ops = {
        .mount = binfs_mount,
        .lookup = binfs_lookup,
    },
};
