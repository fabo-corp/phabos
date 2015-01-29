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

ssize_t semihosting_read(char *buffer, size_t buflen);
ssize_t semihosting_write(const char *buffer, size_t buflen);
void semihosting_putc(char c);
void semihosting_init(void);

#endif /* __ARM_SEMIHOSTING_H__ */

