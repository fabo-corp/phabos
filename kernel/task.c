/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/irq.h>
#include <asm/spinlock.h>
#include <phabos/scheduler.h>
#include <phabos/utils.h>
#include <phabos/panic.h>
#include <phabos/syscall.h>
#include <phabos/fs.h>
#include <phabos/hashtable.h>
#include <phabos/string.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>

#define DEFAULT_STACK_ORDER         4
#define DEFAULT_STACK_SIZE          ((1 << DEFAULT_STACK_ORDER) << PAGE_ORDER)

static struct hashtable *task_table;
static struct spinlock task_table_lock = SPINLOCK_INIT(task_table_lock);
static int next_task_id;

void task_init(void)
{
    task_table = hashtable_create_uint();
}

struct task *find_task_by_id(int id)
{
    struct task *task;

    RET_IF_FAIL(id >= 0, NULL);

    spinlock_lock(&task_table_lock);
    task = hashtable_get(task_table, (void*) id);
    spinlock_unlock(&task_table_lock);

    return task;
}

int _getpid(void)
{
    return current->id;
}

void _exit(int code)
{
    // Trying to kill init process
    if (current->id == 0)
        panic("scheduler: trying to exit from idle task.\n");

    task_annihilate(current);
    panic("_exit: reach unreachable...\n");
}

struct task *task_create(const char *name)
{
    struct task *task;

    task = kzalloc(sizeof(*task), MM_KERNEL);
    RET_IF_FAIL(task, NULL);

    task_cond_init(&task->wait_cond);
    mutex_init(&task->wait_mutex);

    list_init(&task->masked_queued_signals);
    list_init(&task->queued_signals);
    list_init(&task->list);
    task->fd = hashtable_create_uint();

    irq_disable();
    task->id = next_task_id++;
    irq_enable();

    task->name = astrcpy(name);
    if (!task->name)
        goto error_alloc_task_name;

    return task;

error_alloc_task_name:
    hashtable_destroy(task->fd);
    kfree(task);

    return NULL;
}

struct task *task_fork(void)
{
    struct task *task = task_create(current->name);
    if (!task)
        return NULL;

    task->sid = current->sid;
    task->pgid = current->pgid;
    task->controlling_terminal = current->controlling_terminal;

    return task;
}

pid_t task_setsid(void)
{
    if (current->pid == current->pgid)
        return -EPERM;

    current->sid = current->pgid = current->pid;
    current->controlling_terminal = NULL;
    return current->pgid;
}

void task_cond_init(struct task_cond* cond)
{
    list_init(&cond->wait_list);
}

void task_cond_wait(struct task_cond* cond, struct mutex *mutex)
{
    mutex_unlock(mutex);
    task_add_to_wait_list(task_get_running(), &cond->wait_list);
    sched_yield();
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
    struct hashtable_iterator iter = HASHTABLE_ITERATOR_INIT;

    // assert
    if (task->allocated_stack)
        page_free(task->allocated_stack, DEFAULT_STACK_ORDER);

    while (hashtable_iterate(task->fd, &iter)) {
        task_free_fdnum(task, (int) iter.key);
    }

    hashtable_destroy(task->fd);
    kfree(task);
}

struct task *task_get_running(void)
{
    return current;
}

void task_add_to_wait_list(struct task *task, struct list_head *wait_list)
{
    sched_rm_from_runqueue(task);

    irq_disable();
    list_add(wait_list, &task->list);
    irq_enable();
}

void task_remove_from_wait_list(struct task *task)
{
    irq_disable();
    list_del(&task->list);
    irq_enable();

    sched_add_to_runqueue(task);
}

struct task *task_run(const char *name, task_entry_t entry, void *data,
                      uint32_t stack_addr)
{
    struct task *task = task_create(name);
    if (!task)
        return NULL;

    if (!stack_addr) {
        task->allocated_stack = page_alloc(MM_KERNEL, DEFAULT_STACK_ORDER);
        if (!task->allocated_stack)
            goto error_stack;

        stack_addr = (uint32_t) task->allocated_stack + DEFAULT_STACK_SIZE;
    }

    task_init_registers(task, entry, data, stack_addr);
    task->state = TASK_RUNNING;

    sched_add_to_runqueue(task);

    return task;
error_stack:
    kfree(task);
    return NULL;
}

void task_annihilate(struct task *task)
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
        panic("task_annihilate: reach unreachable...\n");
    }

    list_del(&task->list);
    task_destroy(task);

    irq_enable();
}

int task_wait(struct task *task)
{
    if (!task)
        return -EINVAL;

    mutex_lock(&task->wait_mutex);
    task_cond_wait(&task->wait_cond, &task->wait_mutex);
    mutex_unlock(&task->wait_mutex);

    return 0;
}

void task_exit(void)
{
    sched_rm_from_runqueue(current);

    task_cond_broadcast(&current->wait_cond);

    kill_task = true;
    sched_yield();
}

void sys_exit(int status)
{
    // Trying to kill init process
    if (current->id == 0)
        panic("sys_exit: trying to exit from idle task.\n");

    task_annihilate(current);
    panic("sys_exit: reach unreachable...\n");
}
DEFINE_SYSCALL(SYS_EXIT, exit, 0);

pid_t sys_getpid(void)
{
    return current->id;
}
DEFINE_SYSCALL(SYS_GETPID, getpid, 0);
