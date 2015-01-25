/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __I2C_H__
#define __I2C_H__

#define I2C_READ    (1 << 1)

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include <phabos/assert.h>
#include <phabos/driver.h>

struct i2c_dev;
struct i2c_msg;

struct i2c_ops {
    int (*transfer)(struct i2c_dev *dev, struct i2c_msg *msg, size_t count);
    int (*set_address)(struct i2c_dev *dev, unsigned int addr,
                       unsigned int nbits);
};

struct i2c_dev {
    struct device device;

    struct i2c_ops *ops;
};

struct i2c_msg {
    uint16_t addr;
    uint8_t *buffer;
    size_t length;
    unsigned long flags;
};

static inline int i2c_transfer(struct i2c_dev *dev, struct i2c_msg *msg,
                               size_t count)
{
    RET_IF_FAIL(dev, -EINVAL);
    RET_IF_FAIL(dev->ops, -EINVAL);
    RET_IF_FAIL(dev->ops->transfer, -EINVAL);
    return dev->ops->transfer(dev, msg, count);
}

static inline int i2c_set_address(struct i2c_dev *dev, unsigned int addr,
                                  unsigned int nbits)
{
    RET_IF_FAIL(dev, -EINVAL);
    RET_IF_FAIL(dev->ops, -EINVAL);
    RET_IF_FAIL(dev->ops->set_address, -EINVAL);
    return dev->ops->set_address(dev, addr, nbits);
}

#endif /* __I2C_H__ */

