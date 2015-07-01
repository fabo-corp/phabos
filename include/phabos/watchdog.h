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
    void *priv;
    void *user_priv;
};

void watchdog_start(struct watchdog *wd, unsigned long timeout);
void watchdog_cancel(struct watchdog *wd);
void watchdog_init(struct watchdog *wd);
void watchdog_delete(struct watchdog *wd);
bool watchdog_has_expired(struct watchdog *wd);

uint64_t watchdog_get_ticks_until_next_expiration(void);

#endif /* __WATCHDOG_H__ */

