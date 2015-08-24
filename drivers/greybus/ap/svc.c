#include <asm/byteordering.h>

#include <phabos/driver.h>
#include <phabos/greybus.h>
#include <phabos/greybus/svc.h>
#include <phabos/unipro/tsb.h>

#include "interface.h"
#include "cport.h"

#include <errno.h>

#define GB_SVC_CPORT        0
#define GB_AP_DEVICE_ID     1

static struct gb_device *dev;

int gb_svc_create_connection(struct gb_connection *connection)
{
    struct gb_svc_conn_create_request *req;
    struct gb_operation *op;
    int retval = 0;

    op = gb_operation_create(dev->bus, GB_SVC_CPORT, GB_SVC_TYPE_CONN_CREATE,
                             sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->intf1_id = connection->interface1->id;
    req->cport1_id = cpu_to_le16(connection->cport1);
    req->intf2_id = connection->interface2->id;
    req->cport2_id = cpu_to_le16(connection->cport2);
    req->flags = 0x7;

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

int gb_svc_destroy_connection(struct gb_connection *connection)
{
    struct gb_svc_conn_destroy_request *req;
    struct gb_operation *op;
    int retval = 0;

    op = gb_operation_create(dev->bus, GB_SVC_CPORT, GB_SVC_TYPE_CONN_DESTROY,
                             sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->intf1_id = connection->interface1->id;
    req->cport1_id = cpu_to_le16(connection->cport1);
    req->intf2_id = connection->interface2->id;
    req->cport2_id = cpu_to_le16(connection->cport2);

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

int gb_svc_create_route(struct gb_interface *iface)
{
    struct gb_svc_route_create_request *req;
    struct gb_operation *op;
    int retval = 0;

    op = gb_operation_create(dev->bus, GB_SVC_CPORT, GB_SVC_TYPE_ROUTE_CREATE,
                             sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->intf1_id = gb_interface_self(iface->bus)->id;
    req->dev1_id = GB_AP_DEVICE_ID;
    req->intf2_id = iface->id;
    req->dev2_id = iface->device_id;

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

int gb_svc_assign_device_id(struct gb_interface *iface)
{
    struct gb_svc_intf_device_id_request *req;
    struct gb_operation *op;
    int retval = 0;

    op = gb_operation_create(dev->bus, GB_SVC_CPORT, GB_SVC_TYPE_INTF_DEVICE_ID,
                             sizeof(*req));
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->intf_id = iface->id;
    req->device_id = iface->device_id;

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

static uint8_t gb_svc_protocol_version(struct gb_operation *op)
{
    struct gb_svc_protocol_version_response *resp;
    struct gb_svc_protocol_version_request *req =
        gb_operation_get_request_payload(op);

    if (gb_operation_get_request_payload_size(op) < sizeof(*req))
        return GB_OP_INVALID;

    dev_info(&op->greybus->device, "svc protocol %hhu:%hhu\n",
             req->major, req->minor);

    resp = gb_operation_alloc_response(op, sizeof(*resp));
    if (!resp)
        return GB_OP_NO_MEMORY;

    resp->major = GB_SVC_VERSION_MAJOR;
    resp->minor = GB_SVC_VERSION_MINOR;

    return GB_OP_SUCCESS;
}

static uint8_t gb_svc_hello(struct gb_operation *op)
{
    struct gb_interface *iface;
    struct gb_svc_hello_request *req = gb_operation_get_request_payload(op);

    if (gb_operation_get_request_payload_size(op) < sizeof(*req))
        return GB_OP_INVALID;

    dev_info(&op->greybus->device, "AP interface %hhu\n", req->interface_id);

    iface = gb_interface_create(op->greybus, req->interface_id);
    if (!iface)
        return GB_OP_NO_MEMORY;

    op->greybus->device.priv = iface;

    return GB_OP_SUCCESS;
}

static void interface_init(void *data)
{
    RET_IF_FAIL(data,);

    int retval = gb_interface_init(data);
    if (retval)
        gb_interface_destroy(data);
}

static uint8_t gb_svc_intf_hotplug(struct gb_operation *op)
{
    struct gb_svc_intf_hotplug_request *req =
        gb_operation_get_request_payload(op);
    struct gb_interface *iface;

    if (gb_operation_get_request_payload_size(op) < sizeof(*req))
        return GB_OP_INVALID;

    iface = gb_interface_create(op->greybus, req->intf_id);
    if (!iface)
        return GB_OP_NO_MEMORY;

    dev_info(&op->greybus->device, "interface %u plugged\n", iface->id);
    workqueue_queue(iface->wq, interface_init, iface);

    return GB_OP_SUCCESS;
}

static struct gb_operation_handler gb_svc_handlers[] = {
    GB_HANDLER(GB_SVC_TYPE_PROTOCOL_VERSION, gb_svc_protocol_version),
    GB_HANDLER(GB_SVC_TYPE_HELLO, gb_svc_hello),
    GB_HANDLER(GB_SVC_TYPE_INTF_HOTPLUG, gb_svc_intf_hotplug),
};

static struct gb_driver svc_driver = {
    .op_handlers = (struct gb_operation_handler*) gb_svc_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_svc_handlers),
};

static int gb_svc_probe(struct device *device)
{
    int retval;

    RET_IF_FAIL(device, -EINVAL);

    dev_debug_add_name("greybus");
    dev_debug_add_name("gb-ap-svc");
    dev_debug_add_name("gb-ap-gpio-phy");
    dev_debug_add_name("gb-ap-gpio-control");

    dev = containerof(device, struct gb_device, device);

    gb_cport_init(dev->bus);

    retval = gb_register_driver(dev->bus, GB_SVC_CPORT, &svc_driver);
    if (retval)
        return retval;

    retval = gb_listen(dev->bus, GB_SVC_CPORT);
    if (retval)
        return retval;

    dev_info(device, "AP is ready\n");
    extern struct unipro_device tsb_unipro;
    tsb_unipro_mbox_set(&tsb_unipro, TSB_MAIL_READY_AP, true);

    return 0;
}

__driver__ struct driver gb_svc_driver = {
    .name = "gb-ap-svc",
    .probe = gb_svc_probe,
};
