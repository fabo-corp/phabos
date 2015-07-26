/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <stdbool.h>
#include <stdio.h>

#include <asm/hwio.h>

#define STM32_USART1_BASE   0x40011000
#define STM32_USART1_SR     (STM32_USART1_BASE + 0x00)
#define STM32_USART1_DR     (STM32_USART1_BASE + 0x04)

#define STM32_USART1_SR_TXE (1 << 7)

void machine_lowputchar(char c)
{
    while (!(read32(STM32_USART1_SR) & STM32_USART1_SR_TXE));
    write32(STM32_USART1_DR, c);
}
