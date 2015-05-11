/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ARM_IRQ_H__
#define __ARM_IRQ_H__

typedef void (*irq_handler_t)(void);

void irq_initialize(void);
void irq_disable(void);
void irq_enable(void);
void irq_enable_line(int line);
void irq_disable_line(int line);
int irq_attach(int line, irq_handler_t handler);
void irq_detach(int line);
int irq_get_active_line(void);

#endif /* __ARM_IRQ_H__ */

