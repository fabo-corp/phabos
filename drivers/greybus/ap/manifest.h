#ifndef __GREYBUS_AP_MANIFEST_H__
#define __GREYBUS_AP_MANIFEST_H__

#include <stdint.h>

struct gb_interface;
struct gb_manifest;

struct gb_manifest_hdr {
    uint16_t size;
    uint8_t version_major;
    uint8_t version_minor;
} __attribute__((packed));

struct gb_descriptor_hdr {
    uint16_t size;
    uint8_t type;
    uint8_t pad;
} __attribute__((packed));

struct gb_interface_descriptor {
    uint16_t size;
    uint8_t type;
    uint8_t pad;
    uint8_t vendor_string_id;
    uint8_t product_string_id;
    uint8_t pad2[2];
} __attribute__((packed));

struct gb_string_descriptor {
    uint16_t size;
    uint8_t type;
    uint8_t pad;
    uint8_t length;
    uint8_t id;
    uint8_t string[0];
} __attribute__((packed));

struct gb_bundle_descriptor {
    uint16_t size;
    uint8_t type;
    uint8_t pad;
    uint8_t id;
    uint8_t class;
    uint8_t pad2[2];
} __attribute__((packed));

struct gb_cport_descriptor {
    uint16_t size;
    uint8_t type;
    uint8_t pad;
    uint16_t id;
    uint8_t bundle;
    uint8_t protocol;
} __attribute__((packed));

enum {
    GB_DESCRIPTOR_INVALID       = 0,
    GB_DESCRIPTOR_INTERFACE     = 1,
    GB_DESCRIPTOR_STRING        = 2,
    GB_DESCRIPTOR_BUNDLE        = 3,
    GB_DESCRIPTOR_CPORT         = 4,
};

int manifest_parse(struct gb_interface *iface, struct gb_manifest *manifest);

#endif /* __GREYBUS_AP_MANIFEST_H__ */

