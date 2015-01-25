/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

struct watchdog {
    void (*timeout)(struct watchdog *wd);
};

static inline void watchdog_start(struct watchdog *wd, unsigned long timeout)
{

}

static inline void watchdog_cancel(struct watchdog *wd)
{
}

static inline void watchdog_init(struct watchdog *wd)
{
}

static inline void watchdog_delete(struct watchdog *wd)
{
}

#endif /* __WATCHDOG_H__ */

