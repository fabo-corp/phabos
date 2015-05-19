/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __WORKQUEUE_H__
#define __WORKQUEUE_H__

#include <asm/spinlock.h>
#include <phabos/semaphore.h>
#include <phabos/list.h>
#include <phabos/watchdog.h>

typedef void (*work_entry_t)(void *data);

struct workqueue {
    struct task *task;
    const char *name;
    struct list_head list;
    struct semaphore semaphore;
    struct semaphore empty_semaphore;
    struct spinlock lock;
    atomic_t work_count;
};

struct work {
    work_entry_t entry_point;
    void *data;
    bool is_schedulable;

    struct watchdog watchdog;
    struct workqueue *wq;
    struct list_head list;
};

struct workqueue *workqueue_create(const char *name);
void workqueue_destroy(struct workqueue *wq);
void workqueue_queue(struct workqueue *wq, work_entry_t callback, void *data);
void workqueue_schedule(struct workqueue *wq, work_entry_t callback,
                        void *data, uint32_t delay);
bool workqueue_has_pending_work(struct workqueue *wq);
int workqueue_wait_empty(struct workqueue *wq, int timeout);

#endif /* __WORKQUEUE_H__ */

