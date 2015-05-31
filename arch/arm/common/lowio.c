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
#ifdef CONFIG_ARM_SEMIHOSTING_LOWIO
    semihosting_putc(c);
#else
    machine_lowputchar(c);
#endif
}

ssize_t low_write(char *buffer, int count)
{
#ifdef CONFIG_ARM_SEMIHOSTING_LOWIO
    return semihosting_write_stdout(buffer, count);
#else
    for (int i = 0; i < count; i++)
        low_putchar(buffer[i]);
    return count;
#endif
}
