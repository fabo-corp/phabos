#ifndef __GREYBUS_AP_H__
#define __GREYBUS_AP_H__

#include <phabos/greybus.h>

struct greybus;

struct gb_ap {
    struct device device;

    struct greybus *bus;
    struct gb_device svc;
};

struct gb_device *gb_ap_create_device(struct greybus *bus, unsigned cport);

#endif /* __GREYBUS_AP_H__ */

