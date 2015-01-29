/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <config.h>
#include <asm/semihosting.h>
#include <stdbool.h>
#include <stdio.h>

void machine_lowputchar(char c);
int machine_lowgetchar(bool wait);

void low_putchar(char c)
{
#ifdef CONFIG_ARM_SEMIHOSTING
    semihosting_putc(c);
#else
    machine_lowputchar(c);
#endif
}

ssize_t low_write(char *buffer, int count)
{
#ifdef CONFIG_ARM_SEMIHOSTING
    return semihosting_write(buffer, count);
#else
    for (int i = 0; i < count; i++)
        low_putchar(buffer[i]);
    return count;
#endif
}

int low_getchar(bool wait)
{
#ifdef CONFIG_ARM_SEMIHOSTING
    char c;

    if (!wait)
        return EOF;

    semihosting_read(&c, 1);
    return c;
#else
    return machine_lowgetchar(wait);
#endif
}
