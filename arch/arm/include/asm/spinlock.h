/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <stdbool.h>
#include <phabos/utils.h>

struct spinlock {
};

#define SPINLOCK_INIT(x) {}

struct spinlock *spinlock_create(void);
void spinlock_init(struct spinlock *spinlock);
void _spinlock_lock(struct spinlock *spinlock);
void spinlock_unlock(struct spinlock *spinlock);
void spinlock_destroy(struct spinlock *spinlock);
bool _spinlock_trylock(struct spinlock *spinlock);

#define spinlock_lock(l) DEFINE_LOCK_WITH_BARRIER(_spinlock_lock, l)
#define spinlock_trylock(l) DEFINE_TRYLOCK_WITH_BARRIER(_spinlock_trylock, l)

#endif /* __SPINLOCK_H__ */

