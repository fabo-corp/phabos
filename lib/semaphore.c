/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <phabos/semaphore.h>
#include <phabos/list.h>
#include <phabos/scheduler.h>
#include <phabos/assert.h>
#include <phabos/mm.h>
#include <asm/irq.h>

struct semaphore *semaphore_create(unsigned val)
{
    struct semaphore *semaphore;

    semaphore = kmalloc(sizeof(*semaphore), MM_KERNEL);
    if (!semaphore)
        return NULL;
    semaphore_init(semaphore, val);

    return semaphore;
}

void semaphore_init(struct semaphore *semaphore, unsigned val)
{
    RET_IF_FAIL(semaphore,);

    memset(semaphore, 0, sizeof(*semaphore));
    list_init(&semaphore->wait_list);
    atomic_init(&semaphore->count, val);
}

void semaphore_destroy(struct semaphore *semaphore)
{
    if (!semaphore)
        return;

    RET_IF_FAIL(list_is_empty(&semaphore->wait_list),);
    kfree(semaphore);
}

void _semaphore_lock(struct semaphore *semaphore)
{
    RET_IF_FAIL(semaphore,);

    irq_disable();
    while (atomic_get(&semaphore->count) <= 0) {
        task_add_to_wait_list(task_get_running(), &semaphore->wait_list);
        irq_enable();
        sched_yield();
        irq_disable();
    }

    atomic_dec(&semaphore->count);
    irq_enable();
}

bool _semaphore_trylock(struct semaphore *semaphore)
{
    RET_IF_FAIL(semaphore, false);

    irq_disable();
    if (atomic_get(&semaphore->count) <= 0) {
        irq_enable();
        return false;
    }

    semaphore_lock(semaphore);
    irq_enable();
    return true;
}

void semaphore_unlock(struct semaphore *semaphore)
{
    RET_IF_FAIL(semaphore,);

    atomic_inc(&semaphore->count);

    if (!list_is_empty(&semaphore->wait_list))
        task_remove_from_wait_list(list_first_entry(&semaphore->wait_list,
                                                    struct task, list));
}
