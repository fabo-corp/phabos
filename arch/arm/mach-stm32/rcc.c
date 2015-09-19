/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/hwio.h>

#include "stm32f4xx.h"

static uint32_t get_reg(unsigned peripheral)
{
    return STM32_RCC_BASE + ((peripheral >> 16) & 0xff)
                          + ((peripheral >> 8)  & 0xff);
}

static uint32_t get_mask(unsigned peripheral)
{
    return 1 << (peripheral & 0xff);
}

void stm32_clk_enable(unsigned clock)
{
    uint32_t reg = read32(get_reg(clock));
    write32(get_reg(clock), reg | get_mask(clock));
}

void stm32_clk_disable(unsigned clock)
{
    uint32_t reg = read32(get_reg(clock));
    write32(get_reg(clock), reg & ~get_mask(clock));
}

void stm32_reset(unsigned rst)
{
    uint32_t reg = read32(get_reg(rst));
    write32(get_reg(rst), reg | get_mask(rst));
    write32(get_reg(rst), reg & ~get_mask(rst));
}
