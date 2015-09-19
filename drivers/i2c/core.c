/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/spinlock.h>
#include <phabos/driver.h>
#include <phabos/hashtable.h>
#include <phabos/i2c.h>
#include <phabos/char.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#define DRIVER_NAME "i2c-core"

static struct hashtable *master_table;
static struct spinlock master_table_lock;

int i2c_transfer(struct i2c_master *master, struct i2c_msg *msg, size_t count)
{
    if (!master->ops->transfer)
        return -ENOSYS;

    return master->ops->transfer(master, msg, count);
}

int i2c_set_frequency(struct i2c_master *master, unsigned long freq)
{
        if (!master->ops->set_frequency)
            return -ENOSYS;

        return master->ops->set_frequency(master, freq);
}

static int i2c_master_ioctl(struct file *file, unsigned long cmd, va_list vl)
{
    struct device *device = devnum_get_device(file->inode->dev);
    struct i2c_master *master = to_master(device);
    struct i2c_msg *msg;
    size_t msg_count;

    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(master->ops, -EINVAL);

    switch (cmd) {
    case I2C_TRANSFER:
        msg = va_arg(vl, struct i2c_msg*);
        msg_count = va_arg(vl, size_t);
        return i2c_transfer(master, msg, msg_count);

    case I2C_SET_FREQUENCY:
        return i2c_set_frequency(master, va_arg(vl, unsigned long));

    default:
        return -EINVAL;
    }
}

static struct file_operations i2c_operations = {
    .ioctl = i2c_master_ioctl,
};

int i2c_master_register(struct i2c_master *master, dev_t devnum)
{
    unsigned int i;
    int retval;
    char *name;

    RET_IF_FAIL(master, -EINVAL);

    spinlock_lock(&master_table_lock);

    for (i = 0; hashtable_has(master_table, (void*) i); i++)
        ;

    master->id = i;
    hashtable_add(master_table, (void*) i, master);

    spinlock_unlock(&master_table_lock);

    retval = asprintf(&name, "i2c-%u", i);
    if (retval < 0)
        return -ENOMEM;

    retval = chrdev_register(&master->device, devnum, name, &i2c_operations);

    free(name);
    return retval;
}

int i2c_master_unregister(struct i2c_master *master)
{
    RET_IF_FAIL(master, -EINVAL);

    spinlock_lock(&master_table_lock);
    hashtable_remove(master_table, (void*) master->id);
    spinlock_unlock(&master_table_lock);

    return 0;
}

static int i2c_core_init(struct driver *driver)
{
    master_table = hashtable_create_uint();
    spinlock_init(&master_table_lock);
    return 0;
}

__driver__ struct driver i2c_core_driver = {
    .name = DRIVER_NAME,

    .init = i2c_core_init,
};
