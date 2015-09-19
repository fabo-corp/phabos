/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <phabos/greybus.h>
#include <phabos/greybus/ap.h>
#include <phabos/gpio.h>

#include "../gpio-gb.h"

#include <errno.h>

static struct gb_device *gb_device;

static int gb_gpio_get_direction(struct gpio_device *dev, unsigned int line)
{
    struct gb_operation *op;
    struct gb_gpio_get_direction_request *req;
    struct gb_gpio_get_direction_response *resp;
    int retval;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_GPIO_TYPE_GET_DIRECTION, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->which = line;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EPROTO; // TODO write gb_op_result_to_errno
        goto out;
    }

    resp = gb_operation_get_request_payload(op->response);
    retval = resp->direction;

out:
    gb_operation_destroy(op);

    return retval;
}

static int gb_gpio_direction_in(struct gpio_device *dev, unsigned int line)
{
    struct gb_operation *op;
    struct gb_gpio_direction_in_request *req;
    int retval = 0;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_GPIO_TYPE_DIRECTION_IN, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->which = line;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EPROTO; // TODO write gb_op_result_to_errno
        goto out;
    }

out:
    gb_operation_destroy(op);

    return retval;
}

static int gb_gpio_direction_out(struct gpio_device *dev, unsigned int line,
                                 unsigned int value)
{
    struct gb_operation *op;
    struct gb_gpio_direction_out_request *req;
    int retval = 0;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_GPIO_TYPE_DIRECTION_OUT, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->which = line;
    req->value = value;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EPROTO; // TODO write gb_op_result_to_errno
        goto out;
    }

out:
    gb_operation_destroy(op);

    return retval;
}

static int gb_gpio_activate(struct gpio_device *dev, unsigned int line)
{
    struct gb_operation *op;
    struct gb_gpio_activate_request *req;
    int retval = 0;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_GPIO_TYPE_ACTIVATE, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->which = line;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EPROTO; // TODO write gb_op_result_to_errno
        goto out;
    }

out:
    gb_operation_destroy(op);

    return retval;
}

static int gb_gpio_deactivate(struct gpio_device *dev, unsigned int line)
{
    struct gb_operation *op;
    struct gb_gpio_deactivate_request *req;
    int retval = 0;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_GPIO_TYPE_DEACTIVATE, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->which = line;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EPROTO; // TODO write gb_op_result_to_errno
        goto out;
    }

out:
    gb_operation_destroy(op);

    return retval;
}

static int gb_gpio_set_value(struct gpio_device *dev, unsigned int line,
                             unsigned int value)
{
    struct gb_operation *op;
    struct gb_gpio_set_value_request *req;
    int retval = 0;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_GPIO_TYPE_SET_VALUE, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->which = line;
    req->value = value;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EPROTO; // TODO write gb_op_result_to_errno
        goto out;
    }

out:
    gb_operation_destroy(op);

    return retval;
}

static int gb_gpio_get_value(struct gpio_device *dev, unsigned int line)
{
    struct gb_operation *op;
    struct gb_gpio_get_value_request *req;
    struct gb_gpio_get_value_response *resp;
    int retval = 0;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_GPIO_TYPE_GET_VALUE, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->which = line;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EPROTO; // TODO write gb_op_result_to_errno
        goto out;
    }

    resp = gb_operation_get_request_payload(op->response);
    retval = resp->value;

out:
    gb_operation_destroy(op);

    return retval;
}

static struct gpio_ops gb_gpio_device_ops = {
    .get_direction = gb_gpio_get_direction,
    .direction_in = gb_gpio_direction_in,
    .direction_out = gb_gpio_direction_out,

    .activate = gb_gpio_activate,
    .deactivate = gb_gpio_deactivate,

    .get_value = gb_gpio_get_value,
    .set_value = gb_gpio_set_value,
};

void gb_gpio_line_count_cb(struct gb_operation *op)
{
    struct gb_gpio_line_count_response *resp;
    struct gpio_device *gpio;
    int retval;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EPROTO; // TODO write gb_op_result_to_errno
        goto error_operation_failed;
    }

    resp = gb_operation_get_request_payload(op->response);

    gpio = kzalloc(sizeof(*gpio), MM_KERNEL);
    if (!gpio) {
        retval = -ENOMEM;
        goto error_alloc_gpio_device;
    }

    gpio->base = -1;
    gpio->count = resp->count + 1;
    gpio->ops = &gb_gpio_device_ops;
    gpio->device.priv = op->greybus;

    device_register(&gpio->device);
    gpio_device_register(gpio);

    dev_info(&op->greybus->device, "new GPIO device: GPIO%d to GPIO%d\n",
             gpio->base, gpio->base + gpio->count);

error_alloc_gpio_device:
error_operation_failed:
    return;
}

static int gb_gpio_connected(struct greybus *bus, unsigned cport)
{
    struct gb_operation *op;

    op = gb_operation_create(bus, cport, GB_GPIO_TYPE_LINE_COUNT, 0);
    if (!op)
        return -ENOMEM;

    gb_operation_send_request(op, gb_gpio_line_count_cb, true);
    gb_operation_destroy(op);

    return 0;
}

static uint8_t gb_gpio_irq_event(struct gb_operation *op)
{
    return GB_OP_SUCCESS;
}

static struct gb_operation_handler gb_gpio_handlers[] = {
    GB_HANDLER(GB_GPIO_TYPE_IRQ_EVENT, gb_gpio_irq_event),
};

static struct gb_driver gpio_driver = {
    .op_handlers = (struct gb_operation_handler*) gb_gpio_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_gpio_handlers),
};

int gb_gpio_init_device(struct device *device)
{
    device->name = "gb-ap-gpio";
    device->description = "Greybus AP GPIO PHY Protocol";
    device->driver = "gb-ap-gpio-phy";

    return 0;
}

static struct gb_protocol gpio_protocol = {
    .id = GB_PROTOCOL_GPIO,
    .init_device = gb_gpio_init_device,
};

static int gb_gpio_probe(struct device *device)
{
    int retval;
    gb_device = containerof(device, struct gb_device, device);

    retval = gb_register_driver(gb_device->bus, gb_device->cport, &gpio_driver);
    if (retval)
        return retval;

    gb_listen(gb_device->bus, gb_device->cport);
    gb_gpio_connected(gb_device->bus, gb_device->cport);

    return 0;
}

static int gb_gpio_init(struct driver *driver)
{
    return gb_protocol_register(&gpio_protocol);
}

__driver__ struct driver gb_gpio_driver = {
    .name = "gb-ap-gpio-phy",
    .init = gb_gpio_init,
    .probe = gb_gpio_probe,
};
