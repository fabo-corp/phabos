#ifndef __GREYBUS_AP_INTERFACE_H__
#define __GREYBUS_AP_INTERFACE_H__

#include <phabos/workqueue.h>
#include <phabos/hashtable.h>
#include <phabos/greybus.h>

#include "connection.h"

struct gb_interface {
    struct greybus *bus;

    unsigned id;
    unsigned device_id;
    struct hashtable bundles;
    struct workqueue *wq;

    struct gb_connection control_connection;

    struct list_head connections;
};

struct gb_interface *gb_interface_create(struct greybus *bus, unsigned id);
void gb_interface_destroy(struct gb_interface *iface);
int gb_interface_init(struct gb_interface *iface);

static inline struct gb_interface *gb_interface_self(struct greybus *bus)
{
    return bus->device.priv;
}

#endif /* __GREYBUS_AP_INTERFACE_H__ */

