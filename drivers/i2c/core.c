#include <asm/spinlock.h>
#include <phabos/driver.h>
#include <phabos/hashtable.h>
#include <phabos/i2c.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#define DRIVER_NAME "i2c-core"

static struct hashtable adapter_table;
static struct spinlock adapter_table_lock;

int i2c_transfer(struct i2c_adapter *adapter, struct i2c_msg *msg, size_t count)
{
    if (!adapter->ops->transfer)
        return -ENOSYS;

    return adapter->ops->transfer(adapter, msg, count);
}

int i2c_set_frequency(struct i2c_adapter *adapter, unsigned long freq)
{
        if (!adapter->ops->set_frequency)
            return -ENOSYS;

        return adapter->ops->set_frequency(adapter, freq);
}

static int i2c_adapter_ioctl(struct file *file, unsigned long cmd, va_list vl)
{
    struct device *device = devnum_get_device(file->inode->dev);
    struct i2c_adapter *adapter = to_adapter(device);
    struct i2c_msg *msg;
    size_t msg_count;

    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(adapter->ops, -EINVAL);

    switch (cmd) {
    case I2C_TRANSFER:
        msg = va_arg(vl, struct i2c_msg*);
        msg_count = va_arg(vl, size_t);
        return i2c_transfer(adapter, msg, msg_count);

    case I2C_SET_FREQUENCY:
        return i2c_set_frequency(adapter, va_arg(vl, unsigned long));

    default:
        return -EINVAL;
    }
}

static struct file_operations i2c_operations = {
    .ioctl = i2c_adapter_ioctl,
};

int i2c_adapter_register(struct i2c_adapter *adapter, dev_t devnum)
{
    unsigned int i;
    int retval;
    char *name;

    RET_IF_FAIL(adapter, -EINVAL);

    spinlock_lock(&adapter_table_lock);

    for (i = 0; hashtable_has(&adapter_table, (void*) i); i++)
        ;

    adapter->id = i;
    hashtable_add(&adapter_table, (void*) i, adapter);

    spinlock_unlock(&adapter_table_lock);

    retval = asprintf(&name, "i2c-%u", i);
    if (retval < 0)
        return -ENOMEM;

    retval = chrdev_register(&adapter->device, devnum, name, &i2c_operations);

    free(name);
    return retval;
}

int i2c_adapter_unregister(struct i2c_adapter *adapter)
{
    RET_IF_FAIL(adapter, -EINVAL);

    spinlock_lock(&adapter_table_lock);
    hashtable_remove(&adapter_table, (void*) adapter->id);
    spinlock_unlock(&adapter_table_lock);

    return 0;
}

static int i2c_core_init(struct driver *driver)
{
    hashtable_init_uint(&adapter_table);
    spinlock_init(&adapter_table_lock);
    return 0;
}

__driver__ struct driver i2c_core_driver = {
    .name = DRIVER_NAME,

    .init = i2c_core_init,
};
