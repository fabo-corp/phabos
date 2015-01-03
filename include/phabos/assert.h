/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __LIB_ASSERT_H__
#define __LIB_ASSERT_H__

#include <phabos/kprintf.h>

#define RET_IF_FAIL(cond, val) \
    do { \
        if (!(cond)) { \
            kprintf("%s:%d | assert failed: %s\n", __FILE__, __LINE__, #cond); \
            return val; \
        } \
    } while (0)

#define GOTO_IF_FAIL(cond, label) \
    do { \
        if (!(cond)) { \
            kprintf("%s:%d | assert failed: %s\n", __FILE__, __LINE__, #cond); \
            goto label; \
        } \
    } while (0)

#endif /* __LIB_ASSERT_H__ */

