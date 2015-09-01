#include <errno.h>
#include <sys/signal.h>

#include <asm/scheduler.h>
#include <phabos/task.h>
#include <phabos/syscall.h>
#include <phabos/signal.h>

int _kill(int pid, int sig)
{
    struct task *task = find_task_by_id(pid);
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    task_annihilate(task);

    return 0;
}

static int task_queue_signal(struct task *task, int sig)
{
    struct signal *signal;

    signal = kzalloc(sizeof(*signal), MM_KERNEL);
    if (!signal)
        return -ENOMEM;

    list_init(&signal->list);
    signal->id = sig;

    if (sigismember(&task->masked_interrupts, sig))
        list_add(&task->masked_queued_signals, &signal->list);
    else
        list_add(&task->queued_signals, &signal->list);

    return 0;
}

int sys_kill(pid_t pid, int sig)
{
    struct task *task;

    /* FIXME: process group are not supported right now */
    if (pid <= 0)
        return -ENOSYS;

    task = find_task_by_id(pid);
    if (!task)
        return -ESRCH;

    return task_queue_signal(task, sig);
}
DEFINE_SYSCALL(SYS_KILL, kill, 2);
