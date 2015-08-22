#include <asm/byteordering.h>

#include <phabos/driver.h>
#include <phabos/greybus.h>
#include <phabos/greybus/svc.h>
#include <phabos/unipro/tsb.h>
#include <phabos/workqueue.h>

#include "interface.h"

#include <errno.h>

#define GB_SVC_CPORT        0
#define GB_AP_DEVICE_ID     1
#define GB_CONTROL_CPORT    0

static uint8_t iface_id;
static uint8_t next_devid;
static uint16_t next_cport;
static struct workqueue *svc_wq;
static struct gb_device *dev;

static uint16_t control_cport;
static uint16_t gpio_cport;

static void gpio_connected_cb(struct gb_operation *op)
{
    dev_info(&op->greybus->device, "%s(): %hhu\n", __func__,
             gb_operation_get_request_result(op));

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        // TODO: destroy interface
        return;
    }

    dev_debug(&op->greybus->device, "gpio connected\n");

    int gb_gpio_connected(struct greybus *bus, unsigned cport);
    gb_gpio_connected(op->greybus, gpio_cport);
}

#include "../control-gb.h"

static void svc_create_gpio_connection_cb(struct gb_operation *op)
{
    struct gb_control_connected_request *req;
    struct greybus *bus = op->greybus;

    dev_info(&op->greybus->device, "%s(): %hhu\n", __func__,
             gb_operation_get_request_result(op));

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        // TODO: destroy interface
        return;
    }

    gb_listen(op->greybus, gpio_cport);

    dev_debug(&op->greybus->device, "gpio connection created\n");

    op = gb_operation_create(dev->bus, control_cport,
                             GB_CONTROL_TYPE_CONNECTED,
                             sizeof(*req));
    if (!op) {
        dev_error(&bus->device, "couldn't create operation\n");
        return;
    }

    req = gb_operation_get_request_payload(op);
    req->cport_id = cpu_to_le16(3);

    dev_debug(&bus->device, "connecting gpio on peer device...\n");
    gb_operation_send_request(op, gpio_connected_cb, true);
    gb_operation_destroy(op);
}

static void svc_create_gpio_connection(void *data)
{
    struct gb_interface *iface = data;
    struct gb_svc_conn_create_request *req;
    struct gb_operation *op;

    op = gb_operation_create(dev->bus, GB_SVC_CPORT, GB_SVC_TYPE_CONN_CREATE,
                             sizeof(*req));

    if (!op) {
        // TODO: destroy interface
        return;
    }

    gpio_cport = next_cport++;

    req = gb_operation_get_request_payload(op);
    req->intf1_id = iface_id;
    req->cport1_id = cpu_to_le16(gpio_cport);
    req->intf2_id = iface->id;
    req->cport2_id = cpu_to_le16(3);
    req->flags = 0x7;

    op->priv_data = iface;

    gb_operation_send_request(op, svc_create_gpio_connection_cb, true);
    gb_operation_destroy(op);
}

static void svc_create_control_connection_cb(struct gb_operation *op)
{
    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        // TODO: destroy interface
        return;
    }

    gb_listen(op->greybus, control_cport);

    dev_debug(&op->greybus->device, "control connection created\n");
}

static void svc_create_control_connection(void *data)
{
    struct gb_interface *iface = data;
    struct gb_svc_conn_create_request *req;
    struct gb_operation *op;

    op = gb_operation_create(dev->bus, GB_SVC_CPORT, GB_SVC_TYPE_CONN_CREATE,
                             sizeof(*req));

    if (!op) {
        // TODO: destroy interface
        return;
    }

    control_cport = next_cport++;

    req = gb_operation_get_request_payload(op);
    req->intf1_id = iface_id;
    req->cport1_id = cpu_to_le16(control_cport);
    req->intf2_id = iface->id;
    req->cport2_id = cpu_to_le16(GB_CONTROL_CPORT);
    req->flags = 0x7;

    op->priv_data = iface;

    gb_operation_send_request(op, svc_create_control_connection_cb, true);
    gb_operation_destroy(op);
}

static void svc_create_route_cb(struct gb_operation *op)
{
    dev_debug(&op->greybus->device, "route created %u\n", gb_operation_get_request_result(op));

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        // TODO: destroy interface
        return;
    }

    dev_debug(&op->greybus->device, "route created\n");

    workqueue_queue(svc_wq, svc_create_control_connection, op->priv_data);
    workqueue_queue(svc_wq, svc_create_gpio_connection, op->priv_data); // XXX
}

static void svc_create_route(void *data)
{
    struct gb_interface *iface = data;
    struct gb_svc_route_create_request *req;
    struct gb_operation *op;

    op = gb_operation_create(dev->bus, GB_SVC_CPORT, GB_SVC_TYPE_ROUTE_CREATE,
                             sizeof(*req));

    if (!op) {
        // TODO: destroy interface
        return;
    }

    req = gb_operation_get_request_payload(op);
    req->intf1_id = iface_id;
    req->dev1_id = GB_AP_DEVICE_ID;
    req->intf2_id = iface->id;
    req->dev2_id = iface->device_id;

    op->priv_data = iface;

    gb_operation_send_request(op, svc_create_route_cb, true);
    gb_operation_destroy(op);
}

static void svc_probe_interface_cb(struct gb_operation *op)
{
    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS) {
        // TODO: destroy interface
        return;
    }

    dev_debug(&op->greybus->device, "interface id assigned\n");

    workqueue_queue(svc_wq, svc_create_route, op->priv_data);
}

static void svc_probe_interface(void *data)
{
    struct gb_interface *iface = data;
    struct gb_svc_intf_device_id_request *req;
    struct gb_operation *op;

    op = gb_operation_create(dev->bus, GB_SVC_CPORT, GB_SVC_TYPE_INTF_DEVICE_ID,
                             sizeof(*req));

    if (!op) {
        // TODO: destroy interface
        return;
    }

    iface->device_id = next_devid++;

    req = gb_operation_get_request_payload(op);
    req->intf_id = iface->id;
    req->device_id = iface->device_id;

    op->priv_data = iface;

    gb_operation_send_request(op, svc_probe_interface_cb, true);
    gb_operation_destroy(op);
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
    struct gb_svc_hello_request *req = gb_operation_get_request_payload(op);

    if (gb_operation_get_request_payload_size(op) < sizeof(*req))
        return GB_OP_INVALID;

    dev_info(&op->greybus->device, "AP interface %hhu\n", req->interface_id);

    iface_id = req->interface_id;

    return GB_OP_SUCCESS;
}

static uint8_t gb_svc_intf_hotplug(struct gb_operation *op)
{
    struct gb_svc_intf_hotplug_request *req =
        gb_operation_get_request_payload(op);
    struct gb_interface *iface;

    if (gb_operation_get_request_payload_size(op) < sizeof(*req))
        return GB_OP_INVALID;

    iface = kzalloc(sizeof(*iface), MM_KERNEL);
    if (!iface)
        return GB_OP_NO_MEMORY;

    iface->id = req->intf_id;

    dev_info(&op->greybus->device, "new module on interface %u\n", iface->id);
    workqueue_queue(svc_wq, svc_probe_interface, iface);

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

    dev = containerof(device, struct gb_device, device);

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

static int gb_svc_init(struct driver *driver)
{
    svc_wq = workqueue_create("gb-ap-svc");
    if (!svc_wq)
        return -ENOMEM;

    next_cport = 1;
    next_devid = 2;

    return 0;
}

__driver__ struct driver gb_svc_driver = {
    .name = "gb-ap-svc",
    .init = gb_svc_init,
    .probe = gb_svc_probe,
};
