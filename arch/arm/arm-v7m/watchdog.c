/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <config.h>
#include <asm/spinlock.h>
#include <asm/machine.h>
#include <asm/scheduler.h>
#include <phabos/list.h>
#include <phabos/watchdog.h>
#include <phabos/mm.h>

static struct list_head wdog_head = LIST_INIT(wdog_head);
static struct spinlock wdog_lock = SPINLOCK_INIT(wdog_lock);

struct watchdog_priv {
    struct watchdog *wd;
    struct list_head list;
    uint64_t start;
    uint64_t end;
};

#define to_watchdog_priv(x) ((struct watchdog_priv*) wd->priv)

bool watchdog_has_expired(struct watchdog *wd)
{
    assert(wd);
    assert(wd->priv);

    struct watchdog_priv *wdog = to_watchdog_priv(wd);

    uint64_t ticks = get_ticks();
    if (wdog->end > wdog->start) {
        return ticks >= wdog->end;
    } else {
        return ticks >= wdog->end && ticks < wdog->start;
    }

    return false;
}

static int watchdog_expiration_comparator(struct list_head *node1,
                                          struct list_head *node2)
{
    struct watchdog_priv *wdog1 = list_entry(node1, struct watchdog_priv, list);
    struct watchdog_priv *wdog2 = list_entry(node2, struct watchdog_priv, list);
    return wdog1->end - wdog2->end;
}

uint64_t watchdog_get_ticks_until_next_expiration(void)
{
    if (list_is_empty(&wdog_head))
        return ~0ull;

    uint64_t ticks = get_ticks();

    spinlock_lock(&wdog_lock);
    struct watchdog_priv *wdog =
        list_first_entry(&wdog_head, struct watchdog_priv, list);
    spinlock_unlock(&wdog_lock);

    return wdog->end - ticks;
}

/**
 * Executed from the SYSTICK interrupt
 */
void watchdog_check_expired(void)
{
    list_foreach_safe(&wdog_head, iter) {
        struct watchdog_priv *wdog =
            list_entry(iter, struct watchdog_priv, list);
        struct watchdog *wd = wdog->wd;

        if (!watchdog_has_expired(wd))
            break;;

        watchdog_cancel(wd);
        wd->timeout(wd); // FIXME call from a thread
    }
}

void watchdog_start(struct watchdog *wd, unsigned long usec)
{
    assert(wd);
    assert(wd->priv);
    assert(usec > 0);

    uint64_t ticks = get_ticks();
    struct watchdog_priv *wdog = to_watchdog_priv(wd);

    wdog->start = ticks;
#define ONE_SEC_IN_USEC 1000000
    wdog->end = ticks + (usec / ONE_SEC_IN_USEC) * HZ;
    if (wdog->end == ticks)
        wdog->end = ticks + 1;

    spinlock_lock(&wdog_lock);
    list_sorted_add(&wdog_head, &wdog->list, watchdog_expiration_comparator);

#if defined(CONFIG_TICKLESS)
    uint64_t ticks_until_first_expiration;
    ticks_until_first_expiration = watchdog_get_ticks_until_next_expiration();
    if (ticks_until_first_expiration < sched_get_tick_multiplier())
        sched_set_tick_multiplier(ticks_until_first_expiration);
#endif

    spinlock_unlock(&wdog_lock);
}

void watchdog_cancel(struct watchdog *wd)
{
    assert(wd);
    assert(wd->priv);

    struct watchdog_priv *wdog = to_watchdog_priv(wd);

    spinlock_lock(&wdog_lock);
    if (!list_is_empty(&wdog->list))
        list_del(&wdog->list);
    spinlock_unlock(&wdog_lock);
}

void watchdog_init(struct watchdog *wd)
{
    struct watchdog_priv *wdog;

    assert(wd);
    memset(wd, 0, sizeof(*wd));
    wd->priv = wdog = kmalloc(sizeof(*wdog), 0);
    list_init(&wdog->list);
    wdog->wd = wd;
}

void watchdog_delete(struct watchdog *wd)
{
    if (!wd)
        return;

    assert(wd->priv);

    watchdog_cancel(wd);
    kfree(wd->priv);
}
