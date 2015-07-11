/*
 * Copyright (c) 2014-2015 Google Inc.
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
 */

#include "scm.h"
#include "chip.h"

#include <asm/delay.h>
#include <asm/hwio.h>
#include <asm/tsb-irq.h>
#include <phabos/driver.h>
#include <phabos/mm.h>
#include <phabos/gpio.h>
#include <phabos/serial/uart16550.h>
#include <phabos/usb/hcd-dwc2.h>

#define UART_RBR_THR_DLL            (UART_BASE + 0x0)
#define UART_IER_DLH                (UART_BASE + 0x4)
#define UART_FCR_IIR                (UART_BASE + 0x8)
#define UART_LCR                    (UART_BASE + 0xc)

#define UART_115200_BPS             26
#define UART_DLL_115200             ((UART_115200_BPS >> 0) & 0xff)
#define UART_DLH_115200             ((UART_115200_BPS >> 8) & 0xff)
#define UART_LCR_DLAB               (1 << 7)
#define UART_LCR_DLS_8              3

#define UART_FCR_IIR_IID0_FIFOE     (1 << 0)
#define UART_FCR_IIR_IID1_RFIFOR    (1 << 1)
#define UART_FCR_IIR_IID1_XFIFOR    (1 << 2)

extern struct driver uart16550_driver;

#define HUB_LINE_N_RESET                    0
#define HUB_RESET_ASSERTION_TIME_IN_USEC    5 /* us */
#define HUB_RESET_DEASSERTION_TIME_IN_MSEC  1 /* ms */

#define TSB_SYSCTL_USBOTG_HSIC_CONTROL  (SYSCTL_BASE + 0x500)
#define TSB_HSIC_DPPULLDOWN             (1 << 0)
#define TSB_HSIC_DMPULLDOWN             (1 << 1)

static int tsb_hcd_power_on(struct device *device)
{
    gpio_activate(HUB_LINE_N_RESET);

    gpio_direction_out(HUB_LINE_N_RESET, 0);
    udelay(HUB_RESET_ASSERTION_TIME_IN_USEC);
    mdelay(1000);
    mdelay(HUB_RESET_DEASSERTION_TIME_IN_MSEC);
//    gpio_direction_out(HUB_LINE_N_RESET, 1);

    write32(TSB_SYSCTL_USBOTG_HSIC_CONTROL,
            TSB_HSIC_DPPULLDOWN | TSB_HSIC_DMPULLDOWN);

    tsb_clk_enable(TSB_CLK_HSIC480);
    tsb_clk_enable(TSB_CLK_HSICBUS);
    tsb_clk_enable(TSB_CLK_HSICREF);

    tsb_reset(TSB_RST_HSIC);
    tsb_reset(TSB_RST_HSICPHY);
    tsb_reset(TSB_RST_HSICPOR);

    tsb_clr_pinshare(TSB_PIN_UART_CTSRTS);

    return 0;
}

static int tsb_hcd_power_off(struct device *device)
{
    tsb_clk_disable(TSB_CLK_HSIC480);
    tsb_clk_disable(TSB_CLK_HSICBUS);
    tsb_clk_disable(TSB_CLK_HSICREF);

    gpio_direction_out(HUB_LINE_N_RESET, 0);
    gpio_deactivate(HUB_LINE_N_RESET);

    return 0;
}

static struct uart16550_device uart16550_device = {
    .base = (void*) UART_BASE,
    .irq = TSB_IRQ_UART,

    .device = {
        .name = "dw_apb_uart",
        .description = "Designware UART16550 compatible UART",
        .driver = "uart16550",
    },
};

static struct usb_hcd usb_hcd_device = {
    .has_hsic_phy = true,

    .device = {
        .name = "dw_usb2_hcd",
        .description = "Designware USB 2.0 Host Controller Driver",
        .driver = "dw-usb2-hcd",

        .reg_base = HSIC_BASE,
        .irq = TSB_IRQ_HSIC,

        .power_on = tsb_hcd_power_on,
        .power_off = tsb_hcd_power_off,
    },
};

static struct gpio_device gpio_device = {
#if defined(CONFIG_TSB_ES1)
    .count = 16,
#else
    .count = 27,
#endif

    .device = {
        .name = "tsb_gpio",
        .description = "Toshiba Bridges GPIO controller",
        .driver = "tsb-gpio",
    },
};

void tsb_uart_init(void)
{
    tsb_set_pinshare(TSB_PIN_UART_RXTX | TSB_PIN_UART_CTSRTS);

    tsb_clk_enable(TSB_CLK_UARTP);
    tsb_clk_enable(TSB_CLK_UARTS);

    tsb_reset(TSB_RST_UARTP);
    tsb_reset(TSB_RST_UARTS);

    for (int i = 0; i < 3; i++) {
        write32(UART_LCR, UART_LCR_DLAB | UART_LCR_DLS_8);
        write32(UART_RBR_THR_DLL, UART_DLL_115200);
        write32(UART_IER_DLH, UART_DLH_115200);
        write32(UART_LCR, UART_LCR_DLS_8);
        write32(UART_IER_DLH, 1);
        write32(UART_FCR_IIR, UART_FCR_IIR_IID0_FIFOE |
                UART_FCR_IIR_IID1_RFIFOR | UART_FCR_IIR_IID1_XFIFOR);
    }
}

void machine_init(void)
{
    int order = size_to_order(BUFRAM_SIZE);

    mm_add_region(BUFRAM0_BASE, order, MM_DMA);
    mm_add_region(BUFRAM1_BASE, order, MM_DMA);
    mm_add_region(BUFRAM2_BASE, order, MM_DMA);
    mm_add_region(BUFRAM3_BASE, order, MM_DMA);

    tsb_uart_init();

    device_register(&gpio_device.device);
    device_register(&uart16550_device.device);
    device_register(&usb_hcd_device.device);
}
