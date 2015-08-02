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

static void sleep_timeout(struct watchdog *watchdog)
{
    RET_IF_FAIL(watchdog,);
    RET_IF_FAIL(watchdog->user_priv,);

    semaphore_unlock(watchdog->user_priv);
}

int usleep(useconds_t usec)
{
    int retval;
    struct timespec rem_timespec = {0};
    struct timespec sleep_timespec = {
        .tv_sec = usec / 1000000,
        .tv_nsec = (usec % 1000000) * 1000,
    };

    retval = nanosleep(&sleep_timespec, &rem_timespec);
    if (retval == -EINTR) {
        return rem_timespec.tv_sec * 1000 + rem_timespec.tv_nsec / 1000 +
                ((rem_timespec.tv_nsec % 1000) ? 1 : 0);
    }

    if (retval < 0)
        return usec;
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    struct semaphore semaphore;
    struct watchdog watchdog;

    RET_IF_FAIL(req, -EINVAL);

    semaphore_init(&semaphore, 0);

    watchdog_init(&watchdog);
    watchdog.timeout = sleep_timeout;
    watchdog.user_priv = &semaphore;

    watchdog_start_sec(&watchdog, req->tv_sec);
    semaphore_lock(&semaphore);

    watchdog_start_nsec(&watchdog, req->tv_nsec);
    semaphore_lock(&semaphore);

    watchdog_delete(&watchdog);

    return 0;
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
