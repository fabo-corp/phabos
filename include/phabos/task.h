#ifndef __TASK_H__
#define __TASK_H__

#include <config.h>
#include <phabos/list.h>
#include <phabos/mutex.h>

#include <sys/signal.h>

extern bool kill_task;

struct task_cond {
    struct list_head wait_list;
};

#define TASK_RUNNING                    (1 << 1)

struct hashtable;
struct tty_device;

typedef int (*interrupt_lock_cb)(struct task *task, void *lock);

struct task {
    const char *name;

    int id;

    int priority;
    uint16_t state;
    struct hashtable *fd;

    register_t registers[MAX_REG];
    void *allocated_stack;

    struct task_cond wait_cond;
    struct mutex wait_mutex;

    struct sched_policy *policy;
    struct list_head list;

    struct list_head queued_signals;
    struct list_head masked_queued_signals;
    sigset_t masked_interrupts;

    interrupt_lock_cb unlock;
    void *lock_handle;
    bool lock_interrupted;
};

typedef void (*task_entry_t)(void *data);

struct task *task_create(const char *name);

void task_init(void);
struct task *find_task_by_id(int id);

/**
 * Stop and destroy a task
 *
 * A task can call this to kill another task or to kill itself
 *
 * tid: task ID
 */
void task_annihilate(struct task *task);

int task_wait(struct task *task);
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
struct task *task_run(const char *name, task_entry_t task, void *data,
                      uint32_t stack_addr);

/**
 * Get the task ID of the running task
 */
struct task *task_get_running(void);

void task_cond_init(struct task_cond* cond);
void task_cond_wait(struct task_cond* cond, struct mutex *mutex);
void task_cond_signal(struct task_cond* cond);
void task_cond_broadcast(struct task_cond* cond);

#endif /* __TASK_H__ */

