#ifndef __USB_DRIVER_H__
#define __USB_DRIVER_H__

#include <phabos/list.h>

struct usb_class_driver {
    unsigned int class;
    struct list_head list;

    int (*init)(struct usb_device *device);
};

int usb_register_class_driver(struct usb_class_driver *driver);

#endif /* __USB_DRIVER_H__ */

