/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <phabos/syscall.h>

extern uint32_t _syscall;
extern uint32_t _esyscall;

static int syscall_compare(const void *x, const void *y)
{
    const struct syscall *a = x;
    const struct syscall *b = y;

    return b->id - a->id;
}

void syscall_init(void)
{
    size_t sys_count = ((uint32_t) &_esyscall - (uint32_t) &_syscall) /
                       sizeof(struct syscall);
    qsort(&_syscall, sys_count, sizeof(struct syscall), syscall_compare);
}

struct syscall *syscall_find(int id)
{
    struct syscall sys = {.id = id};
    size_t sys_count = ((uint32_t) &_esyscall - (uint32_t) &_syscall) /
                       sizeof(struct syscall);
    return bsearch(&sys, &_syscall, sys_count, sizeof(struct syscall),
                   syscall_compare);
}
