#include "bundle.h"
#include "cport.h"

#include <phabos/greybus.h>

struct gb_bundle *gb_bundle_create(unsigned id)
{
    struct gb_bundle *bundle = kzalloc(sizeof(*bundle), MM_KERNEL);
    if (!bundle)
        return NULL;

    bundle->id = id;
    hashtable_init_uint(&bundle->cports);

    return bundle;
}

void gb_bundle_destroy(struct gb_bundle *bundle)
{
    struct hashtable_iterator iter = HASHTABLE_ITERATOR_INIT;

    if (!bundle)
        return;

    while (hashtable_iterate(&bundle->cports, &iter))
        gb_cport_destroy(iter.value);

    hashtable_deinit(&bundle->cports);
    kfree(bundle);
}
