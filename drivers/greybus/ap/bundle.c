#include "bundle.h"
#include "cport.h"

#include <phabos/greybus.h>

#include <errno.h>

struct gb_bundle *gb_bundle_create(unsigned id)
{
    struct gb_bundle *bundle = kzalloc(sizeof(*bundle), MM_KERNEL);
    if (!bundle)
        return NULL;

    bundle->id = id;
    bundle->cports = hashtable_create_uint();

    return bundle;
}

void gb_bundle_destroy(struct gb_bundle *bundle)
{
    struct hashtable_iterator iter = HASHTABLE_ITERATOR_INIT;

    if (!bundle)
        return;

    while (hashtable_iterate(bundle->cports, &iter))
        gb_cport_destroy(iter.value);

    hashtable_destroy(bundle->cports);
    kfree(bundle);
}

int gb_bundle_init(struct gb_bundle *bundle)
{
    struct hashtable_iterator iter = HASHTABLE_ITERATOR_INIT;

    if (!bundle)
        return -EINVAL;

    while (hashtable_iterate(bundle->cports, &iter))
        gb_cport_connect(iter.value);

    return 0;
}
