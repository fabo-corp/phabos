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

#define SYS_EXIT                        1
#define SYS_READ                        3
#define SYS_WRITE                       4
#define SYS_OPEN                        5
#define SYS_CLOSE                       6
#define SYS_CREAT                       8
#define SYS_MKNOD                       14
#define SYS_LSEEK                       19
#define SYS_GETPID                      20
#define SYS_MOUNT                       21
#define SYS_KILL                        37
#define SYS_MKDIR                       39
#define SYS_RMDIR                       40
#define SYS_UMOUNT                      52
#define SYS_IOCTL                       54
#define SYS_MMAP                        90
#define SYS_GETDENTS                    141
#define SYS_SCHED_SETSCHEDULER          156
#define SYS_SCHED_GETSCHEDULER          157
#define SYS_SCHED_YIELD                 158
#define SYS_SCHED_GET_PRIORITY_MAX      159
#define SYS_SCHED_GET_PRIORITY_MIN      160
#define SYS_SCHED_RR_GET_INTERVAL       161

#define SYS_SPAWN                       0xfab0

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

