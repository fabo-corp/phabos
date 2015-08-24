#ifndef __GREYBUS_AP_BUNDLE_H__
#define __GREYBUS_AP_BUNDLE_H__

#include <phabos/hashtable.h>

struct gb_bundle {
    unsigned id;
    unsigned class;

    struct gb_interface *interface;
    struct hashtable cports;
};

enum {
    GB_CLASS_CONTROL    = 0,
    GB_CLASS_SVC        = 1,
    GB_CLASS_GPIO       = 2,
    GB_CLASS_I2C        = 3,
    GB_CLASS_UART       = 4,
    GB_CLASS_HID        = 5,
    GB_CLASS_USB        = 6,
    GB_CLASS_SDIO       = 7,
    GB_CLASS_BATTERY    = 8,
    GB_CLASS_PWM        = 9,
    GB_CLASS_I2S        = 10,
    GB_CLASS_SPI        = 11,
    GB_CLASS_DISPLAY    = 12,
    GB_CLASS_CAMERA     = 13,
    GB_CLASS_SENSOR     = 14,
    GB_CLASS_LIGHTS     = 15,
    GB_CLASS_VIBRATOR   = 16,
};

struct gb_bundle *gb_bundle_create(unsigned id);
void gb_bundle_destroy(struct gb_bundle *bundle);

#endif /* __GREYBUS_AP_BUNDLE_H__ */

