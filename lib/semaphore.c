/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <errno.h>
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

static int semaphore_interrupt(struct task *task, void *lock)
{
    task->lock_interrupted = true;
    task_remove_from_wait_list(task);
    return 0;
}

static int __semaphore_down_interruptible(struct semaphore *semaphore,
                                          interrupt_lock_cb unlock_cb)
{
    struct task *task = task_get_running();

    RET_IF_FAIL(semaphore, -EINVAL);

    task->lock_interrupted = false;

    irq_disable();
    while (atomic_get(&semaphore->count) <= 0 && !task->lock_interrupted) {
        task_add_to_wait_list(task, &semaphore->wait_list);
        task->unlock = unlock_cb;
        task->lock_handle = semaphore;

        irq_enable();
        sched_yield();
        irq_disable();
    }

    task->lock_handle = NULL;
    task->unlock = NULL;
    atomic_dec(&semaphore->count);
    irq_enable();

    if (task->lock_interrupted) {
        task->lock_interrupted = false;
        return -EINTR;
    }

    return 0;
}

int _semaphore_down_interruptible(struct semaphore *semaphore)
{
    return __semaphore_down_interruptible(semaphore, semaphore_interrupt);
}

void _semaphore_lock(struct semaphore *semaphore)
{
    __semaphore_down_interruptible(semaphore, NULL);
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
