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
#include <phabos/i2c.h>
#include <phabos/serial/uart16550.h>
#include <phabos/usb/hcd-dwc2.h>
#include <phabos/greybus.h>
#include <phabos/unipro.h>
#include <phabos/unipro/tsb.h>

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

#define HUB_LINE_N_RESET                    0
#define HUB_RESET_ASSERTION_TIME_IN_USEC    5 /* us */
#define HUB_RESET_DEASSERTION_TIME_IN_MSEC  1 /* ms */

#define TSB_SYSCTL_USBOTG_HSIC_CONTROL  (SYSCTL_BASE + 0x500)
#define TSB_HSIC_DPPULLDOWN             (1 << 0)
#define TSB_HSIC_DMPULLDOWN             (1 << 1)

#if defined(CONFIG_TSB_ES1)
#define TSB_I2C_PINSHARE TSB_PIN_SDIO
#else
#define TSB_I2C_PINSHARE (TSB_PIN_GPIO21 | TSB_PIN_GPIO22)
#endif

#define APBRIDGE_CPORT_MAX 44 // number of CPorts available on the APBridges
#define GPBRIDGE_CPORT_MAX 16 // number of CPorts available on the GPBridges

static int tsb_hcd_power_on(struct device *device)
{
    int retval;

    retval = tsb_request_pinshare(TSB_PIN_UART_CTSRTS);
    if (retval)
        return retval;

    tsb_clr_pinshare(TSB_PIN_UART_CTSRTS);

    gpio_activate(HUB_LINE_N_RESET);

    gpio_direction_out(HUB_LINE_N_RESET, 0);
    udelay(HUB_RESET_ASSERTION_TIME_IN_USEC);
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

    return 0;
}

static int tsb_hcd_power_off(struct device *device)
{
    tsb_clk_disable(TSB_CLK_HSIC480);
    tsb_clk_disable(TSB_CLK_HSICBUS);
    tsb_clk_disable(TSB_CLK_HSICREF);

    gpio_direction_out(HUB_LINE_N_RESET, 0);
    gpio_deactivate(HUB_LINE_N_RESET);

    tsb_release_pinshare(TSB_PIN_UART_CTSRTS);

    return 0;
}

static int tsb_i2c_power_on(struct device *device)
{
    int retval;

    retval = tsb_request_pinshare(TSB_I2C_PINSHARE);
    if (retval)
        return retval;

    tsb_clr_pinshare(TSB_I2C_PINSHARE);

    /* enable I2C clocks */
    tsb_clk_enable(TSB_CLK_I2CP);
    tsb_clk_enable(TSB_CLK_I2CS);

    /* reset I2C module */
    tsb_reset(TSB_RST_I2CP);
    tsb_reset(TSB_RST_I2CS);

    return 0;
}

static int tsb_i2c_power_off(struct device *device)
{
    tsb_clk_disable(TSB_CLK_I2CP);
    tsb_clk_disable(TSB_CLK_I2CS);

    tsb_release_pinshare(TSB_I2C_PINSHARE);

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

static struct i2c_master dw_i2c_device = {
    .device = {
        .name = "dw_i2c",
        .description = "Designware I2C Controller Driver",
        .driver = "dw-i2c",

        .reg_base = I2C_BASE,
        .irq = TSB_IRQ_I2C,

        .power_on = tsb_i2c_power_on,
        .power_off = tsb_i2c_power_off,
    },
};

static struct tsb_unipro_pdata tsb_unipro_pdata = {
    .cport_irq_base = TSB_IRQ_UNIPRO_RX_EOM00,
};

struct unipro_device tsb_unipro = {
    .device = {
        .name = "tsb-unipro",
        .description = "Toshiba UniPro Controller",
        .driver = "tsb-unipro-es2",

        .reg_base = AIO_UNIPRO_BASE,
        .irq = TSB_IRQ_UNIPRO,

        .pdata = &tsb_unipro_pdata,
    },
};

static struct greybus greybus = {
    .device = {
        .name = "greybus",
        .description = "Greybus",
        .driver = "greybus",
    },

    .unipro = &tsb_unipro,
};

static struct gb_device gb_i2c_device = {
    .cport = 4,
    .real_device = &dw_i2c_device,
    .bus = &greybus,

    .device = {
        .name = "gb-dw-i2c",
        .description = "Greybus I2C PHY device",
        .driver = "gb-i2c-phy",
    },
};

static struct gb_device gb_gpio_device = {
    .cport = 3,
    .bus = &greybus,

    .device = {
        .name = "gb-gpio",
        .description = "Greybus GPIO PHY device",
        .driver = "gb-gpio-phy",
    },
};

static unsigned char all_modules_manifest[] = {
  0x9c, 0x00, 0x00, 0x01, 0x08, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x00,
  0x14, 0x00, 0x02, 0x00, 0x0b, 0x01, 0x50, 0x72, 0x6f, 0x6a, 0x65, 0x63,
  0x74, 0x20, 0x41, 0x72, 0x61, 0x00, 0x00, 0x00, 0x14, 0x00, 0x02, 0x00,
  0x0b, 0x02, 0x41, 0x6c, 0x6c, 0x20, 0x4d, 0x6f, 0x64, 0x75, 0x6c, 0x65,
  0x73, 0x00, 0x00, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x04, 0x00,
  0x03, 0x00, 0x01, 0x02, 0x08, 0x00, 0x04, 0x00, 0x04, 0x00, 0x01, 0x03,
  0x08, 0x00, 0x04, 0x00, 0x05, 0x00, 0x01, 0x04, 0x08, 0x00, 0x04, 0x00,
  0x06, 0x00, 0x01, 0x06, 0x08, 0x00, 0x04, 0x00, 0x07, 0x00, 0x01, 0x07,
  0x08, 0x00, 0x04, 0x00, 0x08, 0x00, 0x01, 0x08, 0x08, 0x00, 0x04, 0x00,
  0x09, 0x00, 0x01, 0x09, 0x08, 0x00, 0x04, 0x00, 0x0a, 0x00, 0x01, 0x0b,
  0x08, 0x00, 0x04, 0x00, 0x0b, 0x00, 0x01, 0x10, 0x08, 0x00, 0x04, 0x00,
  0x0c, 0x00, 0x01, 0x11, 0x08, 0x00, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00
};

static struct gb_manifest gb_bdb_manifest = {
    .data = &all_modules_manifest,
    .size = ARRAY_SIZE(all_modules_manifest),
};

static struct gb_device gb_control_device = {
    .bus = &greybus,

    .device = {
        .name = "gb-control-all-protocols",
        .description = "Greybus Control Protocol",
        .driver = "gb-control-gpb",

        .priv = &gb_bdb_manifest,
    },
};

static struct gb_device gb_ap_svc_device = {
    .bus = &greybus,

    .device = {
        .name = "gb-ap-svc",
        .description = "Greybus AP SVC Protocol",
        .driver = "gb-ap-svc",
    },
};

static struct gb_device gb_ap_gpio_device = {
    .bus = &greybus,

    .device = {
        .name = "gb-ap-gpio",
        .description = "Greybus AP GPIO PHY Protocol",
        .driver = "gb-ap-gpio-phy",
    },
};

static struct gb_device gb_ap_control_device = {
    .bus = &greybus,

    .device = {
        .name = "gb-ap-control",
        .description = "Greybus AP Control Protocol",
        .driver = "gb-ap-control",
    },
};

static struct gpio_device gpio_device = {
    .base = -1,
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

struct mm_region tsb_mm_regions[] = {
#if defined(CONFIG_TSB_ES1)
    {
        .start = BUFRAM0_BASE,
        .size = BUFRAM_SIZE,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM1_BASE,
        .size = BUFRAM_SIZE,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM2_BASE,
        .size = BUFRAM_SIZE,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM3_BASE,
        .size = BUFRAM_SIZE,
        .flags = MM_KERNEL | MM_DMA,
    },
#else
    {
        .start = BUFRAM0_BASE,
        .size = 0x8000,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM0_BASE + 0x8000,
        .size = 0x4000,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM1_BASE,
        .size = 0x8000,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM1_BASE + 0x8000,
        .size = 0x4000,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM2_BASE,
        .size = 0x8000,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM2_BASE + 0x8000,
        .size = 0x4000,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM3_BASE,
        .size = 0x8000,
        .flags = MM_KERNEL | MM_DMA,
    },
    {
        .start = BUFRAM3_BASE + 0x8000,
        .size = 0x4000,
        .flags = MM_KERNEL | MM_DMA,
    },
#endif
};

void tsb_uart_init(void)
{
    tsb_request_pinshare(TSB_PIN_UART_RXTX);
    tsb_set_pinshare(TSB_PIN_UART_RXTX);

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
    tsb_clk_init();
    tsb_uart_init();

    for (int i = 0; i < ARRAY_SIZE(tsb_mm_regions); i++)
        mm_add_region(&tsb_mm_regions[i]);

    tsb_unipro_pdata.debug_0720 = tsb_get_debug_reg(0x0720);
    switch (tsb_get_product_id()) {
    case tsb_pid_apbridge:
        tsb_unipro_pdata.product_id = TSB_UNIPRO_APBRIDGE;
        tsb_unipro.cport_count = APBRIDGE_CPORT_MAX;
        break;

    case tsb_pid_gpbridge:
        tsb_unipro_pdata.product_id = TSB_UNIPRO_GPBRIDGE;
        tsb_unipro.cport_count = GPBRIDGE_CPORT_MAX;
        break;

    default:
        tsb_unipro_pdata.product_id = TSB_UNIPRO_OTHER;
        break;
    }

#if defined(CONFIG_ARA_BACKPORT)
    int tsb_device_table_register(void);
    tsb_device_table_register();
#endif

    device_register(&gpio_device.device);
    device_register(&uart16550_device.device);
    device_register(&usb_hcd_device.device);
    device_register(&dw_i2c_device.device);
    device_register(&tsb_unipro.device);

    device_register(&greybus.device);
    device_register(&gb_control_device.device);
    device_register(&gb_gpio_device.device);
    device_register(&gb_i2c_device.device);
    device_register(&gb_ap_gpio_device.device);
    device_register(&gb_ap_control_device.device);
    device_register(&gb_ap_svc_device.device);
}
