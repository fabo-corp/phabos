/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>

#include <phabos/syscall.h>
#include <phabos/fs.h>

off_t lseek(int fd, off_t offset, int whence)
{
    long retval = syscall(SYS_LSEEK, fd, offset, whence);
    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

ssize_t read(int fd, void *buf, size_t count)
{
    long retval = syscall(SYS_READ, fd, buf, count);

    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    long retval = syscall(SYS_WRITE, fd, buf, count);
    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}


int getdents(int fd, struct phabos_dirent *dirp, size_t count)
{
    long retval = syscall(SYS_GETDENTS, fd, dirp, count);
    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

int close(int fd)
{
    long retval = syscall(SYS_CLOSE, fd);

    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return 0;
}

int open(const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    long retval;
    va_list vl;

    if (flags & O_CREAT) {
        va_start(vl, flags);
        mode = va_arg(vl, typeof(mode));
        va_end(vl);
    }

    retval = syscall(SYS_OPEN, pathname, flags, mode);

    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

int creat(const char *pathname, mode_t mode)
{
    long retval = syscall(SYS_CREAT, pathname, mode);
    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    long retval = syscall(SYS_MKNOD, pathname, mode, dev);
    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

int mkdir(const char *pathname, mode_t mode)
{
    long retval = syscall(SYS_MKDIR, pathname, mode);
    if (retval) {
        errno = -retval;
        return -1;
    }

    return 0;
}

int ioctl(int fd, unsigned long request, ...)
{
    va_list vl;

    va_start(vl, request);

    long retval = syscall(SYS_IOCTL, fd, request, vl);

    if (retval < 0) {
        errno = -retval;
    }

    va_end(vl);

    return retval;
}

int mount(const char *source, const char *target, const char *filesystemtype,
          unsigned long mountflags, const void *data)
{
    long retval =
        syscall(SYS_MOUNT, source, target, filesystemtype, mountflags, data);

    if (retval) {
        errno = -retval;
        return -1;
    }

    return 0;
}
