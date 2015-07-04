/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <stddef.h>
#include <phabos/list.h>
#include <phabos/fs.h>

#define __driver__ __attribute__((section(".driver")))

struct device;
struct driver;

struct device {
    const char *name;
    const char *description;
    const char *driver;

    uintptr_t reg_base;
    int irq;

    struct file_operations ops;

    void *priv;

    struct list_head list;

    int (*power_on)(struct device *device);
    int (*power_off)(struct device *device);
};

struct driver {
    const char *name;
    void *priv;
    struct device *dev;
    struct list_head list;

    int (*init)(struct driver *drv);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
};

void device_driver_probe_all(void);

void driver_init(void);
int driver_register(struct driver *driver);
int driver_unregister(struct driver *driver);
struct driver *driver_lookup(const char *name);

int device_register(struct device *driver);
int device_unregister(struct device *dev);
struct list_head *device_get_list(void);

struct device *devnum_get_device(dev_t dev);
int devnum_alloc(struct driver *driver, struct device *device, dev_t *devnum);

int devfs_init(void);
int devfs_mknod(const char *name, mode_t mode, dev_t dev);

#endif /* __DRIVER_H__ */

