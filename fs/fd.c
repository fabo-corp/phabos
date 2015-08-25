#include <phabos/assert.h>
#include <phabos/fs.h>
#include <phabos/utils.h>
#include <phabos/scheduler.h>
#include <phabos/hashtable.h>

#include <errno.h>

#define TASK_FD_MAX 20

struct fd *to_fd(int fdnum)
{
    struct task *task;

    if (fdnum < 0)
        return NULL;

    task = task_get_running();
    RET_IF_FAIL(task, NULL);

    return hashtable_get(task->fd, (void*) fdnum);
}

int allocate_fdnum(void)
{
    struct task *task;
    struct fd *fd;

    task = task_get_running();
    RET_IF_FAIL(task, -1);

    for (int i = 0; i < TASK_FD_MAX; i++) {
        fd = hashtable_get(task->fd, (void*) i);
        if (fd)
            continue;

        fd = zalloc(sizeof(*fd));
        if (!fd)
            return -ENOMEM;

        hashtable_add(task->fd, (void*) i, fd);
        return i;
    }

    return -1;
}

int task_free_fdnum(struct task *task, int fdnum)
{
    struct fd *fd;

    fd = hashtable_get(task->fd, (void*) fdnum);
    if (!fd)
        return -EBADF;

    if (is_directory(fd->file->inode))
        mutex_unlock(&fd->file->inode->dlock); // FIXME safety checks
    hashtable_remove(task->fd, (void*) fdnum);

    kfree(fd->file);
    kfree(fd);

    return 0;
}

int free_fdnum(int fdnum)
{
    struct task *task;

    task = task_get_running();
    RET_IF_FAIL(task,-1);

    return task_free_fdnum(task, fdnum);
}
