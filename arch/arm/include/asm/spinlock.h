/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <stdbool.h>

struct spinlock {
};

#define SPINLOCK_INIT(x) {}

struct spinlock *spinlock_create(void);
void spinlock_init(struct spinlock *spinlock);
void spinlock_lock(struct spinlock *spinlock);
void spinlock_unlock(struct spinlock *spinlock);
void spinlock_destroy(struct spinlock *spinlock);
bool spinlock_trylock(struct spinlock *spinlock);

#endif /* __SPINLOCK_H__ */

