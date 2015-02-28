/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include <asm/machine.h>
#include <asm/hwio.h>
#include <asm/irq.h>
#include <phabos/kprintf.h>

#define SETENA0 0xE000E100
#define CLRENA0 0xE000E180

#define ARM_CM_NUM_EXCEPTION 16

extern irq_handler_t intr_vector[CPU_NUM_IRQ + ARM_CM_NUM_EXCEPTION];
extern struct irq_handler irq_vector[CPU_NUM_IRQ + ARM_CM_NUM_EXCEPTION];

static unsigned int irq_global_state;
static uint8_t irq_state[CPU_NUM_IRQ];

void default_irq_handler(int irq, void *data)
{
    kprintf("unhandled interrupt: %d\n", irq);

    while (1)
        asm volatile("nop");
}

void irq_initialize(void)
{
    for (int i = 0; i < CPU_NUM_IRQ; i++)
        irq_disable_line(i);
}

void irq_disable_line(int line)
{
    irq_disable();

    assert(line < CPU_NUM_IRQ);
    assert(line != 0xFF);

    irq_state[line]++;
    write32(CLRENA0 + 4 * (line / 32), 1 << (line % 32));

    irq_enable();
}

void irq_enable_line(int line)
{
    irq_disable();

    assert(line < CPU_NUM_IRQ);
    assert(irq_state[line] != 0);

    if (--irq_state[line] == 0)
        write32(SETENA0 + 4 * (line / 32), 1 << (line % 32));

    irq_enable();
}

void irq_disable(void)
{
    asm volatile("cpsid i");
    irq_global_state++;
}

void irq_enable(void)
{
    if (--irq_global_state == 0)
        asm volatile("cpsie i");
}

int irq_attach(int line, irq_handler_t handler, void *data)
{
    assert(line >= 0);

    if (line >= CPU_NUM_IRQ)
        return -EINVAL;

    irq_vector[ARM_CM_NUM_EXCEPTION + line].handler = handler;
    irq_vector[ARM_CM_NUM_EXCEPTION + line].data = data;
    return 0;
}

void irq_detach(int line)
{
    assert(line >= 0);

    if (line >= CPU_NUM_IRQ)
        return;

    irq_vector[ARM_CM_NUM_EXCEPTION + line].handler = default_irq_handler;
    irq_vector[ARM_CM_NUM_EXCEPTION + line].data = NULL;
}

int irq_get_active_line(void)
{
    uint32_t psr;

    asm volatile("mrs %0, xpsr" :"=r"(psr));

    psr &= 0xff;

    if (psr < ARM_CM_NUM_EXCEPTION)
        return -1;
    return psr - ARM_CM_NUM_EXCEPTION;
}
