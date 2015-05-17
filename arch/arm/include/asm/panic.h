/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ARM_PANIC_H__
#define __ARM_PANIC_H__

static inline void _panic(void)
{
    asm volatile("b .");
}

#endif /* __ARM_PANIC_H__ */

