#include <phabos/greybus.h>

#include "interface.h"
#include "bundle.h"
#include "cport.h"
#include "control.h"
#include "manifest.h"
#include "svc.h"

struct gb_interface *gb_interface_create(struct greybus *bus, unsigned id)
{
    struct gb_interface *iface;
    int cport;

    iface = kzalloc(sizeof(*iface), MM_KERNEL);
    if (!iface)
        return NULL;

    iface->bus = bus;
    iface->id = id;
    iface->wq = workqueue_create(bus->device.name ? bus->device.name
                                                  : "greybus");
    if (!iface->wq)
        goto error_wq_create;

    list_init(&iface->connections);
    gb_connection_init(&iface->control_connection);

    cport = gb_cport_allocate(iface->bus);
    if (cport < 0) {
        dev_warn(&bus->device, "out of cports\n");
        goto error_control_cport_allocate;
    }

    iface->control_connection.interface1 = gb_interface_self(bus);
    iface->control_connection.cport1 = cport;
    iface->control_connection.interface2 = iface;
    iface->control_connection.cport2 = GB_CONTROL_CPORT;

    hashtable_init_uint(&iface->bundles);

    return iface;

error_control_cport_allocate:
error_wq_create:
    kfree(iface);
    return NULL;
}

void gb_interface_destroy(struct gb_interface *iface)
{
    struct hashtable_iterator iter = HASHTABLE_ITERATOR_INIT;

    if (!iface)
        return;

    while (hashtable_iterate(&iface->bundles, &iter))
        gb_bundle_destroy(iter.value);

    // TODO destroy all connections

    gb_cport_deallocate(iface->bus, iface->control_connection.cport1);
    workqueue_destroy(iface->wq);
    hashtable_deinit(&iface->bundles);

    kfree(iface);
}

static int gb_interface_initialize_bundles(struct gb_interface *iface)
{
    struct hashtable_iterator iter = HASHTABLE_ITERATOR_INIT;

    while (hashtable_iterate(&iface->bundles, &iter))
        gb_bundle_init(iter.value);

    return 0;
}

int gb_interface_init(struct gb_interface *iface)
{
    static unsigned next_devid = 2;
    int retval;
    struct gb_manifest manifest;
    struct gb_device *control_device;

    iface->device_id = next_devid++;

    retval = gb_svc_assign_device_id(iface);
    if (retval) {
        dev_error(&iface->bus->device,
                  "cannot assign device ID to interface %u\n", iface->id);
        return retval;
    }

    dev_debug(&iface->bus->device, "device ID %u assigned to interface %u\n",
              iface->device_id, iface->id);

    retval = gb_svc_create_route(iface);
    if (retval) {
        dev_error(&iface->bus->device, "cannot create route to interface %u\n",
                  iface->id);
        return retval;
    }

    dev_debug(&iface->bus->device, "route created AP <-> %u:%u\n", iface->id,
              iface->device_id);

    retval = gb_svc_create_connection(&iface->control_connection);
    if (retval) {
        dev_error(&iface->bus->device,
                  "cannot create control connection to interface %u\n",
                  iface->id);
        return retval;
    }

    control_device = gb_control_create_device(iface->bus,
                                              iface->control_connection.cport1);
    if (!control_device) {
        dev_error(&iface->bus->device,
                  "cannot create control device to interface %u\n", iface->id);
        return retval;
    }

    device_register(&control_device->device);
    gb_listen(iface->bus, iface->control_connection.cport1);

    retval = gb_control_get_manifest(iface, &manifest);
    if (retval) {
        dev_error(&iface->bus->device,
                  "cannot get manifest from interface %u\n", iface->id);
        return retval;
    }

    dev_debug(&iface->bus->device, "manifest size: %zu\n", manifest.size);

    retval = manifest_parse(iface, &manifest);
    if (retval) {
        dev_error(&iface->bus->device,
                  "failed to parse manifest of interface %u\n", iface->id);
        return retval;
    }

    dev_debug(&iface->bus->device, "parsed manifest successfully\n");

    gb_interface_initialize_bundles(iface);

    return 0;
}
