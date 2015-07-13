#ifndef __USB_HUB_H__
#define __USB_HUB_H__

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

#endif /* __USB_HUB_H__ */

