/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stdint.h>
#include <time.h>

#include <asm/machine.h>
#include <asm/scheduler.h>
#include <phabos/task.h>

extern struct task *current;

struct sched_policy {
    int (*init)(void);

    struct task *(*pick_task)(void);
    int (*enqueue_task)(struct task *task);
    int (*dequeue_task)(struct task *task);

    int (*get_priority_min)(void);
    int (*get_priority_max)(void);

    int (*rr_get_interval)(struct task *task, struct timespec *interval);
};

/**
 * Initialize the scheduler
 *
 * Must be called before using any of the scheduler functions AND before
 * activating the Systick
 */
void sched_init(void);
int sched_add_to_runqueue(struct task *task);
int sched_rm_from_runqueue(struct task *task);

/**
 * Call the scheduler to let another task run
 */
void sched_yield(void);

void sched_lock(void);
void sched_unlock(void);

#endif /* __SCHEDULER_H__ */

