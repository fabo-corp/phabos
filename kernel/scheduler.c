/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include <phabos/scheduler.h>
#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/panic.h>
#include <asm/scheduler.h>
#include <asm/irq.h>
#include <asm/atomic.h>

#define TASK_RUNNING                    (1 << 1)
#define DEFAULT_STACK_SIZE              4096

static struct list_head runqueue = LIST_INIT(runqueue);
struct task *current;
bool need_resched;
static bool kill_task;
static atomic_t is_locked;
static int next_task_id;

static struct task *task_create(void)
{
    struct task *task;

    task = zalloc(sizeof(*task));
    RET_IF_FAIL(task, NULL);

    list_init(&task->list);

    irq_disable();
    task->id = next_task_id++;
    irq_enable();

    return task;
}

void task_cond_wait(struct task_cond* cond, struct mutex *mutex)
{
    mutex_unlock(mutex);
    task_add_to_wait_list(task_get_running(), &cond->wait_list);
    mutex_lock(mutex);
}

void task_cond_signal(struct task_cond* cond)
{
    task_remove_from_wait_list(list_first_entry(&cond->wait_list,
                                                struct task, list));
}

void task_cond_broadcast(struct task_cond* cond)
{
    list_foreach_safe(&cond->wait_list, iter)
        task_remove_from_wait_list(list_entry(iter, struct task, list));
}

static void task_destroy(struct task *task)
{
    // assert
    if (task->allocated_stack)
        free(task->allocated_stack);
    free(task);
}

struct task *task_get_running(void)
{
    return current;
}

void task_add_to_wait_list(struct task *task, struct list_head *wait_list)
{
    irq_disable();

    if (task->id == 0)
        panic("PANIC: Trying to remove idle task from runqueue\n");

    list_del(&task->list);
    list_add(wait_list, &task->list);
    task->state &= ~TASK_RUNNING;

    irq_enable();
}

void task_remove_from_wait_list(struct task *task)
{
    irq_disable();

    list_del(&task->list);
    list_add(&runqueue, &task->list);
    task->state |= TASK_RUNNING;

    irq_enable();
}

struct task *task_run(task_entry_t entry, void *data, uint32_t stack_addr)
{
    struct task *task = task_create();
    if (!task)
        return NULL;

    if (!stack_addr) {
        task->allocated_stack = malloc(DEFAULT_STACK_SIZE);
        if (!task->allocated_stack)
            goto error_stack;

        stack_addr = (uint32_t) task->allocated_stack + DEFAULT_STACK_SIZE;
    }

    task_init_registers(task, entry, data, stack_addr);
    task->state = TASK_RUNNING;

    irq_disable();
    list_add(&runqueue, &task->list);
    irq_enable();

    return task;
error_stack:
    free(task);
    return NULL;
}

void task_kill(struct task *task)
{
    if (task->id == 0) {
        kprintf("Oops: trying to kill idle task...\n");
        return;
    }

    irq_disable();
    if (current == task) {
        irq_enable(); // FIXME: force enable the interrupts in order for
                      // PendSV to work
        task_exit();
        panic("scheduler: reach unreachable...\n");
    }

    list_del(&task->list);
    task_destroy(task);

    irq_enable();
}

void task_exit(void)
{
    kill_task = true;
    task_yield();
}

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

static struct task *find_task_by_id(int id)
{
    struct task *task;

    irq_disable();
    list_foreach(&runqueue, iter) {
        task = list_entry(iter, struct task, list);
        if (id == task->id)
            break;
        task = NULL;
    }
    irq_enable();

    return task;
}

int _getpid(void)
{
    return current->id;
}

int _kill(int pid, int sig)
{
    struct task *task = find_task_by_id(pid);
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    task_kill(task);

    return 0;
}

void _exit(int code)
{
    // Trying to kill init process
    if (current->id == 0)
        panic("scheduler: trying to exit from idle task.\n");

    task_kill(current);
    panic("scheduler: reach unreachable...\n");
}
