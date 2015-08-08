/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include <asm/atomic.h>
#include <phabos/list.h>
#include <phabos/assert.h>
#include <phabos/utils.h>

struct semaphore {
    struct list_head wait_list;
    atomic_t count;
};

struct semaphore *semaphore_create(unsigned val);
void semaphore_init(struct semaphore *semaphore, unsigned val);
void _semaphore_lock(struct semaphore *semaphore);
bool _semaphore_trylock(struct semaphore *semaphore);
void semaphore_unlock(struct semaphore *semaphore);
void semaphore_destroy(struct semaphore *semaphore);

static inline void semaphore_up(struct semaphore *semaphore)
{
    semaphore_unlock(semaphore);
}

static inline void _semaphore_down(struct semaphore *semaphore)
{
    _semaphore_lock(semaphore);
}

static inline unsigned semaphore_get_value(struct semaphore *semaphore)
{
    RET_IF_FAIL(semaphore, 0);
    return atomic_get(&semaphore->count);;
}

#define semaphore_lock(l) DEFINE_LOCK_WITH_BARRIER(_semaphore_lock, l)
#define semaphore_down(l) DEFINE_LOCK_WITH_BARRIER(_semaphore_down, l)
#define semaphore_trylock(l) DEFINE_TRYLOCK_WITH_BARRIER(_semaphore_trylock, l)

#endif /* __SEMAPHORE_H__ */

