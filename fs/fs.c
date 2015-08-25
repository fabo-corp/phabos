#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <phabos/assert.h>
#include <phabos/hashtable.h>
#include <phabos/syscall.h>
#include <phabos/fs.h>
#include <phabos/utils.h>
#include <phabos/mutex.h>
#include <phabos/string.h>
#include <phabos/scheduler.h>
#include <phabos/driver.h>

#define O_CLOEXEC 0x80000

static struct inode *root;
static struct hashtable *fs_table;

int sys_open(const char *pathname, int flags, mode_t mode);

void fs_init(void)
{
    fs_table = hashtable_create_string();

    root = inode_alloc();
    RET_IF_FAIL(root,);

    root->flags = S_IFDIR;
}

int fs_register(struct fs *fs)
{
    hashtable_add(fs_table, (void*) fs->name, fs);
    return 0;
}

struct inode *_fs_walk(struct inode *cwd, char *pathname)
{
    int i = 0;
    int j;

    RET_IF_FAIL(cwd, NULL);
    RET_IF_FAIL(pathname, NULL);

    if (!is_directory(cwd))
        return NULL;

    while (pathname[i] == '/')
        i++;

    if (i) {
        pathname += i;
        cwd = root;
        i = 0;
    }

    if (pathname[0] == '\0')
        return cwd;

    while (pathname[i] != '/' && pathname[i] != '\0')
        i++;

    for (j = i; pathname[j] == '/'; j++)
        ;

    if (pathname[j] == '\0') {
        return cwd->fs->inode_ops.lookup(cwd, pathname);
    }

    pathname[i] = '\0';
    return _fs_walk(cwd->fs->inode_ops.lookup(cwd, pathname), pathname + j);
}

struct inode *fs_walk(struct inode *cwd, const char *pathname)
{
    size_t length;
    char *path;
    struct inode *node;

    RET_IF_FAIL(pathname, NULL);

    length = strlen(pathname) + 1;
    path = kmalloc(length, MM_KERNEL);

    RET_IF_FAIL(path, NULL);

    memcpy(path, pathname, length);
    node = _fs_walk(cwd, path);
    kfree(path);

    return node;
}

int sys_mount(const char *source, const char *target,
              const char *filesystemtype, unsigned long mountflags,
              const void *data)
{
    struct fs *fs;
    struct inode *cwd;
    struct inode *inode;
    int retval;

    RET_IF_FAIL(root || !target, -EINVAL);

    fs = hashtable_get(fs_table, (char*) filesystemtype);
    if (!fs)
        return -ENODEV;

    if (target)
        cwd = fs_walk(root, target);
    else
        cwd = root;

    /* Cannot find target or target is not a directory */
    if (!cwd || !is_directory(cwd))
        return -ENOTDIR;

    /* Directory already mounted */
    if (cwd->mounted_inode)
        return -EBUSY;

    // TODO: check that the directory is empty

    inode = inode_alloc();
    if (!inode)
        return -ENOMEM;

    memcpy(inode, cwd, sizeof(*inode));

    inode_init(cwd);
    cwd->fs = cwd->fs;
    cwd->flags = S_IFDIR;
    cwd->fs = fs;
    cwd->mounted_inode = inode;

    retval = fs->inode_ops.mount(cwd);
    if (retval)
        goto error_mount;

    return 0;

error_mount:
    inode = cwd->mounted_inode;
    memcpy(cwd, inode, sizeof(*inode));
    kfree(inode);

    return retval;
}
DEFINE_SYSCALL(SYS_MOUNT, mount, 5);

int sys_mkdir(const char *pathname, mode_t mode)
{
    struct inode *inode;
    char *name;
    char *path;
    int retval;

    if (!pathname)
        return -EINVAL;

    name = abasename(pathname);
    path = adirname(pathname);

    inode = _fs_walk(root, path);
    if (!inode) {
        retval = -ENOENT;
        goto exit;
    }

    RET_IF_FAIL(inode->fs, -EINVAL);
    RET_IF_FAIL(inode->fs->inode_ops.lookup, -EINVAL);
    RET_IF_FAIL(inode->fs->inode_ops.mkdir, -EINVAL);

    if (inode->fs->inode_ops.lookup(inode, name)) {
        retval = -EEXIST;
        goto exit;
    }

    retval = inode->fs->inode_ops.mkdir(inode, name, mode);

exit:
    kfree(name);
    kfree(path);

    return retval;
}
DEFINE_SYSCALL(SYS_MKDIR, mkdir, 5);

int sys_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    int retval;
    char *name;
    char *path;
    struct inode *inode;

    RET_IF_FAIL(pathname, -EINVAL);

    name = abasename(pathname);
    if (!name)
        return -ENOMEM;

    path = adirname(pathname);
    if (!path) {
        retval = -ENOMEM;
        goto end;
    }

    inode = fs_walk(root, path);
    if (!inode) {
        retval = -ENOENT;
        goto end;
    }

    if (!inode->fs) {
        retval = -EINVAL;
        goto end;
    }

    if (!inode->fs->inode_ops.mknod) {
        retval = -ENOSYS;
        goto end;
    }

    retval = inode->fs->inode_ops.mknod(inode, name, mode, dev);

end:
    kfree(name);
    kfree(path);

    return retval;
}
DEFINE_SYSCALL(SYS_MKNOD, mknod, 3);

int sys_creat(const char *pathname, mode_t mode)
{
    return sys_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, 0);
}
DEFINE_SYSCALL(SYS_CREAT, creat, 2);

int sys_open(const char *pathname, int flags, mode_t mode)
{
    struct task *task;
    struct inode *inode;
    struct fd *fd;
    int fdnum;
    int retval;

    if (!pathname)
        return -EINVAL;

    inode = fs_walk(root, pathname);

    if (inode && flags & O_CREAT && flags & O_EXCL)
        return -EEXIST;

    if (!inode && flags & O_CREAT) {
        retval = sys_mknod(pathname, (mode & S_IFMT) | S_IFREG, 0);
        if (retval)
            return retval;
    }

    inode = fs_walk(root, pathname);
    if (!inode)
        return -ENOMEM; // FIXME not right error

    fdnum = allocate_fdnum();
    if (fdnum < 0)
        return fdnum;

    task = task_get_running();
    RET_IF_FAIL(task, -1);

    fd = hashtable_get(task->fd, (void*) fdnum);

    if (flags & O_CLOEXEC)
        fd->flags |= O_CLOEXEC;
    flags &= ~O_CLOEXEC;

    fd->file = zalloc(sizeof(*fd->file));
    if (!fd->file) {
        retval = -ENOMEM;
        goto error_file_alloc;
    }

    fd->file->inode = inode;
    fd->file->flags = flags;

    if (is_directory(inode))
        mutex_lock(&inode->dlock);

    if (is_char_device(fd->file->inode)) {
        struct device *dev = devnum_get_device(fd->file->inode->dev);

        if (dev->ops.open) {
            retval =  dev->ops.open(fd->file);
            if (retval)
                goto error_char_device_open;
        }
    }

    return fdnum;

error_char_device_open:
error_file_alloc:
    free_fdnum(fdnum);
    return retval;
}
DEFINE_SYSCALL(SYS_OPEN, open, 3);

int sys_close(int fdnum)
{
    struct fd *fd;

    fd = to_fd(fdnum);
    if (!fd)
        return -EBADF;

    if (is_char_device(fd->file->inode)) {
        struct device *dev = devnum_get_device(fd->file->inode->dev);

        if (dev->ops.close)
            dev->ops.close(fd->file);
    }

    return free_fdnum(fdnum);
}
DEFINE_SYSCALL(SYS_CLOSE, close, 1);

int sys_getdents(int fdnum, struct phabos_dirent *dirp, size_t count)
{
    struct fd *fd;

    fd = to_fd(fdnum);
    if (!fd)
        return -EBADF;

    RET_IF_FAIL(fd->file, -EINVAL);
    RET_IF_FAIL(fd->file->inode, -EINVAL);
    RET_IF_FAIL(fd->file->inode->fs, -EINVAL);
    RET_IF_FAIL(fd->file->inode->fs->file_ops.getdents, -ENOSYS);
    return fd->file->inode->fs->file_ops.getdents(fd->file, dirp, count);
}
DEFINE_SYSCALL(SYS_GETDENTS, getdents, 3);

int sys_ioctl(int fdnum, unsigned long cmd, va_list vl)
{
    struct fd *fd;

    fd = to_fd(fdnum);
    if (!fd)
        return -EBADF;

    RET_IF_FAIL(fd->file, -EINVAL);
    RET_IF_FAIL(fd->file->inode, -EINVAL);

    if (is_char_device(fd->file->inode)) {
        struct device *dev = devnum_get_device(fd->file->inode->dev);
        RET_IF_FAIL(dev, -EINVAL);

        if (dev->ops.ioctl)
            return dev->ops.ioctl(fd->file, cmd, vl);

        return -ENOSYS;
    }

    RET_IF_FAIL(fd->file->inode->fs, -EINVAL);

    if (!fd->file->inode->fs->file_ops.ioctl)
        return -ENOSYS;

    return fd->file->inode->fs->file_ops.ioctl(fd->file, cmd, vl);
}
DEFINE_SYSCALL(SYS_IOCTL, ioctl, 3);

ssize_t sys_write(int fdnum, const void *buf, size_t count)
{
    struct fd *fd;

    if (!count)
        return 0;

    fd = to_fd(fdnum);
    if (!fd)
        return -EBADF;

    RET_IF_FAIL(buf, -EINVAL);
    RET_IF_FAIL(fd->file, -EINVAL);
    RET_IF_FAIL(fd->file->inode, -EINVAL);

    if (is_char_device(fd->file->inode)) {
        struct device *dev = devnum_get_device(fd->file->inode->dev);
        RET_IF_FAIL(dev, -EINVAL);

        if (dev->ops.write)
            return dev->ops.write(fd->file, buf, count);

        return 0;
    }

    RET_IF_FAIL(fd->file->inode->fs, -EINVAL);
    RET_IF_FAIL(fd->file->inode->fs->file_ops.write, -ENOSYS);
    return fd->file->inode->fs->file_ops.write(fd->file, buf, count);
}
DEFINE_SYSCALL(SYS_WRITE, write, 3);

ssize_t sys_read(int fdnum, void *buf, size_t count)
{
    struct fd *fd;

    if (!count)
        return 0;

    fd = to_fd(fdnum);
    if (!fd)
        return -EBADF;

    RET_IF_FAIL(buf, -EINVAL);
    RET_IF_FAIL(fd->file, -EINVAL);
    RET_IF_FAIL(fd->file->inode, -EINVAL);

    if (is_char_device(fd->file->inode)) {
        struct device *dev = devnum_get_device(fd->file->inode->dev);
        RET_IF_FAIL(dev, -EINVAL);

        if (dev->ops.read)
            return dev->ops.read(fd->file, buf, count);

        return 0;
    }

    RET_IF_FAIL(fd->file->inode->fs, -EINVAL);
    RET_IF_FAIL(fd->file->inode->fs->file_ops.read, -ENOSYS);
    return fd->file->inode->fs->file_ops.read(fd->file, buf, count);
}
DEFINE_SYSCALL(SYS_READ, read, 3);

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fdnum,
               off_t offset)
{
    struct fd *fd;

    if (!length)
        return (void*) ~0;

    fd = to_fd(fdnum);
    if (!fd)
        return (void*) ~0;

    RET_IF_FAIL(fd->file, (void*) ~0);
    RET_IF_FAIL(fd->file->inode, (void*) ~0);

    RET_IF_FAIL(fd->file->inode->fs, (void*) ~0);
    RET_IF_FAIL(fd->file->inode->fs->file_ops.mmap, (void*) ~0);
    return fd->file->inode->fs->file_ops.mmap(fd->file, addr, length, prot,
                                              flags, offset);
}
DEFINE_SYSCALL(SYS_MMAP, mmap, 6);

off_t sys_lseek(int fdnum, off_t offset, int whence)
{
    struct fd *fd;

    fd = to_fd(fdnum);
    if (!fd)
        return -EBADF;

    RET_IF_FAIL(fd->file, -EINVAL);

    switch (whence) {
    case SEEK_SET:
        fd->file->offset = offset;
        break;

    case SEEK_CUR:
        // FIXME: check for overflow
        fd->file->offset += offset;
        break;

    default:
        return -EINVAL;
    }

    return fd->file->offset;
}
DEFINE_SYSCALL(SYS_LSEEK, lseek, 3);
