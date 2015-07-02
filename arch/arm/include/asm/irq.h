/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ARM_IRQ_H__
#define __ARM_IRQ_H__

struct irq_handler {
    void (*handler)(int line, void *data);
    void *data;
};

typedef void (*irq_handler_t)(int line, void *data);

void irq_initialize(void);
void irq_disable(void);
void irq_enable(void);
void irq_enable_line(int line);
void irq_disable_line(int line);
int irq_attach(int line, irq_handler_t handler, void *data);
void irq_detach(int line);
int irq_get_active_line(void);
void irq_clear(int line);
void irq_pend(int line);

#endif /* __ARM_IRQ_H__ */

