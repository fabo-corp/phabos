#include <asm/spinlock.h>
#include <phabos/driver.h>
#include <phabos/hashtable.h>
#include <phabos/serial/tty.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#define DRIVER_NAME "tty"

static struct hashtable tty_table;
static struct spinlock tty_table_lock;

int tty_register(struct tty_device *dev, dev_t devnum)
{
    unsigned int i;
    int retval;
    char *name;

    RET_IF_FAIL(dev, -EINVAL);

    spinlock_lock(&tty_table_lock);

    for (i = 0; hashtable_has(&tty_table, (void*) i); i++)
        ;

    dev->id = i;
    hashtable_add(&tty_table, (void*) i, dev);

    spinlock_unlock(&tty_table_lock);

    retval = asprintf(&name, "ttyS%u", i);
    if (retval < 0)
        return -ENOMEM;

    devfs_mknod(name, S_IFCHR, devnum);

    kprintf(DRIVER_NAME ": new device ttyS%u\n", i);

    free(name);
    return 0;
}

int tty_unregister(struct tty_device *dev)
{
    RET_IF_FAIL(dev, -EINVAL);

    spinlock_lock(&tty_table_lock);
    hashtable_remove(&tty_table, (void*) dev->id);
    spinlock_unlock(&tty_table_lock);

    return 0;
}

int tty_init(struct driver *driver)
{
    hashtable_init_uint(&tty_table);
    spinlock_init(&tty_table_lock);
    return 0;
}

__driver__ struct driver tty_driver = {
    .name = DRIVER_NAME,

    .init = tty_init,
};
