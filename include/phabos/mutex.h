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

static inline void mutex_lock(struct mutex *mutex)
{
    semaphore_lock((struct semaphore*) mutex);
}

static inline void mutex_unlock(struct mutex *mutex)
{
    semaphore_unlock((struct semaphore*) mutex);
}

static inline void mutex_destroy(struct mutex *mutex)
{
    semaphore_destroy((struct semaphore*) mutex);
}

static inline bool mutex_trylock(struct mutex *mutex)
{
    return semaphore_trylock((struct semaphore*) mutex);
}

#endif /* __MUTEX_H__ */

