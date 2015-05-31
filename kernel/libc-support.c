/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <stdint.h>
#include <asm/spinlock.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static struct spinlock malloc_spinlock = SPINLOCK_INIT(malloc_spinlock);

uint32_t _sbrk(int incr)
{
    extern uint32_t _sheap;
    static uint32_t s_heap_end;
    uint32_t new_heap_space;

    if (!s_heap_end)
        s_heap_end = (uint32_t) &_sheap;

    new_heap_space = s_heap_end;
    s_heap_end += incr;
    return new_heap_space;
}

void __malloc_lock(struct _reent *reent)
{
    spinlock_lock(&malloc_spinlock);
}

void __malloc_unlock(struct _reent *reent)
{
    spinlock_unlock(&malloc_spinlock);
}

int _write(int fd, char *buffer, int count)
{
    return write(fd, buffer, count);
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

int _isatty(int file)
{
    return -1;
}
