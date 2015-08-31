/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include <stdint.h>

typedef volatile int atomic_t;

static inline uint32_t atomic_get(atomic_t *atomic)
{
    return *(uint32_t*) atomic;
}

static inline void atomic_init(atomic_t *atomic, uint32_t val)
{
    *atomic = (atomic_t) val;
}

uint32_t atomic_add(atomic_t *atomic, int n);
uint32_t atomic_inc(atomic_t *atomic);
uint32_t atomic_dec(atomic_t *atomic);

#endif /* __ATOMIC_H__ */

