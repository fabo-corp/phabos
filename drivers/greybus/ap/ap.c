#include <phabos/greybus.h>
#include <phabos/greybus/ap.h>
#include <phabos/unipro/tsb.h>
#include <phabos/hashtable.h>

#include "cport.h"
#include "bundle.h"

static struct hashtable protocol_map;
static bool is_protocol_map_initialized;

struct gb_device *gb_ap_create_device(struct greybus *bus, unsigned cport)
{
    struct gb_device *dev = kzalloc(sizeof(*dev), MM_KERNEL);
    if (!dev)
        return NULL;

    dev->bus = bus;
    dev->cport = cport;

    return dev;
}

int gb_protocol_register(struct gb_protocol *protocol)
{
    if (!is_protocol_map_initialized) {
        hashtable_init_uint(&protocol_map);
        is_protocol_map_initialized = true;
    }

    hashtable_add(&protocol_map, (void*) protocol->id, protocol);
    return 0;
}

struct gb_protocol *gb_protocol_find(unsigned id)
{
    if (!is_protocol_map_initialized)
        return NULL;

    return hashtable_get(&protocol_map, (void*) id);
}

static int gb_ap_probe(struct device *device)
{
    struct gb_ap *ap = containerof(device, struct gb_ap, device);

    dev_debug_add_name("greybus");
    dev_debug_add_name("gb-ap");
    dev_debug_add_name("gb-ap-svc");
    dev_debug_add_name("gb-ap-gpio-phy");
    dev_debug_add_name("gb-ap-gpio-control");

    ap->svc.bus = ap->bus;
    ap->svc.device.name = "gb-ap-svc",
    ap->svc.device.description = "Greybus AP SVC Protocol",
    ap->svc.device.driver = "gb-ap-svc",

    gb_cport_init(ap->bus);
    device_register(&ap->svc.device);

    dev_info(device, "AP is ready\n");
    extern struct unipro_device tsb_unipro;
    tsb_unipro_mbox_set(&tsb_unipro, TSB_MAIL_READY_AP, true);

    return 0;
}

__driver__ struct driver gb_ap_driver = {
    .name = "gb-ap",
    .probe = gb_ap_probe,
};
