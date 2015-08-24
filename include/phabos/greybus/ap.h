#ifndef __GREYBUS_AP_H__
#define __GREYBUS_AP_H__

#include <phabos/greybus.h>

struct greybus;

struct gb_ap {
    struct device device;

    struct greybus *bus;
    struct gb_device svc;
};

struct gb_protocol {
    unsigned id;

    int (*init_device)(struct device *device);
};

struct gb_device *gb_ap_create_device(struct greybus *bus, unsigned cport);
int gb_protocol_register(struct gb_protocol *protocol);
struct gb_protocol *gb_protocol_find(unsigned id);

enum {
    GB_PROTOCOL_CONTROL     = 0x00,
    GB_PROTOCOL_AP          = 0x01,
    GB_PROTOCOL_GPIO        = 0x02,
    GB_PROTOCOL_I2C         = 0x03,
    GB_PROTOCOL_UART        = 0x04,
    GB_PROTOCOL_HID         = 0x05,
    GB_PROTOCOL_USB         = 0x06,
    GB_PROTOCOL_SDIO        = 0x07,
    GB_PROTOCOL_BATTERY     = 0x08,
    GB_PROTOCOL_PWM         = 0x09,
    GB_PROTOCOL_I2S_MGMT    = 0x0a,
    GB_PROTOCOL_SPI         = 0x0b,
    GB_PROTOCOL_DISPLAY     = 0x0c,
    GB_PROTOCOL_CAMERA      = 0x0d,
    GB_PROTOCOL_SENSOR      = 0x0e,
    GB_PROTOCOL_LIGHTS      = 0x0f,
    GB_PROTOCOL_VIBRATOR    = 0x10,
    GB_PROTOCOL_LOOPBACK    = 0x11,
    GB_PROTOCOL_I2S_RX      = 0x12,
    GB_PROTOCOL_I2S_TX      = 0x13,
    GB_PROTOCOL_RAW         = 0xfe,
};

#endif /* __GREYBUS_AP_H__ */

