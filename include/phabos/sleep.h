/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __LIB_SLEEP_H__
#define __LIB_SLEEP_H__

#include <time.h>

int usleep(useconds_t usec);
int nanosleep(const struct timespec *req, struct timespec *rem);
unsigned int sleep(unsigned int seconds);

#endif /* __LIB_SLEEP_H__ */

