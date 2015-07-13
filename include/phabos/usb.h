/*
 * Copyright (c) 2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 */

#ifndef __PHABOS_USB_H__
#define __PHABOS_USB_H__

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include <asm/atomic.h>
#include <phabos/utils.h>
#include <phabos/semaphore.h>

#define USB_SPEED_LOW                   1
#define USB_SPEED_FULL                  2
#define USB_SPEED_HIGH                  3

#define USB_URB_GIVEBACK_ASAP           1
#define USB_URB_SEND_ZERO_PACKET        2

#define USB_HOST_DIR_OUT                0
#define USB_HOST_DIR_IN                 0x80

#define USB_HOST_PIPE_ISOCHRONOUS       0
#define USB_HOST_PIPE_INTERRUPT         1
#define USB_HOST_PIPE_CONTROL           2
#define USB_HOST_PIPE_BULK              3

#define usb_host_pipein(pipe)           ((pipe) & USB_HOST_DIR_IN)
#define usb_host_pipeout(pipe)          (!usb_host_pipein(pipe))

#define usb_host_pipedevice(pipe)       (((pipe) >> 8) & 0x7f)
#define usb_host_pipeendpoint(pipe)     (((pipe) >> 15) & 0xf)

#define usb_host_pipetype(pipe)         (((pipe) >> 30) & 3)
#define usb_host_pipeisoc(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_ISOCHRONOUS)
#define usb_host_pipeint(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_INTERRUPT)
#define usb_host_pipecontrol(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_CONTROL)
#define usb_host_pipebulk(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_BULK)

#define USB_HOST_ENDPOINT_XFER_CONTROL  0
#define USB_HOST_ENDPOINT_XFER_ISOC     1
#define USB_HOST_ENDPOINT_XFER_BULK     2
#define USB_HOST_ENDPOINT_XFER_INT      3

#define URB_DIRECTION_SHIFT 0
#define URB_DEVICE_SHIFT    8
#define URB_ENDPOINT_SHIFT  15
#define URB_XFER_TYPE_SHIFT 30

struct urb;
typedef void (*urb_complete_t)(struct urb *urb);

enum usb_descritor_type {
    USB_DESCRIPTOR_HUB = 0x29,
};

enum usb_device_class {
    USB_DEVICE_CLASS_HUB = 0x9,
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

struct usb_device {
    int address;
    int speed;
    int ttport;
    int port;

    struct usb_hcd *hcd;
    struct usb_device *hub;
};

struct urb {
    atomic_t refcount;
    struct semaphore semaphore;
    urb_complete_t complete;

    void *urb;
    unsigned int pipe;
    unsigned int flags;

    int status;

    size_t length;
    size_t actual_length;

    unsigned int maxpacket;

    int interval;

    uint8_t setup_packet[8];
    void *buffer;

    struct usb_device *device;

    void *hcpriv;
    void *hcpriv_ep;
};

static inline struct urb *urb_create(struct usb_device *dev) // FIXME
{
    struct urb *urb;

    urb = zalloc(sizeof(*urb));
    if (!urb) {
        return NULL;
    }

    atomic_init(&urb->refcount, 1);
    semaphore_init(&urb->semaphore, 0);

    urb->device = dev;

    return urb;
}

static inline void urb_ref(struct urb *urb)
{
    RET_IF_FAIL(urb,);
    atomic_inc(&urb->refcount);
}

static inline void urb_unref(struct urb *urb)
{
    if (!urb) {
        return;
    }

    if (atomic_dec(&urb->refcount)) {
        return;
    }

    kfree(urb);
}

static inline void urb_destroy(struct urb *urb)
{
    urb_unref(urb);
}

#endif /* __PHABOS_USB_H__ */

