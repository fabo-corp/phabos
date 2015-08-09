#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/greybus.h>

#include "control-gb.h"
#include "gpio-gb.h"

static void connect_gpio_cport(void)
{
    struct gb_control_connected_request *req;

    struct gb_operation *operation = gb_operation_create(0, GB_CONTROL_TYPE_CONNECTED, sizeof(*req));
    if (!operation)
        return;

    req = gb_operation_get_request_payload(operation);
    req->cport_id = 3;

    gb_operation_send_request(operation, NULL, true);
    gb_operation_destroy(operation);
}

static void toggle_gpio(void)
{
    struct gb_gpio_direction_out_request *req;

    struct gb_operation *operation = gb_operation_create(1, GB_GPIO_TYPE_DIRECTION_OUT, sizeof(*req));
    if (!operation)
        return;

    req = gb_operation_get_request_payload(operation);
    req->which = 0;
    req->value = 1;

    gb_operation_send_request(operation, NULL, false);
    gb_operation_destroy(operation);
}

int gb_ap_init(void)
{
    connect_gpio_cport();
    //connect_gpio_cport();
    //toggle_gpio();
    return 0;
}
