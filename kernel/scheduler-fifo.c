/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <config.h>
#include <asm/spinlock.h>
#include <phabos/list.h>
#include <phabos/utils.h>
#include <phabos/scheduler.h>

#include <errno.h>

static struct list_head fifo_runqueue[CONFIG_SCHED_NUM_FIFO_PRIORITIES];
static struct spinlock fifo_runqueue_lock = SPINLOCK_INIT(fifo_runqueue_lock);

static int sched_fifo_init(void)
{
    for (int i = 0; i < ARRAY_SIZE(fifo_runqueue); i++)
        list_init(&fifo_runqueue[i]);
    return 0;
}

static int sched_fifo_get_priority_min(void)
{
    return 0;
}

static int sched_fifo_get_priority_max(void)
{
    return CONFIG_SCHED_NUM_FIFO_PRIORITIES - 1;
}

static struct task *sched_fifo_pick_task(void)
{
    struct task *task = NULL;

    spinlock_lock(&fifo_runqueue_lock);

    for (int i = ARRAY_SIZE(fifo_runqueue) - 1; i >= 0; i--) {
        if (list_is_empty(&fifo_runqueue[i]))
            continue;

        task = list_first_entry(&fifo_runqueue[i], struct task, list);
        break;
    }

    spinlock_unlock(&fifo_runqueue_lock);

    return task;
}

static int sched_fifo_enqueue_task(struct task *task)
{
    RET_IF_FAIL(task, -EINVAL);
    RET_IF_FAIL(sched_fifo_get_priority_min() <= task->priority, -EINVAL);
    RET_IF_FAIL(sched_fifo_get_priority_max() >= task->priority, -EINVAL);

    spinlock_lock(&fifo_runqueue_lock);
    list_add(&fifo_runqueue[task->priority], &task->list);
    spinlock_unlock(&fifo_runqueue_lock);

    return 0;
}

static int sched_fifo_dequeue_task(struct task *task)
{
    RET_IF_FAIL(task, -EINVAL);

    spinlock_lock(&fifo_runqueue_lock);
    list_del(&task->list);
    spinlock_unlock(&fifo_runqueue_lock);

    return 0;
}

struct sched_policy sched_fifo_policy = {
    .init = sched_fifo_init,
    .pick_task = sched_fifo_pick_task,
    .enqueue_task = sched_fifo_enqueue_task,
    .dequeue_task = sched_fifo_dequeue_task,
    .get_priority_min = sched_fifo_get_priority_min,
    .get_priority_max = sched_fifo_get_priority_max,
};
