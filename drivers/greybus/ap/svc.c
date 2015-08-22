#include <phabos/driver.h>
#include <phabos/greybus.h>
#include <phabos/greybus/svc.h>

#include <phabos/unipro/tsb.h>

#include <errno.h>

#define GB_SVC_CPORT        0

static uint8_t iface_id;

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

    if (gb_operation_get_request_payload_size(op) < sizeof(*req))
        return GB_OP_INVALID;

    dev_info(&op->greybus->device, "new module on interface %hhu\n",
             req->intf_id);

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
    struct gb_device *dev;

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

__driver__ struct driver gb_svc_driver = {
    .name = "gb-ap-svc",
    .probe = gb_svc_probe,
};
