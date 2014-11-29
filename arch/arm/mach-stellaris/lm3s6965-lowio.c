/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <stdbool.h>
#include <stdio.h>

#include <asm/hwio.h>

#define UART_BASE   0x4000c000
#define UARTDR      (UART_BASE + 0x0)
#define UARTFR      (UART_BASE + 0x18)

#define UARTFR_RXFE (1 << 4)

void machine_lowputchar(char c)
{
    write32(UARTDR, c);
}

int machine_lowgetchar(bool wait)
{
    if (!wait && read32(UARTFR) & UARTFR_RXFE)
        return EOF;

    while (read32(UARTFR) & UARTFR_RXFE);
    return read32(UARTDR);
}
