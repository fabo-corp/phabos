/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __GPIO_H__
#define __GPIO_H__

#include <stdint.h>
#include <phabos/driver.h>
#include <phabos/list.h>

#define IRQ_TYPE_NONE           0x00000000
#define IRQ_TYPE_EDGE_RISING    0x00000001
#define IRQ_TYPE_EDGE_FALLING   0x00000002
#define IRQ_TYPE_EDGE_BOTH      (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_HIGH     0x00000004
#define IRQ_TYPE_LEVEL_LOW      0x00000008

struct gpio_device;

typedef void (*gpio_irq_handler_t)(int pin);

struct gpio_ops {
    int (*get_direction)(struct gpio_device *dev, unsigned int line);
    int (*direction_in)(struct gpio_device *dev, unsigned int line);
    int (*direction_out)(struct gpio_device *dev, unsigned int line,
                         unsigned int value);

    int (*activate)(struct gpio_device *dev, unsigned int line);
    int (*deactivate)(struct gpio_device *dev, unsigned int line);

    int (*get_value)(struct gpio_device *dev, unsigned int line);
    int (*set_value)(struct gpio_device *dev, unsigned int line,
                     unsigned int value);

    int (*set_debounce)(struct gpio_device *dev, unsigned int line,
                        unsigned int usec);

    int (*irq_attach)(struct gpio_device *dev, unsigned int line,
                      gpio_irq_handler_t handler);
    int (*irq_set_triggering)(struct gpio_device *dev, unsigned int line,
                              int trigger);
    int (*irq_mask)(struct gpio_device *dev, unsigned int line);
    int (*irq_unmask)(struct gpio_device *dev, unsigned int line);
    int (*irq_clear)(struct gpio_device *dev, unsigned int line);

    size_t (*line_count)(struct gpio_device *dev);
};

struct gpio_device {
    struct device device;
    struct gpio_ops *ops;
    struct list_head list;

    unsigned int base;
    size_t count;
};

int gpio_device_register(struct gpio_device *gpio);
size_t gpio_line_count(void);

int gpio_get_direction(unsigned int line);
int gpio_direction_in(unsigned int line);
int gpio_direction_out(unsigned int line,
                       unsigned int value);

int gpio_activate(unsigned int line);
int gpio_deactivate(unsigned int line);

int gpio_get_value(unsigned int line);
int gpio_set_value(unsigned int line, unsigned int value);

int gpio_set_debounce(unsigned int line, unsigned int usec);

int gpio_irq_attach(unsigned int line, gpio_irq_handler_t handler);
int gpio_irq_set_triggering(unsigned int line, int trigger);
int gpio_irq_mask(unsigned int line);
int gpio_irq_unmask(unsigned int line);
int gpio_irq_clear(unsigned int line);

#endif /* __GPIO_H__ */

