/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <errno.h>

#include <phabos/mm.h>
#include <phabos/unipro.h>

int unipro_register_cport_driver(struct unipro_device *device, unsigned cport,
                                 struct unipro_cport_driver *driver)
{
    int retval;

    if (!device || !device->cports || !driver)
        return -EINVAL;

    if (cport >= device->cport_count)
        return -EINVAL;

    device->cports[cport].driver = driver;

    retval = device->ops->cport.init(&device->cports[cport]);
    if (retval)
        goto err_cport_init;

    dev_info(&device->device, "registered driver on cport %u\n", cport);
    return 0;

err_cport_init:
    device->cports[cport].driver = NULL;
    return retval;
}

int unipro_unregister_cport_driver(struct unipro_device *device, unsigned cport)
{
    if (!device || !device->cports)
        return -EINVAL;

    if (cport >= device->cport_count)
        return -EINVAL;

    device->cports[cport].driver = NULL;

    dev_info(&device->device, "unregistered driver on cport %u\n", cport);
    return 0;
}

int unipro_register_device(struct unipro_device *device, struct unipro_ops *ops)
{
    struct unipro_cport *cports;

    if (!device || !ops)
        return -EINVAL;

    cports = kzalloc(sizeof(*cports) * device->cport_count, MM_KERNEL);
    if (!cports)
        return -ENOMEM;

    device->cports = cports;
    device->ops = ops;

    for (unsigned i = 0; i < device->cport_count; i++) {
        cports[i].id = i;
        cports[i].device = device;
        list_init(&cports[i].tx_fifo);
    }

    return 0;
}
