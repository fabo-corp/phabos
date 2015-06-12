/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <string.h>

#include <phabos/scheduler.h>
#include <phabos/panic.h>
#include <asm/scheduler.h>
#include <asm/atomic.h>

static struct list_head runqueue = LIST_INIT(runqueue);
struct task *current;
bool need_resched;
bool kill_task;
static atomic_t is_locked;

void scheduler_init(void)
{
    struct task *task;

    task = task_create();
    if (!task)
        panic("scheduler: cannot allocate memory.\n");

    task->state = TASK_RUNNING;

    list_add(&runqueue, &task->list);

    atomic_init(&is_locked, 0);

    current = task;
    need_resched = false;

    scheduler_arch_init();
}

void scheduler_add_to_runqueue(struct task *task)
{
    // FIXME add spinlock here
    irq_disable();
    list_add(&runqueue, &task->list);
    irq_enable();
}

void schedule(uint32_t *stack_top)
{
    struct task *current_saved = current;

    if (atomic_get(&is_locked))
        return;

    if (list_is_empty(&runqueue))
        panic("scheduler: no idle task to run\n");

    memcpy(&current->registers, stack_top, sizeof(current->registers));

    list_rotate_anticlockwise(&runqueue);
    current = list_first_entry(&runqueue, struct task, list);
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
