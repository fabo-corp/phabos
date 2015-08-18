/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __I2C_H__
#define __I2C_H__

#include <phabos/utils.h>

#define I2C_STDMODE_MAX_FREQ        100000
#define I2C_STDMODE_MAX_RISE_TIME   1000

#define I2C_FASTMODE_MAX_FREQ       400000
#define I2C_FASTMODE_MAX_RISE_TIME  300

#define I2C_XFER_TYPE_MASK  ~1

/* ioctl operations */
#define I2C_SET_FREQUENCY 0
#define I2C_TRANSFER      1

/* i2c_msg flags */
#define I2C_M_READ  (1 << 0)

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include <phabos/assert.h>
#include <phabos/driver.h>

struct i2c_dev;
struct i2c_msg;
struct i2c_master;

struct i2c_master_ops {
    int (*transfer)(struct i2c_master *adt, struct i2c_msg *msg, size_t count);
    int (*set_frequency)(struct i2c_master *master, unsigned long freq);
};

struct i2c_master {
    struct device device;
    unsigned int id;
    struct i2c_master_ops *ops;
};

struct i2c_dev {
    struct i2c_master *master;
    unsigned address;
    unsigned long freq;
};

struct i2c_msg {
    uint16_t addr;
    uint8_t *buffer;
    size_t length;
    unsigned long flags;
};

static inline struct i2c_master *to_master(struct device *device)
{
    return containerof(device, struct i2c_master, device);
}

int i2c_master_register(struct i2c_master *master, dev_t devnum);
int i2c_master_unregister(struct i2c_master *master);
int i2c_set_frequency(struct i2c_master *master, unsigned long freq);
int i2c_transfer(struct i2c_master *adap, struct i2c_msg *msg, size_t count);

#endif /* __I2C_H__ */

