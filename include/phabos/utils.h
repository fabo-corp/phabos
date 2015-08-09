/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>

#include <asm/machine.h>
#include <phabos/mm.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define containerof(x, s, f) ((void*) ((char*)(x) - offsetof(s, f)))

#define __weak__ __attribute__((weak))

#define MAX(x, y) ({ \
    typeof(x) __a = (x); \
    typeof(y) __b = (y); \
    __a > __b ? __a : __b; })
#define MIN(x, y) ({ \
    typeof(x) __a = (x); \
    typeof(y) __b = (y); \
    __a < __b ? __a : __b; })

#define barrier() asm volatile("" ::: "memory");

#define DEFINE_LOCK_WITH_BARRIER(fct, ...)  \
    do {                                    \
        fct(__VA_ARGS__);                   \
        barrier();                          \
    } while (0)

#define DEFINE_TRYLOCK_WITH_BARRIER(fct, ...)   \
    ({                                          \
        bool locked = fct(__VA_ARGS__);         \
        barrier();                              \
        locked;                                 \
    })

static inline uint64_t msecs_to_ticks(unsigned long msecs)
{
    return (msecs * HZ) / 1000;
}

#endif /* __UTILS_H__ */

