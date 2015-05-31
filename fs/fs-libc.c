/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <unistd.h>
#include <fcntl.h>

int _write(int fd, char *buffer, int count)
{
    return write(fd, buffer, count);
}

int _open(const char *pathname, int flags, mode_t mode)
{
    return open(pathname, flags, mode);
}

int _close(int fd)
{
    return close(fd);
}

int _fstat(int fd, struct stat *stat)
{
    return -1;
}

int _read(int fd, char *buffer, int count)
{
    return read(fd, buffer, count);
}

int _lseek(int fd, int offset, int whence)
{
    return lseek(fd, offset, whence);
}

int _isatty(int fd)
{
    return -1;
}
