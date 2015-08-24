#ifndef __GREYBUS_AP_CPORT_H__
#define __GREYBUS_AP_CPORT_H__

#include <phabos/greybus.h>

#include "connection.h"

struct gb_cport {
    unsigned id;
    unsigned protocol;
    unsigned bundle_id;

    struct gb_bundle *bundle;
    struct gb_connection connection;
};

#define GB_CONTROL_CPORT    0

int gb_cport_init(struct greybus *bus);
struct gb_cport *gb_cport_create(unsigned id);
void gb_cport_destroy(struct gb_cport *cport);
int gb_cport_allocate(struct greybus *bus);
void gb_cport_deallocate(struct greybus *bus, unsigned cport);
int gb_cport_connect(struct gb_cport *cport);

#endif /* __GREYBUS_AP_CPORT_H__ */

