#include <errno.h>
#include <sys/signal.h>

#include <asm/scheduler.h>
#include <phabos/task.h>
#include <phabos/syscall.h>

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

int sys_kill(pid_t pid, int sig)
{
    struct task *task;

    if (pid <= 0) {
        return -ENOSYS;
    }

    task = find_task_by_id(pid);

    if (!task)
        return -ESRCH;

    switch (sig) {
    case 0:
        break;

    case SIGKILL:
        task_annihilate(task);
        break;

    default:
        return -ENOSYS;
    }

    return 0;
}
DEFINE_SYSCALL(SYS_KILL, kill, 2);
