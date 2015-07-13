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

int usb_control_msg(struct usb_device *dev, uint16_t typeReq, uint16_t wValue,
                    uint16_t wIndex, uint16_t wLength, void *buffer);
struct usb_device *usb_device_create(struct usb_hcd *hcd,
                                     struct usb_device *hub);

static inline unsigned int urb_pipe(int direction, int endpoint, int device,
                                    int xfer_type)
{
    return (direction << URB_DIRECTION_SHIFT) |
           (endpoint << URB_ENDPOINT_SHIFT)   |
           (device << URB_DEVICE_SHIFT)       |
           (xfer_type << URB_XFER_TYPE_SHIFT);
}

static inline bool urb_is_in(struct urb *urb)
{
    return urb->pipe & USB_HOST_DIR_IN;
}

static inline int urb_get_direction(struct urb *urb)
{
    return urb->pipe & USB_HOST_DIR_IN;
}

static inline bool urb_is_out(struct urb *urb)
{
    return !urb_is_in(urb);
}

static inline int urb_get_xfer_type(struct urb *urb)
{
    return (urb->pipe >> URB_XFER_TYPE_SHIFT) & 3;
}

static inline int urb_get_endpoint(struct urb *urb)
{
    return (urb->pipe >> URB_ENDPOINT_SHIFT) & 0xf;
}

static inline int urb_get_device(struct urb *urb)
{
    return (urb->pipe >> URB_DEVICE_SHIFT) & 0x7f;
}

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

