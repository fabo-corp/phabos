#ifndef __HCD_DWC2_H__
#define __HCD_DWC2_H__

#include <config.h>
#include <phabos/usb/hcd.h>

struct dwc2_hcd {
    struct device device;
    struct usb_hcd hcd;

    void *base;
    int irq;
};

#endif /* __HCD_DWC2_H__ */

