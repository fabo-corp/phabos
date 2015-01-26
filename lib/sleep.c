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
