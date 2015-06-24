/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <phabos/gpio.h>
#include <phabos/assert.h>
#include <phabos/utils.h>
#include <asm/spinlock.h>
#include <errno.h>

static struct list_head devices = LIST_INIT(devices);
static struct spinlock dev_lock = SPINLOCK_INIT(dev_lock);
static unsigned int next_base;

int gpio_device_register(struct gpio_device *gpio)
{
    RET_IF_FAIL(gpio, -EINVAL);

    list_init(&gpio->list);

    spinlock_lock(&dev_lock);
    gpio->base = next_base;
    next_base = gpio->base + gpio->count;
    list_add(&devices, &gpio->list);
    spinlock_unlock(&dev_lock);

    return 0;
}

struct gpio_device *gpio_get_device(unsigned int line)
{
    struct gpio_device *device = NULL;

    spinlock_lock(&dev_lock);

    list_foreach(&devices, iter) {
        struct gpio_device *dev = list_entry(iter, struct gpio_device, list);
        if (line >= dev->base && line < dev->base + dev->count) {
            device = dev;
            break;
        }
    }

    spinlock_unlock(&dev_lock);

    return device;
}

size_t gpio_line_count(void)
{
    return next_base;
}

int gpio_get_direction(unsigned int line)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->get_direction)
        return -ENOSYS;

    return device->ops->get_direction(device, line - device->base);
}

int gpio_direction_in(unsigned int line)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->direction_in)
        return -ENOSYS;

    return device->ops->direction_in(device, line - device->base);
}

int gpio_direction_out(unsigned int line, unsigned int value)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->direction_out)
        return -ENOSYS;

    return device->ops->direction_out(device, line - device->base, value);
}

int gpio_activate(unsigned int line)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->activate)
        return -ENOSYS;

    return device->ops->activate(device, line - device->base);
}

int gpio_deactivate(unsigned int line)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->deactivate)
        return -ENOSYS;

    return device->ops->deactivate(device, line - device->base);
}

int gpio_get_value(unsigned int line)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->get_value)
        return -ENOSYS;

    return device->ops->get_value(device, line - device->base);
}

int gpio_set_value(unsigned int line, unsigned int value)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->set_value)
        return -ENOSYS;

    return device->ops->set_value(device, line - device->base, value);
}

int gpio_set_debounce(unsigned int line, unsigned int usec)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->set_debounce)
        return -ENOSYS;

    return device->ops->set_debounce(device, line - device->base, usec);
}

int gpio_irq_attach(unsigned int line, gpio_irq_handler_t handler)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->irq_attach)
        return -ENOSYS;

    return device->ops->irq_attach(device, line - device->base, handler);
}

int gpio_irq_set_triggering(unsigned int line, int trigger)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->irq_set_triggering)
        return -ENOSYS;

    return device->ops->irq_set_triggering(device, line - device->base,
                                           trigger);
}

int gpio_irq_mask(unsigned int line)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->irq_mask)
        return -ENOSYS;

    return device->ops->irq_mask(device, line - device->base);
}

int gpio_irq_unmask(unsigned int line)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->irq_unmask)
        return -ENOSYS;

    return device->ops->irq_unmask(device, line - device->base);
}

int gpio_irq_clear(unsigned int line)
{
    struct gpio_device *device = gpio_get_device(line);
    if (!device)
        return -ENODEV;

    RET_IF_FAIL(device->ops, -EINVAL);

    if (!device->ops->irq_clear)
        return -ENOSYS;

    return device->ops->irq_clear(device, line - device->base);
}
