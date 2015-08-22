#include <phabos/greybus.h>

#include "../control-gb.h"

#include <errno.h>

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
    struct gb_device *gb_device = containerof(device, struct gb_device, device);
    return gb_register_driver(gb_device->bus, 1, &control_driver);
}

__driver__ struct driver gb_control_driver = {
    .name = "gb-ap-control",
    .probe = gb_control_probe,
};
