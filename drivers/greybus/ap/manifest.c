#include <asm/byteordering.h>

#include <phabos/greybus.h>

#include "interface.h"
#include "cport.h"
#include "bundle.h"
#include "manifest.h"

#include <errno.h>

static ssize_t manifest_parse_interface(struct gb_interface *iface,
                                        const void *data, size_t max_size)
{
    const struct gb_interface_descriptor *desc = data;

    if (sizeof(*desc) > max_size || le16_to_cpu(desc->size) > max_size)
        return -EINVAL;

    return le16_to_cpu(desc->size);
}

static ssize_t manifest_parse_string(struct gb_interface *iface,
                                     const void *data, size_t max_size)
{
    const struct gb_string_descriptor *desc = data;

    if (sizeof(*desc) > max_size || le16_to_cpu(desc->size) > max_size)
        return -EINVAL;

    return le16_to_cpu(desc->size);
}

static ssize_t manifest_parse_bundle(struct gb_interface *iface,
                                     const void *data, size_t max_size)
{
    const struct gb_bundle_descriptor *desc = data;
    struct gb_bundle *bundle;

    if (sizeof(*desc) > max_size || le16_to_cpu(desc->size) > max_size)
        return -EINVAL;

    if (desc->class == GB_CLASS_CONTROL)
        goto out;

    bundle = gb_bundle_create(desc->id);
    if (!bundle)
        goto out;

    bundle->class = desc->class;
    hashtable_add(&iface->bundles, (void*) bundle->id, bundle);

out:
    return le16_to_cpu(desc->size);
}

static ssize_t manifest_parse_cport(struct gb_interface *iface,
                                    const void *data, size_t max_size)
{
    const struct gb_cport_descriptor *desc = data;
    struct gb_cport *cport;
    struct gb_bundle *bundle;

    if (sizeof(*desc) > max_size || le16_to_cpu(desc->size) > max_size)
        return -EINVAL;

    if (desc->id == GB_CONTROL_CPORT)
        goto out;

    uintptr_t bundle_id = desc->bundle;
    bundle = hashtable_get(&iface->bundles, (void*) bundle_id);
    if (!bundle)
        goto out;

    cport = gb_cport_create(le16_to_cpu(desc->id));
    if (!bundle)
        goto out;

    cport->protocol = desc->protocol;
    cport->bundle = desc->bundle;
    hashtable_add(&bundle->cports, (void*) cport->id, cport);

out:
    return le16_to_cpu(desc->size);
}

int manifest_parse(struct gb_interface *iface, struct gb_manifest *manifest)
{
    const struct gb_manifest_hdr *hdr = manifest->data;
    uintptr_t addr = (uintptr_t) manifest->data;
    const struct gb_descriptor_hdr *desc;
    unsigned off = sizeof(hdr);
    size_t manifest_size;

    if (manifest->size < sizeof(*hdr) + sizeof(*desc))
        return -EINVAL;

    manifest_size = le16_to_cpu(hdr->size);

    if (manifest->size < manifest_size)
        return -EINVAL;

    while (off + sizeof(*desc) <= manifest_size) {
        desc = (void*) (addr + off);
        ssize_t nread;

        switch (desc->type) {
        case GB_DESCRIPTOR_INTERFACE:
            nread = manifest_parse_interface(iface, desc, manifest_size - off);
            break;

        case GB_DESCRIPTOR_STRING:
            nread = manifest_parse_string(iface, desc, manifest_size - off);
            break;

        case GB_DESCRIPTOR_BUNDLE:
            nread = manifest_parse_bundle(iface, desc, manifest_size - off);
            break;

        case GB_DESCRIPTOR_CPORT:
            nread = manifest_parse_cport(iface, desc, manifest_size - off);
            break;

        default:
            dev_error(&iface->bus->device,
                      "unknown descriptor type found: %hhu\n", desc->type);
            nread = -EINVAL;
        }

        if (nread <= 0) {
            dev_error(&iface->bus->device, "invalid manifest\n");
            return -EINVAL;
        }

        off += nread;
    }

    // FIXME: giant HACK
    off = sizeof(hdr);
    while (off + sizeof(*desc) <= manifest_size) {
        desc = (void*) (addr + off);
        ssize_t nread;

        switch (desc->type) {
        case GB_DESCRIPTOR_INTERFACE:
        case GB_DESCRIPTOR_STRING:
        case GB_DESCRIPTOR_BUNDLE:
            nread = le16_to_cpu(desc->size);
            break;

        case GB_DESCRIPTOR_CPORT:
            nread = manifest_parse_cport(iface, desc, manifest_size - off);
            break;
        }

        if (nread <= 0) {
            dev_error(&iface->bus->device, "invalid manifest\n");
            return -EINVAL;
        }

        off += nread;
    }

    return 0;
}
