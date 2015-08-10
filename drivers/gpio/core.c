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
static size_t line_count;

static struct gpio_device *_gpio_get_device(unsigned int line)
{
    list_foreach(&devices, iter) {
        struct gpio_device *dev = list_entry(iter, struct gpio_device, list);
        if (line >= dev->base && line < dev->base + dev->count) {
            return dev;
        }
    }

    return NULL;
}

static struct gpio_device *gpio_get_device(unsigned int line)
{
    struct gpio_device *device;

    spinlock_lock(&dev_lock);

    device = _gpio_get_device(line);

    spinlock_unlock(&dev_lock);

    return device;
}

static unsigned gpio_find_base(size_t count)
{
    unsigned base = 0;

    list_foreach(&devices, iter) {
        struct gpio_device *dev = list_entry(iter, struct gpio_device, list);

        unsigned last = base + count - 1;

        if ((base >= dev->base && base < dev->base + dev->count) ||
            (last >= dev->base && last < dev->base + dev->count)) {
            base = dev->base + dev->count;
            continue;
        }
    }

    return base;
}

static int gpio_compare(struct list_head *head1, struct list_head *head2)
{
    struct gpio_device *dev1 = containerof(head1, struct gpio_device, list);
    struct gpio_device *dev2 = containerof(head2, struct gpio_device, list);
    return dev1->base - dev2->base;
}

int gpio_device_register(struct gpio_device *gpio)
{
    int retval = 0;

    RET_IF_FAIL(gpio, -EINVAL);

    list_init(&gpio->list);

    spinlock_lock(&dev_lock);

    if (gpio->base != -1) {
        if (_gpio_get_device(gpio->base) ||
            _gpio_get_device(gpio->base + gpio->count - 1)) {
            retval = -EBUSY;
            goto out;
        }
    } else {
        gpio->base = gpio_find_base(gpio->count);
    }

    line_count += gpio->count;
    list_sorted_add(&devices, &gpio->list, gpio_compare);

out:
    spinlock_unlock(&dev_lock);

    return retval;
}

size_t gpio_line_count(void)
{
    return line_count;
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
