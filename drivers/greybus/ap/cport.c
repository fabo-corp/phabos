#include <phabos/greybus.h>
#include <phabos/greybus/ap.h>

#include "cport.h"
#include "bundle.h"
#include "interface.h"

#include <errno.h>

static struct hashtable cport_map;

int gb_cport_init(struct greybus *bus)
{
    dev_debug(&bus->device, "%zu cports available\n", bus->unipro->cport_count);

    /*
     * leave SVC cport 0 unallocated so that it will be allocated by
     * interface "self".
     */
    hashtable_init_uint(&cport_map);
    return 0;
}

int gb_cport_allocate(struct greybus *bus)
{
    for (unsigned i = 0; i < bus->unipro->cport_count; i++) {
        if (hashtable_has(&cport_map, (void*) i))
            continue;

        hashtable_add(&cport_map, (void*) i, (void*) 1);
        return i;
    }

    return -EBUSY;
}

void gb_cport_deallocate(struct greybus *bus, unsigned cport)
{
    if (hashtable_has(&cport_map, (void*) cport))
        hashtable_remove(&cport_map, (void*) cport);
}

struct gb_cport *gb_cport_create(unsigned id)
{
    struct gb_cport *cport = kzalloc(sizeof(*cport), MM_KERNEL);
    if (!cport)
        return NULL;

    cport->id = id;

    return cport;
}

void gb_cport_destroy(struct gb_cport *cport)
{
    if (!cport)
        return;

    kfree(cport);
}

int gb_cport_connect(struct gb_cport *cport)
{
    struct greybus *bus = cport->bundle->interface->bus;
    struct gb_protocol *protocol;
    struct gb_device *device;
    int cportid;
    int retval;

    if (!cport)
        return -EINVAL;

    protocol = gb_protocol_find(cport->protocol);
    if (!protocol)
        return -ENOTSUP;

    cportid = gb_cport_allocate(bus);
    if (cportid < 0) {
        dev_warn(&bus->device, "ran out of cports\n");
        return cportid;
    }

    device = gb_ap_create_device(bus, cportid);
    if (!device) {
        retval = -ENOMEM;
        goto error_create_device;
    }

    protocol->init_device(device);

    cport->connection.interface1 = gb_interface_self(bus);
    cport->connection.interface2 = cport->bundle->interface;
    cport->connection.cport1 = cportid;
    cport->connection.cport2 = cport->id;

    retval = gb_svc_create_connection(&cport->connection);
    if (retval) {
        dev_error(&bus->device,
                  "cannot create connection to interface %u\n",
                  cport->bundle->interface->id);
        goto error_create_connection;
    }

    device_register(&device->device);

    return 0;

error_create_connection:
    kfree(device);
error_create_device:
    gb_cport_deallocate(bus, cportid);

    return retval;
}
