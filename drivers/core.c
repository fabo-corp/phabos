/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <errno.h>
#include <string.h>
#include <phabos/assert.h>
#include <phabos/utils.h>
#include <phabos/driver.h>
#include <phabos/kprintf.h>
#include <phabos/hashtable.h>
#include <asm/spinlock.h>

static bool is_probing_enabled;
static struct list_head drivers = LIST_INIT(drivers);
static struct list_head probed_devices = LIST_INIT(probed_devices);
static struct list_head devices = LIST_INIT(devices);
static hashtable_t driver_table;
static struct spinlock driver_table_lock = SPINLOCK_INIT(driver_table_lock);

struct devnum {
    struct driver *driver;
    hashtable_t device_table;
};

static dev_t makedev(unsigned int major, unsigned int minor)
{
    dev_t devnum;
    uint8_t *dev = (uint8_t*) &devnum;
    dev[0] = (uint8_t) major;
    dev[1] = (uint8_t) minor;
    return devnum;
}

static unsigned int major(dev_t dev)
{
    uint8_t *devnum = (uint8_t*) &dev;
    return devnum[0];
}

static unsigned int minor(dev_t dev)
{
    uint8_t *devnum = (uint8_t*) &dev;
    return devnum[1];
}

int devnum_alloc(struct driver *driver, struct device *device, dev_t *devnum)
{
    int retval = -ENOMEM;

    spinlock_lock(&driver_table_lock);

    for (unsigned i = 0; i < (sizeof(dev_t) / 2) << 8; i++) {
        struct devnum *dev = hashtable_get(&driver_table, (void*) i);

        if (dev && dev->driver != driver)
            continue;

        if (!dev) {
            dev = zalloc(sizeof(*dev));
            if (!dev) {
                retval = -ENOMEM;
                goto exit;
            }

            dev->driver = driver;
            hashtable_init_uint(&dev->device_table);
            hashtable_add(&driver_table, (void*) i, dev);
        }

        for (unsigned j = 0; j < (sizeof(dev_t) / 2) << 8; j++) {
            if (hashtable_has(&dev->device_table, (void*) j))
                continue;

            hashtable_add(&dev->device_table, (void*) j, device);

            *devnum = makedev(i, j);
            retval = 0;
            goto exit;
        }

        retval = -ENOMEM;
        goto exit;
    }

exit:
    spinlock_unlock(&driver_table_lock);
    return retval;
}

struct device *devnum_get_device(dev_t dev)
{
    struct devnum *devnum;
    struct device *device;

    spinlock_lock(&driver_table_lock);

    devnum = hashtable_get(&driver_table, (void*) major(dev));
    if (!devnum)
        return NULL;

    device = hashtable_get(&devnum->device_table, (void*) minor(dev));

    spinlock_unlock(&driver_table_lock);

    return device;
}

int device_register(struct device *dev)
{
    RET_IF_FAIL(dev, -EINVAL);

    list_init(&dev->list);
    list_add(&devices, &dev->list);

    if (is_probing_enabled) {
        device_driver_probe_all();
    }

    return 0;
}

int device_unregister(struct device *dev)
{
    RET_IF_FAIL(dev, -EINVAL);

    list_del(&dev->list);

    if (dev->driver) {
        struct driver *drv = driver_lookup(dev->driver);

        if (drv && drv->remove)
            drv->remove(dev);
    }

    return 0;
}

int driver_register(struct driver *driver)
{
    RET_IF_FAIL(driver, -EINVAL);
    RET_IF_FAIL(driver->name, -EINVAL);

    if (driver->init) {
        int retval = driver->init(driver);
        if (retval)
            return retval;
    }

    list_init(&driver->list);
    list_add(&drivers, &driver->list);

    kprintf("%s: registered driver\n", driver->name);

    return 0;
}

int driver_unregister(struct driver *driver)
{
    RET_IF_FAIL(driver, -EINVAL);

    list_del(&driver->list);

    return 0;
}

struct driver *driver_lookup(const char *name)
{
    if (!name)
        return NULL;

    list_foreach(&drivers, iter) {
        struct driver *driver = containerof(iter, struct driver, list);
        if (!strcmp(name, driver->name)) {
            return driver;
        }
    }

    return NULL;
}

struct list_head *device_get_list(void)
{
    return &devices;
}

static int device_probe(struct device *dev)
{
    struct driver *drv = driver_lookup(dev->driver);
    int retval;

    if (!drv || !drv->probe)
        return -EINVAL;

    retval = drv->probe(dev);
    if (retval)
        return retval;

    kprintf("%s: new %s device\n", dev->name, dev->description);
    return 0;
}

void device_driver_probe_all(void)
{
    is_probing_enabled = true;
    bool retry = true;
    int retval;

    while (!list_is_empty(&devices) && retry) {
        retry = false;

        list_foreach_safe(&devices, iter) {
            struct device *device = containerof(iter, struct device, list);
            list_del(&device->list);

            retval = device_probe(device);

            if (!retval) {
                retry = true;
                list_add(&probed_devices, &device->list);
            } else if (retval == -EAGAIN) {
                list_add(&devices, &device->list);
            }
        }
    }
}

void driver_init(void)
{
    extern uint32_t _driver;
    extern uint32_t _edriver;
    struct driver *drv;

    hashtable_init_uint(&driver_table);

    for (drv = (struct driver*) &_driver;
         drv != (struct driver*) &_edriver; drv++) {
        driver_register(drv);
    }
}
