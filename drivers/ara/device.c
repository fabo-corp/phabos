/**
 * Copyright (c) 2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Mark Greer
 * @brief Low-level device driver interface
 */

#include <errno.h>
#include <string.h>

#include <asm/irq.h>

#include <phabos/ara/device.h>
#include <phabos/ara/device_table.h>

/**
 * @brief Open specified device
 * @param type Type device belongs to (e.g., GPIO, I2C, I2S, UART)
 * @param id ID or instance of device within its type
 * @return Address of structure representing device or NULL on failure
 */
struct device_ara *device_open(char *type, unsigned int id)
{
    struct device_ara *dev;
    int ret;

    if (!type)
        return NULL;

    irq_disable();

    device_table_for_each_dev(dev) {
        if (!strcmp(dev->type, type) && (dev->id == id)) {
            if (dev->state != DEVICE_STATE_PROBED)
                goto err_irqrestore;

            dev->state = DEVICE_STATE_OPENING;

            if (dev->driver->ops->open) {
                irq_enable();
                ret = dev->driver->ops->open(dev);
                irq_disable();

                if (ret) {
                    dev->state = DEVICE_STATE_PROBED;
                    goto err_irqrestore;
                }
            }

            dev->state = DEVICE_STATE_OPEN;
            break;
        }
    }

    irq_enable();

    return dev;

err_irqrestore:
    irq_enable();

    return NULL;
}

/**
 * @brief Close specified device
 * @param dev Address of structure representing device
 */
void device_close(struct device_ara *dev)
{
    if (!dev)
        return;

    irq_disable();

    if (dev->state != DEVICE_STATE_OPEN) {
        irq_enable();
        return;
    }

    dev->state = DEVICE_STATE_CLOSING;

    if (dev->driver->ops->close) {
        irq_enable();
        dev->driver->ops->close(dev);
        irq_disable();
    }

    dev->state = DEVICE_STATE_PROBED;

    irq_enable();
}

/**
 * @brief Register specified driver
 * @param driver Address of structure containing driver information
 * @return 0: Driver registered
 *         -errno: Negative errno value indicating reason for failure
 */
int device_register_driver(struct device_driver *driver)
{
    struct device_ara *dev;
    int ret;

    if (!driver || !driver->type || !driver->name || !driver->ops)
        return -EINVAL;

    irq_disable();

    device_table_for_each_dev(dev) {
        if (!strcmp(dev->type, driver->type) &&
            !strcmp(dev->name, driver->name)) {

            if (dev->state != DEVICE_STATE_REMOVED)
                continue;

            dev->driver = driver;
            dev->state = DEVICE_STATE_PROBING;

            if (driver->ops->probe) {
                irq_enable();
                ret = driver->ops->probe(dev);
                irq_disable();

                if (ret) {
                    dev->state = DEVICE_STATE_REMOVED;
                    dev->driver = NULL;
                    continue;
                }
            }

            dev->state = DEVICE_STATE_PROBED;
        }
    }

    irq_enable();

    return 0;
}

/**
 * @brief Unregister specified driver
 * @param driver Address of structure used to register the driver
 */
void device_unregister_driver(struct device_driver *driver)
{
    struct device_ara *dev;

    if (!driver)
        return;

    irq_disable();

    device_table_for_each_dev(dev) {
        if (dev->driver == driver) {
            if (dev->state != DEVICE_STATE_PROBED)
                continue;

            dev->state = DEVICE_STATE_REMOVING;

            if (driver->ops->remove) {
                irq_enable();
                driver->ops->remove(dev);
                irq_disable();
            }

            dev->driver = NULL;
            dev->state = DEVICE_STATE_REMOVED;
        }
    }

    irq_enable();
}
