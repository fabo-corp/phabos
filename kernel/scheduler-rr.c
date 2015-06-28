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

static struct list_head rr_runqueue[CONFIG_SCHED_NUM_RR_PRIORITIES];
static struct spinlock rr_runqueue_lock = SPINLOCK_INIT(rr_runqueue_lock);

static int sched_rr_init(void)
{
    for (int i = 0; i < ARRAY_SIZE(rr_runqueue); i++)
        list_init(&rr_runqueue[i]);
    return 0;
}

static int sched_rr_get_priority_min(void)
{
    return 0;
}

static int sched_rr_get_priority_max(void)
{
    return CONFIG_SCHED_NUM_RR_PRIORITIES - 1;
}

static struct task *sched_rr_pick_task(void)
{
    spinlock_lock(&rr_runqueue_lock);

    for (int i = ARRAY_SIZE(rr_runqueue) - 1; i >= 0; i--) {
        if (list_is_empty(&rr_runqueue[i]))
            continue;

        list_rotate_anticlockwise(&rr_runqueue[i]);
        return list_first_entry(&rr_runqueue[i], struct task, list);
    }

    spinlock_unlock(&rr_runqueue_lock);

    return NULL;
}

static int sched_rr_enqueue_task(struct task *task)
{
    RET_IF_FAIL(task, -EINVAL);
    RET_IF_FAIL(sched_rr_get_priority_min() <= task->priority, -EINVAL);
    RET_IF_FAIL(sched_rr_get_priority_max() >= task->priority, -EINVAL);

    spinlock_lock(&rr_runqueue_lock);
    list_add(&rr_runqueue[task->priority], &task->list);
    spinlock_unlock(&rr_runqueue_lock);

    return 0;
}

static int sched_rr_get_interval(struct task *task, struct timespec *interval)
{
    RET_IF_FAIL(task, -EINVAL);
    RET_IF_FAIL(interval, -EINVAL);

    if (HZ == 1) {
        interval->tv_sec = 1;
        interval->tv_nsec = 0;
    } else {
        interval->tv_sec = 0;
        interval->tv_nsec = 1000000000 / HZ;
    }

    return 0;
}

struct sched_policy sched_rr_policy = {
    .init = sched_rr_init,
    .pick_task = sched_rr_pick_task,
    .enqueue_task = sched_rr_enqueue_task,
    .get_priority_min = sched_rr_get_priority_min,
    .get_priority_max = sched_rr_get_priority_max,
    .rr_get_interval = sched_rr_get_interval,
};
