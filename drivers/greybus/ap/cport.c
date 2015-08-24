#include <phabos/greybus.h>

#include "cport.h"

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
