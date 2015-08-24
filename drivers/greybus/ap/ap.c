#include <phabos/greybus.h>

#include "ap.h"

struct gb_device *gb_ap_create_device(struct greybus *bus, unsigned cport)
{
    struct gb_device *dev = kzalloc(sizeof(*dev), MM_KERNEL);
    if (!dev)
        return NULL;

    dev->bus = bus;
    dev->cport = cport;

    return dev;
}
