/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <phabos/workqueue.h>
#include <phabos/utils.h>
#include <phabos/scheduler.h>
#include <phabos/assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void workqueue_thread(void *data)
{
    struct workqueue *wq = data;
    struct work *work;

    RET_IF_FAIL(data,);

    while (1) {
        semaphore_lock(&wq->semaphore);

        list_foreach_safe(&wq->list, iter) {
            work = list_entry(iter, struct work, list);
            if (!work->is_schedulable)
                continue;

            spinlock_lock(&wq->lock);
            list_del(&work->list);
            spinlock_unlock(&wq->lock);

            if (work->entry_point)
                work->entry_point(work->data);

            watchdog_delete(&work->watchdog);
            kfree(work);
            break;
        }

        if (atomic_dec(&wq->work_count) <= 0)
            semaphore_unlock(&wq->empty_semaphore);
    }
}

void workqueue_delay_timeout(struct watchdog *wd)
{
    struct work *work = containerof(wd, struct work, watchdog);

    RET_IF_FAIL(wd,);
    RET_IF_FAIL(work->wq,);

    work->is_schedulable = true;
    semaphore_unlock(&work->wq->semaphore);
}

struct workqueue *workqueue_create(const char *name)
{
    struct workqueue *wq;

    RET_IF_FAIL(name, NULL);

    wq = zalloc(sizeof(*wq));
    RET_IF_FAIL(wq, NULL);

    wq->name = name;

    semaphore_init(&wq->semaphore, 0);
    semaphore_init(&wq->empty_semaphore, 1);
    list_init(&wq->list);
    atomic_init(&wq->work_count, 0);
    spinlock_init(&wq->lock);

    wq->task = task_run(name, workqueue_thread, wq, 0);
    if (!wq->task)
        goto task_run_error;

    return wq;

task_run_error:
    kfree(wq);
    return NULL;
}

void workqueue_destroy(struct workqueue *wq)
{
    struct work *work;

    if (!wq)
        return;

    RET_IF_FAIL(wq->task,);

    task_kill(wq->task);

    list_foreach_safe(&wq->list, iter) {
        work = list_entry(iter, struct work, list);
        list_del(&work->list);
        kfree(work);
    }

    // FIXME
    // destroy will try to free the smeaphore pointer.
    //semaphore_destroy(&wq->semaphore);
    //semaphore_destroy(&wq->empty_semaphore);
    kfree(wq);
}

void workqueue_queue(struct workqueue *wq, work_entry_t entry, void *data)
{
    RET_IF_FAIL(wq,);
    RET_IF_FAIL(entry,);

    workqueue_schedule(wq, entry, data, 0);
}

void workqueue_schedule(struct workqueue *wq, work_entry_t callback,
                        void *data, uint32_t delay)
{
    struct work *work;

    RET_IF_FAIL(wq,);
    RET_IF_FAIL(callback,);

    work = zalloc(sizeof(*work));
    RET_IF_FAIL(work,);

    work->entry_point = callback;
    work->data = data;
    work->is_schedulable = delay ? false : true;
    list_init(&work->list);

    watchdog_init(&work->watchdog);
    work->watchdog.timeout = workqueue_delay_timeout;
    work->watchdog.user_priv = work;
    work->wq = wq;

    if (atomic_inc(&wq->work_count) == 1)
        semaphore_lock(&wq->empty_semaphore);

    spinlock_lock(&wq->lock);
    list_add(&wq->list, &work->list);
    spinlock_unlock(&wq->lock);

    if (delay)
        watchdog_start(&work->watchdog, delay);
    else
        semaphore_unlock(&wq->semaphore);
}

bool workqueue_has_pending_work(struct workqueue *wq)
{
    RET_IF_FAIL(wq, false);
    return !list_is_empty(&wq->list);
}

int workqueue_wait_empty(struct workqueue *wq, int timeout)
{
    RET_IF_FAIL(wq, -EINVAL);

    // TODO implement the timeout

    semaphore_lock(&wq->empty_semaphore);
    semaphore_unlock(&wq->empty_semaphore);
    return 0;
}
