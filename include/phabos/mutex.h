/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <phabos/list.h>
#include <phabos/semaphore.h>
#include <phabos/utils.h>

struct mutex {
    struct semaphore semaphore;
};

static inline struct mutex *mutex_create(void)
{
    return (struct mutex*) semaphore_create(1);
}

static inline void mutex_init(struct mutex *mutex)
{
    semaphore_init((struct semaphore*) mutex, 1);
}

static inline void _mutex_lock(struct mutex *mutex)
{
    _semaphore_lock((struct semaphore*) mutex);
}

static inline void mutex_unlock(struct mutex *mutex)
{
    semaphore_unlock((struct semaphore*) mutex);
}

static inline void mutex_destroy(struct mutex *mutex)
{
    semaphore_destroy((struct semaphore*) mutex);
}

static inline bool _mutex_trylock(struct mutex *mutex)
{
    return _semaphore_trylock((struct semaphore*) mutex);
}

#define mutex_lock(l) DEFINE_LOCK_WITH_BARRIER(_mutex_lock, l)
#define mutex_trylock(l) DEFINE_TRYLOCK_WITH_BARRIER(_mutex_trylock, l)

#endif /* __MUTEX_H__ */

