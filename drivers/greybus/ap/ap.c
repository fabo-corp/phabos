#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/usb/hcd.h>
#include <phabos/usb/std-requests.h>
#include <phabos/usb/driver.h>

void print_descriptor(void *raw_descriptor);

static int gb_ap_init_bus(struct usb_device *dev)
{
    void *buffer = malloc(255);
    struct usb_device_descriptor *desc = buffer;

    usb_control_msg(dev, USB_DEVICE_GET_DESCRIPTOR,
                    USB_DESCRIPTOR_DEVICE << 8, 0, sizeof(*desc), desc);

    if (desc->idVendor != 0xffff)
        return -EINVAL;

    usb_control_msg(dev, USB_DEVICE_GET_DESCRIPTOR,
                    USB_DESCRIPTOR_CONFIGURATION << 8, 0, 255, buffer);

    print_descriptor(buffer);

    return 0;
}

static struct usb_class_driver hub_class_driver = {
    .class = 0,
    .init = gb_ap_init_bus,
};

static int gb_ap_usb_init(struct driver *driver)
{
    return usb_register_class_driver(&hub_class_driver);
}

__driver__ struct driver gb_ap_usb_driver = {
    .name = "gb-ap-usb",
    .init = gb_ap_usb_init,
};
