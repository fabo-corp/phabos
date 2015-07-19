/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <phabos/list.h>
#include <phabos/sleep.h>
#include <phabos/semaphore.h>
#include <phabos/watchdog.h>
#include <phabos/assert.h>

#include <errno.h>

extern struct task *current;

static void usleep_timeout(struct watchdog *watchdog)
{
    RET_IF_FAIL(watchdog,);
    RET_IF_FAIL(watchdog->user_priv,);

    semaphore_unlock(watchdog->user_priv);
}

int usleep(useconds_t usec)
{
    struct semaphore semaphore;
    struct watchdog watchdog;

    semaphore_init(&semaphore, 0);

    watchdog_init(&watchdog);
    watchdog.timeout = usleep_timeout;
    watchdog.user_priv = &semaphore;

    watchdog_start(&watchdog, usec);
    semaphore_lock(&semaphore);
    watchdog_delete(&watchdog);

    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    RET_IF_FAIL(req, -EINVAL);

    useconds_t usec = req->tv_sec * 1000000 + req->tv_nsec / 1000;
    if (req->tv_nsec % 1000)
        usec++;

    return usleep(usec);
}

unsigned int sleep(unsigned int seconds)
{
    int retval;
    struct timespec rem_timespec = {0};
    struct timespec sleep_timespec = {
        .tv_sec = seconds,
    };

    retval = nanosleep(&sleep_timespec, &rem_timespec);
    if (retval == -EINTR) {
        return rem_timespec.tv_sec + (rem_timespec.tv_nsec ? 1 : 0);
    }

    if (retval < 0)
        return seconds;
    return 0;
}
