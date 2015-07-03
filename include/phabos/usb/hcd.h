#ifndef __USB_HCD_H__
#define __USB_HCD_H__

#include <stdint.h>

struct usb_hcd {
    struct usb_hc_driver *driver;

};

struct usb_hc_driver {
    int (*start)(struct usb_hcd *hcd);
    int (*stop)(struct usb_hcd *hcd);

    int (*hub_control)(struct usb_hcd *hcd, uint16_t typeReq, uint16_t wIndex,
                       uint16_t wValue, uint16_t wLength, char *buf);
};

int usb_hcd_register(struct usb_hcd *hcd);

#endif /* __USB_HCD_H__ */

