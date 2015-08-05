/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ARM_DELAY_H__
#define __ARM_DELAY_H__

#include <asm/machine.h>

static inline void udelay(unsigned long usecs)
{
    const unsigned long loop = usecs / USEC_PER_LOOP + 1;
    for (int i = 0; i < loop; i++)
        asm volatile("nop");
}

static inline void mdelay(unsigned long msecs)
{
    udelay(msecs * 1000);
}


#endif /* __ARM_DELAY_H__ */

