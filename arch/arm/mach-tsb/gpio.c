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
 *
 * Authors: Fabien Parent <fparent@baylibre.com>
 *          Benoit Cousson <bcousson@baylibre.com>
 */

#include <config.h>

#include <asm/irq.h>
#include <asm/hwio.h>
#include <asm/spinlock.h>
#include <asm/tsb-irq.h>
#include <phabos/gpio.h>
#include <phabos/utils.h>

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "scm.h"

#define GPIO_BASE           0x40003000
#define GPIO_DATA           (GPIO_BASE)
#define GPIO_ODATA          (GPIO_BASE + 0x4)
#define GPIO_ODATASET       (GPIO_BASE + 0x8)
#define GPIO_ODATACLR       (GPIO_BASE + 0xc)
#define GPIO_DIR            (GPIO_BASE + 0x10)
#define GPIO_DIROUT         (GPIO_BASE + 0x14)
#define GPIO_DIRIN          (GPIO_BASE + 0x18)
#define GPIO_INTMASK        (GPIO_BASE + 0x1c)
#define GPIO_INTMASKSET     (GPIO_BASE + 0x20)
#define GPIO_INTMASKCLR     (GPIO_BASE + 0x24)
#define GPIO_RAWINTSTAT     (GPIO_BASE + 0x28)
#define GPIO_INTSTAT        (GPIO_BASE + 0x2c)
#define GPIO_INTCTRL0       (GPIO_BASE + 0x30)
#define GPIO_INTCTRL1       (GPIO_BASE + 0x34)
#define GPIO_INTCTRL2       (GPIO_BASE + 0x38)
#define GPIO_INTCTRL3       (GPIO_BASE + 0x3c)

#define TSB_IRQ_TYPE_LEVEL_LOW      0x0
#define TSB_IRQ_TYPE_LEVEL_HIGH     0x1
#define TSB_IRQ_TYPE_EDGE_FALLING   0x2
#define TSB_IRQ_TYPE_EDGE_RISING    0x3
#define TSB_IRQ_TYPE_EDGE_BOTH      0x7

#if defined(CONFIG_TSB_ES1)
#define NR_GPIO_IRQS 16
#else
#define NR_GPIO_IRQS 27
#endif

/* A table of handlers for each GPIO interrupt */
static gpio_irq_handler_t tsb_gpio_irq_vector[NR_GPIO_IRQS];
static uint8_t tsb_gpio_irq_gpio_base[NR_GPIO_IRQS];
static volatile uint32_t refcount;
static struct gpio_ops tsb_gpio_ops;
static struct spinlock tsb_gpio_lock = SPINLOCK_INIT(tsb_gpio_lock);

static void tsb_gpio_irq_handler(int irq, void *data);

static int tsb_gpio_get_value(struct gpio_device *dev, unsigned int line)
{
    return !!(read32(GPIO_DATA) & (1 << line));
}

static int tsb_gpio_set_value(struct gpio_device *dev, unsigned int line,
                              unsigned int value)
{
    write32(value ? GPIO_ODATASET : GPIO_ODATACLR, 1 << line);
    return 0;
}

static int tsb_gpio_get_direction(struct gpio_device *dev, unsigned int line)
{
    uint32_t dir = read32(GPIO_DIR);
    return !(dir & (1 << line));
}

static int tsb_gpio_direction_in(struct gpio_device *dev, unsigned int line)
{
    write32(GPIO_DIRIN, 1 << line);
    return 0;
}

static int tsb_gpio_direction_out(struct gpio_device *dev, unsigned int line,
                                  unsigned int value)
{
    tsb_gpio_set_value(dev, line, value);
    write32(GPIO_DIROUT, 1 << line);
    return 0;
}

static size_t tsb_gpio_line_count(struct gpio_device *dev)
{
    return NR_GPIO_IRQS;
}

static void tsb_gpio_initialize(void)
{
    spinlock_lock(&tsb_gpio_lock);

    if (refcount++)
        goto out;

    tsb_clk_enable(TSB_CLK_GPIO);
    tsb_reset(TSB_RST_GPIO);

    memset(tsb_gpio_irq_vector, 0, ARRAY_SIZE(tsb_gpio_irq_vector));

    /* Attach Interrupt Handler */
    irq_attach(TSB_IRQ_GPIO, tsb_gpio_irq_handler, NULL);

    /* Enable Interrupt Handler */
    irq_enable_line(TSB_IRQ_GPIO);

out:
    spinlock_unlock(&tsb_gpio_lock);
}

static void tsb_gpio_uninitialize(void)
{
    spinlock_lock(&tsb_gpio_lock);

    if (!refcount)
        goto out;

    if (--refcount)
        goto out;

    tsb_clk_disable(TSB_CLK_GPIO);

    /* Detach Interrupt Handler */
    irq_detach(TSB_IRQ_GPIO);

out:
    spinlock_unlock(&tsb_gpio_lock);
}

int tsb_gpio_activate(struct gpio_device *dev, unsigned int line)
{
    tsb_gpio_initialize();
    return 0;
}

int tsb_gpio_deactivate(struct gpio_device *dev, unsigned int line)
{
    tsb_gpio_uninitialize();
    return 0;
}

static int tsb_gpio_mask_irq(struct gpio_device *dev, unsigned int line)
{
    write32(GPIO_INTMASKSET, 1 << line);
    return 0;
}

static int tsb_gpio_unmask_irq(struct gpio_device *dev, unsigned int line)
{
    write32(GPIO_INTMASKCLR, 1 << line);
    return 0;
}

static int tsb_gpio_clear_interrupt(struct gpio_device *dev, unsigned int line)
{
    write32(GPIO_RAWINTSTAT, 1 << line);
    return 0;
}

static uint32_t tsb_gpio_get_interrupt(void)
{
    return read32(GPIO_INTSTAT);
}

static int tsb_set_gpio_triggering(struct gpio_device *dev, unsigned int line,
                                   int trigger)
{
    int tsb_trigger;
    uint32_t shift = 4 * (line & 0x7);

    switch(trigger) {
    case IRQ_TYPE_EDGE_RISING:
        tsb_trigger = TSB_IRQ_TYPE_EDGE_RISING;
        break;
    case IRQ_TYPE_EDGE_FALLING:
        tsb_trigger = TSB_IRQ_TYPE_EDGE_FALLING;
        break;
    case IRQ_TYPE_EDGE_BOTH:
        tsb_trigger = TSB_IRQ_TYPE_EDGE_BOTH;
        break;
    case IRQ_TYPE_LEVEL_HIGH:
        tsb_trigger = TSB_IRQ_TYPE_LEVEL_HIGH;
        break;
    case IRQ_TYPE_LEVEL_LOW:
        tsb_trigger = TSB_IRQ_TYPE_LEVEL_LOW;
        break;
    default:
        return -EINVAL;
    }

    read32(GPIO_INTCTRL0 + ((line >> 1) & 0xfc)) |= tsb_trigger << shift;
    return 0;
}

static void tsb_gpio_irq_handler(int irq, void *data)
{
    /*
     * Handle each pending GPIO interrupt.  "The GPIO MIS register is the masked
     * interrupt status register. Bits read High in GPIO MIS reflect the status
     * of input lines triggering an interrupt. Bits read as Low indicate that
     * either no interrupt has been generated, or the interrupt is masked."
     */
    uint32_t irqstat;
    uint8_t base;
    int pin;

    /*
     * Clear all GPIO interrupts that we are going to process. "The GPIO_RAWINTSTAT
     * register is the interrupt clear register. Writing a 1 to a bit in this
     * register clears the corresponding interrupt edge detection logic register.
     * Writing a 0 has no effect."
     */
    irqstat = tsb_gpio_get_interrupt();
    write32(GPIO_RAWINTSTAT, irqstat);

    /* Now process each IRQ pending in the GPIO */
    for (pin = 0; pin < NR_GPIO_IRQS && irqstat != 0; pin++, irqstat >>= 1) {
        if ((irqstat & 1) != 0) {
            base = tsb_gpio_irq_gpio_base[pin];
            if (tsb_gpio_irq_vector[pin])
                tsb_gpio_irq_vector[pin](base + pin);
        }
    }
}

static int tsb_gpio_irqattach(struct gpio_device *dev, unsigned int line,
                              gpio_irq_handler_t handler)
{
    if (line >= NR_GPIO_IRQS)
        return -EINVAL;

    irq_disable();

#if 0
    /* Save the new ISR in the table. */
    tsb_gpio_irq_vector[line] = handler;
    tsb_gpio_irq_gpio_base[irq] = base;
#endif

    irq_enable();

    return 0;
}

static int tsb_gpio_probe(struct device *device)
{
    struct gpio_device *dev = containerof(device, struct gpio_device, device);

    RET_IF_FAIL(device, -EINVAL);

    dev->ops = &tsb_gpio_ops;

    return gpio_device_register(dev);
}

static int tsb_gpio_remove(struct device *device)
{
    irq_disable_line(TSB_IRQ_GPIO);
    irq_detach(TSB_IRQ_GPIO);

    tsb_clk_disable(TSB_CLK_GPIO);

    return 0;
}

static struct gpio_ops tsb_gpio_ops = {
    .activate = tsb_gpio_activate,
    .deactivate = tsb_gpio_deactivate,
    .get_direction = tsb_gpio_get_direction,
    .direction_in = tsb_gpio_direction_in,
    .direction_out = tsb_gpio_direction_out,
    .get_value = tsb_gpio_get_value,
    .set_value = tsb_gpio_set_value,
    .line_count = tsb_gpio_line_count,
    .irq_clear = tsb_gpio_clear_interrupt,
    .irq_mask = tsb_gpio_mask_irq,
    .irq_unmask = tsb_gpio_unmask_irq,
    .irq_set_triggering = tsb_set_gpio_triggering,
    .irq_attach = tsb_gpio_irqattach,
};

__driver__ struct driver tsb_gpio_driver = {
    .name = "tsb-gpio",

    .probe = tsb_gpio_probe,
    .remove = tsb_gpio_remove,
};
