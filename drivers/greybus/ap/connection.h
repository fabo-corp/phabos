#ifndef __GREYBUS_AP_CONNECTION_H__
#define __GREYBUS_AP_CONNECTION_H__

#include <phabos/list.h>

struct gb_interface;

struct gb_connection {
    struct gb_interface *interface1;
    struct gb_interface *interface2;

    unsigned cport1;
    unsigned cport2;

    struct list_head list;
};

void gb_connection_init(struct gb_connection *connection);

#endif /* __GREYBUS_AP_CONNECTION_H__ */

