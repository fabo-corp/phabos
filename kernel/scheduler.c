/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <string.h>
#include <errno.h>

#include <config.h>
#include <phabos/scheduler.h>
#include <phabos/panic.h>
#include <phabos/utils.h>
#include <phabos/assert.h>
#include <asm/scheduler.h>
#include <asm/atomic.h>

struct task *current;
bool need_resched;
bool kill_task;
static atomic_t is_locked;
static struct task *idle_task;

extern struct sched_policy sched_rr_policy;
extern struct sched_policy sched_fifo_policy;

static struct sched_policy *policies[] = {
#if defined(CONFIG_SCHED_FIFO)
    &sched_fifo_policy,
#endif

#if defined(CONFIG_SCHED_RR)
    &sched_rr_policy,
#endif
};

void sched_init(void)
{
    struct task *task;

    task = task_create();
    if (!task)
        panic("scheduler: cannot allocate memory.\n");

    task->state = TASK_RUNNING;
    atomic_init(&is_locked, 0);
    current = idle_task = task;
    need_resched = false;
    kill_task = false;

    for (int i = 0; i < ARRAY_SIZE(policies); i++) {
        if (policies[i]->init)
            policies[i]->init();
    }

    scheduler_arch_init();
}

int sched_add_to_runqueue(struct task *task)
{
    int retval;

    RET_IF_FAIL(task, -EINVAL);

    if (!task->policy)
        task->policy = &sched_rr_policy;

    RET_IF_FAIL(task->policy, -EINVAL);
    RET_IF_FAIL(task->policy->enqueue_task, -EINVAL);

    task->state = TASK_RUNNING;
    retval = task->policy->enqueue_task(task);
    if (retval)
        return retval;

    sched_set_tick_multiplier(1);
    return 0;
}

int sched_rm_from_runqueue(struct task *task)
{
    RET_IF_FAIL(task, -EINVAL);
    RET_IF_FAIL(task->policy, -EINVAL);
    RET_IF_FAIL(task->policy->dequeue_task, -EINVAL);

    task->state &= ~TASK_RUNNING;
    return task->policy->dequeue_task(task);
}

void schedule(uint32_t *stack_top)
{
    struct task *new_task;
    struct task *current_saved = current;

    if (atomic_get(&is_locked))
        return;

    memcpy(&current->registers, stack_top, sizeof(current->registers));

    current = NULL;
    for (int i = 0; i < ARRAY_SIZE(policies); i++) {
        if (!policies[i])
            continue;

        new_task = policies[i]->pick_task();
        if (!new_task)
            continue;

        if (!current || new_task->priority > current->priority)
            current = new_task;
    }

    if (!current) {
        if (idle_task->state != TASK_RUNNING)
            panic("idle task is not runnable...\n");
        current = idle_task;
    }

    need_resched = false;

    memcpy((void*) (current->registers[SP_REG] - 4),
           &current->registers, sizeof(current->registers));

    if (kill_task) {
        kill_task = false;
        task_kill(current_saved);
    }
}

void sched_lock(void)
{
    atomic_inc(&is_locked);
}

void sched_unlock(void)
{
    atomic_dec(&is_locked);
}
