/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include <asm/machine.h>

#define ONE_SEC_IN_NSEC 1000000000
#define ONE_SEC_IN_USEC 1000000
#define ONE_SEC_IN_MSEC 1000

#define TICK_IN_MSEC (ONE_SEC_IN_MSEC / HZ)
#define TICK_IN_USEC (ONE_SEC_IN_USEC / HZ)
#define TICK_IN_NSEC (ONE_SEC_IN_NSEC / HZ)

struct watchdog {
    void (*timeout)(struct watchdog *wd);
    void *priv;
    void *user_priv;
};

void watchdog_start(struct watchdog *wd, unsigned long ticks);
void watchdog_cancel(struct watchdog *wd);
void watchdog_init(struct watchdog *wd);
void watchdog_delete(struct watchdog *wd);
bool watchdog_has_expired(struct watchdog *wd);
bool watchdog_is_active(struct watchdog *wd);

uint64_t watchdog_get_ticks_until_next_expiration(void);

static inline void watchdog_start_nsec(struct watchdog *wd, unsigned long nsec)
{
    unsigned long ticks = (nsec / TICK_IN_NSEC) +
                          ((nsec % TICK_IN_NSEC) ? 1 : 0);
    watchdog_start(wd, ticks ? ticks : 1);
}

static inline void watchdog_start_usec(struct watchdog *wd, unsigned long usec)
{
    unsigned long ticks = (usec / TICK_IN_USEC) +
                          ((usec % TICK_IN_USEC) ? 1 : 0);
    watchdog_start(wd, ticks ? ticks : 1);
}

static inline void watchdog_start_msec(struct watchdog *wd, unsigned long msec)
{
    unsigned long ticks = (msec / TICK_IN_MSEC) +
                          ((msec % TICK_IN_MSEC) ? 1 : 0);
    watchdog_start(wd, ticks ? ticks : 1);
}

static inline void watchdog_start_sec(struct watchdog *wd, unsigned long sec)
{
    unsigned long ticks = sec * HZ;
    watchdog_start(wd, ticks ? ticks : 1);
}

#endif /* __WATCHDOG_H__ */

