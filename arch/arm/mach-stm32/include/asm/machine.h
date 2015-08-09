/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __MACHINE_H__
#define __MACHINE_H__

#define CPU_NUM_IRQ    82
#define VTOR_ALIGNMENT 256

#define HZ              100

#if defined(CONFIG_BOOT_FLASH)
#define LOOP_PER_USEC   25
#define LOOP_PER_MSEC   25000
#define CPU_FREQ        160000000
#else
#define LOOP_PER_USEC   47
#define LOOP_PER_MSEC   47000
#define CPU_FREQ        168000000
#endif

#endif /* __MACHINE_H__ */

