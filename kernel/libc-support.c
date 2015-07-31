/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#if 0
#include <stdint.h>
#include <asm/spinlock.h>
#include <stdio.h>

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
#endif
