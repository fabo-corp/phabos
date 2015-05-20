/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stdint.h>

#include <asm/scheduler.h>
#include <phabos/list.h>
#include <phabos/mutex.h>
#include <phabos/hashtable.h>

extern struct task *current;
extern bool kill_task;

#define TASK_RUNNING                    (1 << 1)

struct task {
    int id;
    uint16_t state;
    hashtable_t fd;

    register_t registers[MAX_REG];
    void *allocated_stack;

    struct list_head list;
};

struct task_cond {
    struct list_head wait_list;
};

typedef void (*task_entry_t)(void *data);

struct task *task_create(void);

/**
 * Initialize the scheduler
 *
 * Must be called before using any of the scheduler functions AND before
 * activating the Systick
 */
void scheduler_init(void);
void scheduler_add_to_runqueue(struct task *task);

/**
 * Call the scheduler to let another task run
 */
void task_yield(void);

/**
 * Stop and destroy a task
 *
 * A task can call this to kill another task or to kill itself
 *
 * tid: task ID
 */
void task_kill(struct task *task);

void task_exit(void);
void task_add_to_wait_list(struct task *task, struct list_head *wait_list);
void task_remove_from_wait_list(struct task *task);

/**
 * Run a new task
 *
 * Add a new task to the scheduler runqueue and returns its task id.
 * The new task will not preempt the current and will have to wait to be chosen
 * by the scheduler to be run.
 *
 * task: Pointer to the new task
 * data: data shared with the new task
 * stack_addr: top of the stack for the task
 */
struct task *task_run(task_entry_t task, void *data, uint32_t stack_addr);

/**
 * Get the task ID of the running task
 */
struct task *task_get_running(void);

void sched_lock(void);
void sched_unlock(void);

void task_cond_wait(struct task_cond* cond, struct mutex *mutex);
void task_cond_signal(struct task_cond* cond);
void task_cond_broadcast(struct task_cond* cond);

#endif /* __SCHEDULER_H__ */

