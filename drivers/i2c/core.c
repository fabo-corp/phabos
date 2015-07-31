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

    devfs_mknod(name, S_IFCHR, devnum);

    kprintf(DRIVER_NAME ": new device i2c-%u\n", i);

    free(name);
    return 0;
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
