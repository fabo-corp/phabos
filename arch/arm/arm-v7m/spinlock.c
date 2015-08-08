/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/spinlock.h>
#include <asm/irq.h>

struct spinlock *spinlock_create(void)
{
    return (struct spinlock*) ~0;
}

void spinlock_init(struct spinlock *spinlock)
{
}

void _spinlock_lock(struct spinlock *spinlock)
{
    irq_disable();
}

void spinlock_unlock(struct spinlock *spinlock)
{
    irq_enable();
}

void spinlock_destroy(struct spinlock *spinlock)
{
}

bool _spinlock_trylock(struct spinlock *spinlock)
{
    irq_disable();
    return true;
}

