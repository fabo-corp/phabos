#include <phabos/usb/hcd.h>

#include <errno.h>

enum usb_descritor_type {
    USB_DESCRIPTOR_HUB,
};

enum {
    USB_GET_STATUS          = 0,
    USB_CLEAR_FEATURE       = 1,
    USB_SET_FEATURE         = 3,
    USB_SET_ADDRESS         = 5,
    USB_GET_DESCRIPTOR      = 6,
    USB_SET_DESCRIPTOR      = 7,
    USB_GET_CONFIGURATION   = 8,
    USB_SET_CONFIGURATION   = 9,
    USB_GET_INTERFACE       = 10,
    USB_SET_INTERFACE       = 11,
    USB_SYNCH_FRAME         = 12,
};

enum usb_hub_class_request {
    USB_CLEAR_HUB_FEATURE   = 0x2000 | USB_CLEAR_FEATURE,
    USB_CLEAR_PORT_FEATURE  = 0x2300 | USB_CLEAR_FEATURE,
    USB_CLEAR_TT_BUFFER     = 0x2308,
    USB_GET_HUB_DESCRIPTOR  = 0xa000 | USB_GET_DESCRIPTOR,
    USB_GET_HUB_STATUS      = 0xa000 | USB_GET_STATUS,
    USB_GET_PORT_STATUS     = 0xa300 | USB_GET_STATUS,
    USB_RESET_TT            = 0x2309,
    USB_SET_HUB_DESCRIPTOR  = 0x2000 | USB_SET_FEATURE,
    USB_SET_PORT_FEATURE    = 0x2300 | USB_SET_FEATURE,
    USB_GET_TT_STATE        = 0xa30a,
    USB_STOP_TT             = 0x230b,
};

enum usb_hub_feature_selector {
    C_HUB_LOCAL_POWER   = 0,
    C_HUB_OVER_CURRENT  = 1,
};

enum usb_hub_port_feature_selector {
    PORT_CONNECTION     = 0,
    PORT_ENABLE         = 1,
    PORT_SUSPEND        = 2,
    PORT_OVER_CURRENT   = 3,
    PORT_RESET          = 4,
    PORT_POWER          = 8,
    PORT_LOW_SPEED      = 9,
    C_PORT_CONNECTION   = 16,
    C_PORT_ENABLE       = 17,
    C_PORT_SUSPEND      = 18,
    C_PORT_OVER_CURRENT = 19,
    C_PORT_RESET        = 20,
    PORT_TEST           = 21,
    PORT_INDICATOR      = 22,
};

struct usb_hub_descriptor {
    uint8_t bDescLength;
    uint8_t bDescriptorType;
    uint8_t bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t bPwrOn2PwrGood;
    uint8_t bHubContrCurrent;
};

struct usb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
};

struct usb_config_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
};

struct usb_string_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bString[0];
};

static int usb_enumerate_bus(struct usb_hcd *hcd)
{
#if 0
    int retval;

    char buf[64];
    struct usb_device_descriptor *device_desc =
        (struct usb_device_descriptor*) buf;
    struct usb_hub_descriptor *hub_desc = (struct usb_hub_descriptor*) buf;

    RET_IF_FAIL(hcd, -EINVAL);
    RET_IF_FAIL(hcd->hub_control);

    retval = hcd->hub_control(USB_GET_HUB_DESCRIPTOR, 0, 0, 2, device_desc);
    if (retval)
        return retval;

    if (device_desc->bDescriptorType != USB_DESCRIPTOR_HUB)
        return -EINVAL;

    // do stuff with hub_desc
#endif

    return 0;
}

int usb_hcd_register(struct usb_hcd *hcd)
{
    kprintf("%s()\n", __func__);

    return 0;
}


