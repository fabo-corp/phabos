/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define containerof(x, s, f) ((void*) ((char*)(x) - offsetof(s, f)))

#define __weak__ __attribute__((weak))

#endif /* __UTILS_H__ */

