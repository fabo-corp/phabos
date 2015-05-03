/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>
#include <string.h>

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

static inline void *zalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

#endif /* __UTILS_H__ */

