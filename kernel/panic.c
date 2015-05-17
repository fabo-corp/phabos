/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/panic.h>
#include <phabos/scheduler.h>
#include <phabos/kprintf.h>

void panic(const char *msg)
{
    sched_lock();

    kprintf("PANIC: %s\n", msg ? msg : "");
    _panic();
}
