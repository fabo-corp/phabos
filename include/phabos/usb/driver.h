#ifndef __USB_DRIVER_H__
#define __USB_DRIVER_H__

#include <phabos/list.h>

#define USB_DRIVER_MATCH_VENDOR                 (1 << 0)
#define USB_DRIVER_MATCH_PRODUCT                (1 << 1)
#define USB_DRIVER_MATCH_DEVICE_CLASS           (1 << 2)
#define USB_DRIVER_MATCH_DEVICE_SUBCLASS        (1 << 3)
#define USB_DRIVER_MATCH_DEVICE_PROTOCOL        (1 << 4)
#define USB_DRIVER_MATCH_INTERFACE_CLASS        (1 << 5)
#define USB_DRIVER_MATCH_INTERFACE_SUBCLASS     (1 << 6)
#define USB_DRIVER_MATCH_INTERFACE_PROTOCOL     (1 << 7)

struct usb_device_id {
    uint16_t vid;
    uint16_t pid;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    uint8_t iclass;
    uint8_t isubclass;
    uint8_t iprotocol;

    unsigned match;
};

struct usb_driver {
    struct list_head list;
    struct usb_device_id *id_table;

    int (*probe)(struct usb_device *device, struct usb_device_id *id);
};

int usb_register_driver(struct usb_driver *driver);

#endif /* __USB_DRIVER_H__ */

