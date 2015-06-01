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

struct semaphore {
    struct list_head wait_list;
    atomic_t count;
};

struct semaphore *semaphore_create(unsigned val);
void semaphore_init(struct semaphore *semaphore, unsigned val);
void semaphore_lock(struct semaphore *semaphore);
bool semaphore_trylock(struct semaphore *semaphore);
void semaphore_unlock(struct semaphore *semaphore);
void semaphore_destroy(struct semaphore *semaphore);

static inline void semaphore_up(struct semaphore *semaphore)
{
    semaphore_unlock(semaphore);
}

static inline void semaphore_down(struct semaphore *semaphore)
{
    semaphore_lock(semaphore);
}

#endif /* __SEMAPHORE_H__ */

