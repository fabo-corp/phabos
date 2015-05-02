/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stdint.h>
#include <stddef.h>

struct syscall {
    int id;
    size_t param_count;
    uintptr_t handler;
};

#define __syscall__ __attribute__((section(".syscall")))
#define DEFINE_SYSCALL(sysid, sysname, count)           \
    __syscall__ struct syscall sys_##sysid = {          \
        .id = sysid,                                    \
        .param_count = count,                           \
        .handler = (uintptr_t) sys_##sysname,           \
    }

void syscall_init(void);
long syscall(long id, ...);

#endif /* __SYSCALL_H__ */

