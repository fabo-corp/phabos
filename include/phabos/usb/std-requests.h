#ifndef __USB_STD_REQUESTS_H__
#define __USB_STD_REQUESTS_H__

#include <stdint.h>

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

enum usb_descriptor_type {
    USB_DESCRIPTOR_DEVICE                       = 1,
    USB_DESCRIPTOR_CONFIGURATION                = 2,
    USB_DESCRIPTOR_STRING                       = 3,
    USB_DESCRIPTOR_INTERFACE                    = 4,
    USB_DESCRIPTOR_ENDPOINT                     = 5,
    USB_DESCRIPTOR_DEVICE_QUALIFIER             = 6,
    USB_DESCRIPTOR_OTHER_SPEED_CONFIGURATION    = 7,
    USB_DESCRIPTOR_INTERFACE_POWER              = 8,
};

enum usb_device_class_request {
    USB_DEVICE_CLEAR_FEATURE        = 0x0000 | USB_CLEAR_FEATURE,
    USB_DEVICE_GET_CONFIGURATION    = 0x8000 | USB_GET_CONFIGURATION,
    USB_DEVICE_GET_DESCRIPTOR       = 0x8000 | USB_GET_DESCRIPTOR,
    USB_DEVICE_GET_INTERFACE        = 0x8100 | USB_GET_INTERFACE,
    USB_DEVICE_GET_STATUS           = 0x8000 | USB_GET_STATUS,
    USB_DEVICE_SET_ADDRESS          = 0x0000 | USB_SET_ADDRESS,
    USB_DEVICE_SET_CONFIGURATION    = 0x0000 | USB_SET_CONFIGURATION,
    USB_DEVICE_SET_DESCRIPTOR       = 0x0000 | USB_SET_DESCRIPTOR,
    USB_DEVICE_SET_FEATURE          = 0x0000 | USB_SET_FEATURE,
    USB_DEVICE_SET_INTERFACE        = 0x0100 | USB_SET_INTERFACE,
    USB_DEVICE_SYNCH_FRAME          = 0x8200 | USB_SYNCH_FRAME,
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

struct usb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
};

struct usb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
};


#endif /* __USB_STD_REQUESTS_H__ */

