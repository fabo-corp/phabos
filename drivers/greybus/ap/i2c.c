#include <asm/byteordering.h>

#include <phabos/greybus.h>
#include <phabos/greybus/ap.h>
#include <phabos/i2c.h>

#include "../i2c-gb.h"

#include <errno.h>

extern struct driver gb_i2c_driver;
static struct gb_device *gb_device;

static ssize_t gb_i2c_transfer(struct i2c_master *master, struct i2c_msg *msg,
                               size_t count)
{
    struct gb_operation *op;
    struct gb_i2c_transfer_req *req;
    int retval = 0;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_I2C_PROTOCOL_TRANSFER,
                             sizeof(*req) + count * sizeof(req->desc[0]));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->op_count = cpu_to_le16(count);

    for (unsigned i = 0; i < count; i++) {
        req->desc[i].addr = cpu_to_le16(msg->addr);
        req->desc[i].size = cpu_to_le16(msg->length);
        req->desc[i].flags = cpu_to_le16(msg->flags); // FIXME
    }

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS)
        retval = -EIO;

out:
    gb_operation_destroy(op);

    return retval;
}

static struct i2c_master_ops gb_i2c_ops = {
    .transfer = gb_i2c_transfer,
};

static struct gb_operation_handler gb_i2c_handlers[] = {
};

static struct gb_driver i2c_driver = {
    .op_handlers = (struct gb_operation_handler*) gb_i2c_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_i2c_handlers),
};

static int gb_i2c_init_device(struct device *device)
{
    device->name = "gb-ap-i2c";
    device->description = "Greybus AP I2C PHY Protocol";
    device->driver = "gb-ap-i2c-phy";

    return 0;
}

static struct gb_protocol i2c_protocol = {
    .id = GB_PROTOCOL_I2C,
    .init_device = gb_i2c_init_device,
};

static int gb_i2c_probe(struct device *device)
{
    int retval;
    struct i2c_master *master;
    dev_t devnum;

    gb_device = containerof(device, struct gb_device, device);

    retval = gb_register_driver(gb_device->bus, gb_device->cport, &i2c_driver);
    if (retval)
        return retval;

    gb_listen(gb_device->bus, gb_device->cport);

    master = kzalloc(sizeof(*master), MM_KERNEL);
    if (!master)
        return -ENOMEM;

    master->ops = &gb_i2c_ops;
    master->device.driver = gb_i2c_driver.name;

    retval = devnum_alloc(&gb_i2c_driver, &master->device, &devnum);
    if (retval)
        goto error_denum_alloc;

    return i2c_master_register(master, devnum);

error_denum_alloc:
    kfree(master);
    return retval;
}

static int gb_i2c_init(struct driver *driver)
{
    return gb_protocol_register(&i2c_protocol);
}

__driver__ struct driver gb_i2c_driver = {
    .name = "gb-ap-i2c-phy",
    .init = gb_i2c_init,
    .probe = gb_i2c_probe,
};
