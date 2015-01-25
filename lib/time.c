/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <errno.h>
#include <time.h>

#include <phabos/time.h>
#include <phabos/assert.h>
#include <asm/scheduler.h>
#include <asm/machine.h>

static int clock_monotonic_gettime(struct timespec *tp)
{
    uint32_t ticks = get_ticks(); // FIXME uint64_t

    RET_IF_FAIL(tp, -EINVAL);

    tp->tv_sec = ticks / HZ;
    tp->tv_nsec = ticks % HZ;

    return 0;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    RET_IF_FAIL(tp, -EINVAL);

    if (clk_id == CLOCK_MONOTONIC)
        return clock_monotonic_gettime(tp);
    return -EINVAL;
}
