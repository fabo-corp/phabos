/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ARM_SEMIHOSTING_H__
#define __ARM_SEMIHOSTING_H__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <phabos/driver.h>

static inline ssize_t semihosting_write_stdout(const char *buffer,
                                               size_t buflen)
{
    ssize_t semihosting_write(struct file*, const void*, size_t);
    return semihosting_write(NULL, buffer, buflen);
}

void semihosting_putc(char c);

#endif /* __ARM_SEMIHOSTING_H__ */

