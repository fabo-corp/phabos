#include <asm/byteordering.h>
#include <phabos/greybus.h>
#include <phabos/greybus/ap.h>

#include "../control-gb.h"
#include "control.h"
#include "interface.h"

#include <errno.h>

struct gb_device *gb_control_create_device(struct greybus *bus, unsigned cport)
{
    struct gb_device *dev;

    dev = gb_ap_create_device(bus, cport);
    if (!dev)
        return NULL;

    dev->device.name = "gb-ap-control";
    dev->device.description = "Greybus AP Control Protocol";
    dev->device.driver = "gb-ap-control";

    return dev;
}

static ssize_t gb_control_get_manifest_size(struct gb_interface *iface)
{
    struct gb_control_get_manifest_size_response *resp;
    struct gb_operation *op;
    ssize_t retval;

    op = gb_operation_create(iface->bus, iface->control_connection.cport1,
                             GB_CONTROL_TYPE_GET_MANIFEST_SIZE, 0);
    if (!op)
        return -ENOMEM;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EIO;
        goto out;
    }

    resp = gb_operation_get_request_payload(op->response);
    retval = le16_to_cpu(resp->size);

out:
    gb_operation_destroy(op);
    return retval;
}

static int gb_control_get_manifest_data(struct gb_interface *iface,
                                        struct gb_manifest *manifest)
{
    struct gb_control_get_manifest_response *resp;
    struct gb_operation *op;
    ssize_t retval;

    op = gb_operation_create(iface->bus, iface->control_connection.cport1,
                             GB_CONTROL_TYPE_GET_MANIFEST, 0);
    if (!op)
        return -ENOMEM;

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EIO;
        goto out;
    }

    resp = gb_operation_get_request_payload(op->response);
    memcpy(manifest->data, resp->data, manifest->size);

out:
    gb_operation_destroy(op);
    return retval;
}

int gb_control_get_manifest(struct gb_interface *iface,
                            struct gb_manifest *manifest)
{
    int retval;
    ssize_t size;

    size = gb_control_get_manifest_size(iface);
    if (size < 0)
        return size;

    if (size == 0)
        return -EIO;

    manifest->size = size;
    manifest->data = kmalloc(size, MM_KERNEL);
    if (!manifest->data)
        return -ENOMEM;

    retval = gb_control_get_manifest_data(iface, manifest);
    if (retval)
        goto error_get_manifest_data;

    return 0;

error_get_manifest_data:
    kfree(manifest->data);
    manifest->data = NULL;

    return retval;
}

int gb_control_connect_cport(struct gb_interface *iface, unsigned cport)
{
    struct gb_control_connected_request *req;
    struct gb_operation *op;
    ssize_t retval;

    op = gb_operation_create(iface->bus, iface->control_connection.cport1,
                             GB_CONTROL_TYPE_CONNECTED, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->cport_id = cpu_to_le16(cport);

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EIO;
        goto out;
    }

out:
    gb_operation_destroy(op);
    return retval;
}

int gb_control_disconnect_cport(struct gb_interface *iface, unsigned cport)
{
    struct gb_control_connected_request *req;
    struct gb_operation *op;
    ssize_t retval;

    op = gb_operation_create(iface->bus, iface->control_connection.cport1,
                             GB_CONTROL_TYPE_DISCONNECTED, sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->cport_id = cpu_to_le16(cport);

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        retval = -EIO;
        goto out;
    }

out:
    gb_operation_destroy(op);
    return retval;
}

static uint8_t gb_control_probe_ap(struct gb_operation *op)
{
    return GB_OP_SUCCESS;
}

static struct gb_operation_handler gb_control_handlers[] = {
    GB_HANDLER(GB_CONTROL_TYPE_PROBE_AP, gb_control_probe_ap),
};

struct gb_driver control_driver = {
    .op_handlers = (struct gb_operation_handler*) gb_control_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_control_handlers),
};

static int gb_control_probe(struct device *device)
{
    struct gb_device *dev = containerof(device, struct gb_device, device);
    return gb_register_driver(dev->bus, dev->cport, &control_driver);
}

__driver__ struct driver gb_control_driver = {
    .name = "gb-ap-control",
    .probe = gb_control_probe,
};
