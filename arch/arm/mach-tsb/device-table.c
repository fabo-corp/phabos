/**
 * Copyright (c) 2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Mark Greer
 */

#include <config.h>

#include <phabos/utils.h>
#include <phabos/ara/device.h>
#include <phabos/ara/device_resource.h>
#include <phabos/ara/device_table.h>
#include <phabos/ara/device_pll.h>
#include <phabos/ara/device_i2s.h>

#include "chip.h"
#include <asm/tsb-irq.h>

#ifdef CONFIG_TSB_PLL
#define TSB_PLLA_CG_BRIDGE_OFFSET    0x900
#define TSB_PLLA_SIZE                0x20

static struct device_resource tsb_plla_resources[] = {
    {
        .name   = "reg_base",
        .type   = DEVICE_RESOURCE_TYPE_REGS,
        .start  = SYSCTL_BASE + TSB_PLLA_CG_BRIDGE_OFFSET,
        .count  = TSB_PLLA_SIZE,
    },
};
#endif

#ifdef CONFIG_TSB_I2S
static struct device_resource tsb_i2s_resources_0[] = {
    {
        .name   = "cg_bridge",
        .type   = DEVICE_RESOURCE_TYPE_REGS,
        .start  = SYSCTL_BASE,
        .count  = SYSCTL_SIZE,
    },
    {
        .name   = "i2slp_sc",
        .type   = DEVICE_RESOURCE_TYPE_REGS,
        .start  = I2SLP_SC_BASE,
        .count  = I2SLP_SC_SIZE,
    },
    {
        .name   = "i2slp_so",
        .type   = DEVICE_RESOURCE_TYPE_REGS,
        .start  = I2SLP_SO_BASE,
        .count  = I2SLP_SO_SIZE,
    },
    {
        .name   = "i2slp_si",
        .type   = DEVICE_RESOURCE_TYPE_REGS,
        .start  = I2SLP_SI_BASE,
        .count  = I2SLP_SI_SIZE,
    },
    {
        .name   = "i2soerr",
        .type   = DEVICE_RESOURCE_TYPE_IRQ,
        .start  = TSB_IRQ_I2SOERR,
        .count  = 1,
    },
    {
        .name   = "i2so",
        .type   = DEVICE_RESOURCE_TYPE_IRQ,
        .start  = TSB_IRQ_I2SO,
        .count  = 1,
    },
    {
        .name   = "i2sierr",
        .type   = DEVICE_RESOURCE_TYPE_IRQ,
        .start  = TSB_IRQ_I2SIERR,
        .count  = 1,
    },
    {
        .name   = "i2si",
        .type   = DEVICE_RESOURCE_TYPE_IRQ,
        .start  = TSB_IRQ_I2SI,
        .count  = 1,
    },
};
#endif

static struct device_ara tsb_devices[] = {
#ifdef CONFIG_TSB_PLL
    {
        .type           = DEVICE_TYPE_PLL_HW,
        .name           = "tsb_pll",
        .desc           = "TSB PLLA Controller",
        .id             = 0,
        .resources      = tsb_plla_resources,
        .resource_count = ARRAY_SIZE(tsb_plla_resources),
    },
#endif
#ifdef CONFIG_TSB_I2S
    {
        .type           = DEVICE_TYPE_I2S_HW,
        .name           = "tsb_i2s",
        .desc           = "TSB I2S Controller",
        .id             = 0,
        .resources      = tsb_i2s_resources_0,
        .resource_count = ARRAY_SIZE(tsb_i2s_resources_0),
    },
#endif
};

static struct device_table tsb_device_table = {
    .device = tsb_devices,
    .device_count = ARRAY_SIZE(tsb_devices),
};

int tsb_device_table_register(void)
{
    return device_table_register(&tsb_device_table);
}
