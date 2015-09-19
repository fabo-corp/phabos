/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <errno.h>

#include <asm/irq.h>
#include <asm/hwio.h>

#include <phabos/driver.h>
#include <phabos/gpio.h>

#define STM32_GPIO_MODER    0x0
#define STM32_GPIO_OTYPER   0x4
#define STM32_GPIO_IDR      0x10
#define STM32_GPIO_BSRR     0x18

#define STM32_GPIO_MODER_MASK   0x3
#define STM32_GPIO_MODER_INPUT  0x0
#define STM32_GPIO_MODER_OUTPUT 0x1

static struct gpio_ops stm32_gpio_ops;

static int stm32_gpio_set_value(struct gpio_device *dev, unsigned int line,
                                unsigned int value);

static size_t stm32_gpio_line_count(struct gpio_device *dev)
{
    return dev->count;
}

static int stm32_gpio_set_direction(struct gpio_device *dev, unsigned int line,
                                    unsigned int direction)
{
    unsigned int offset = line << 1;
    uint32_t moder;

    RET_IF_FAIL(direction <= STM32_GPIO_MODER_MASK, -EINVAL);

    if (!dev || line >= dev->count)
        return -EINVAL;

    irq_disable();
    moder = read32(dev->device.reg_base + STM32_GPIO_MODER);
    moder &= ~(STM32_GPIO_MODER_MASK << offset);
    moder |= direction << offset;
    write32(dev->device.reg_base + STM32_GPIO_MODER, moder);
    irq_enable();

    return 0;
}

static int stm32_gpio_direction_in(struct gpio_device *dev, unsigned int line)
{
    return stm32_gpio_set_direction(dev, line, STM32_GPIO_MODER_INPUT);
}

static int stm32_gpio_direction_out(struct gpio_device *dev, unsigned int line,
                                    unsigned int value)
{
    int retval = stm32_gpio_set_value(dev, line, value);
    if (retval)
        return retval;

    return stm32_gpio_set_direction(dev, line, STM32_GPIO_MODER_OUTPUT);
}

static int stm32_gpio_get_direction(struct gpio_device *dev, unsigned int line)
{
    unsigned int offset = line << 1;
    uint32_t moder;

    if (!dev || line >= dev->count)
        return -EINVAL;

    moder = read32(dev->device.reg_base + STM32_GPIO_MODER);

    switch ((moder >> offset) & STM32_GPIO_MODER_MASK) {
    case STM32_GPIO_MODER_INPUT:
        return 1;

    case STM32_GPIO_MODER_OUTPUT:
        return 0;
    }

    return -EIO;
}

static int stm32_gpio_get_value(struct gpio_device *dev, unsigned int line)
{
    if (!dev || line >= dev->count)
        return -EINVAL;

    return !!(read32(dev->device.reg_base + STM32_GPIO_IDR) & (1 << line));
}

static int stm32_gpio_set_value(struct gpio_device *dev, unsigned int line,
                                unsigned int value)
{
    unsigned int offset = value ? 0 : 16;

    if (!dev || line >= dev->count)
        return -EINVAL;

    write32(dev->device.reg_base + STM32_GPIO_BSRR, (1 << (offset + line)));
    return 0;
}

static int stm32_gpio_probe(struct device *device)
{
    struct gpio_device *dev = containerof(device, struct gpio_device, device);

    RET_IF_FAIL(device, -EINVAL);

    dev->ops = &stm32_gpio_ops;

    return gpio_device_register(dev);
}

static struct gpio_ops stm32_gpio_ops = {
    .get_direction = stm32_gpio_get_direction,
    .direction_in = stm32_gpio_direction_in,
    .direction_out = stm32_gpio_direction_out,
    .get_value = stm32_gpio_get_value,
    .set_value = stm32_gpio_set_value,
    .line_count = stm32_gpio_line_count,
};

__driver__ struct driver stm32_gpio_driver = {
    .name = "stm32-gpio",

    .probe = stm32_gpio_probe,
};
