/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __LIB_TIME_H__
#define __LIB_TIME_H__

#include <time.h>

int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif /* __LIB_TIME_H__ */

