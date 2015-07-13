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

#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#include <asm/irq.h>
#include <phabos/driver.h>
#include <phabos/utils.h>
#include <phabos/usb.h>
#include <phabos/usb/hcd-dwc2.h>

#include "../dwc2/dwc_otg_driver.h"
#include "../dwc2/dwc_otg_hcd_if.h"
#include "../dwc2/dwc_otg_hcd.h"

#define REG_OFFSET 0xFFFFFFFF

static dwc_otg_device_t *g_dev;

static int _hub_info(dwc_otg_hcd_t *hcd, void *urb_handle, uint32_t *hub_addr,
                     uint32_t *port_addr)
{
    struct urb *urb = urb_handle;

    if (!urb) {
        return -EINVAL;
    }

    if (hub_addr) {
        *hub_addr = urb->device->address;
    }

    if (port_addr) {
        *port_addr = urb->device->ttport;
    }

    return 0;
}


static int _speed(dwc_otg_hcd_t *hcd, void *urb_handle)
{
    struct urb *urb = urb_handle;

    if (!urb) {
        return -EINVAL;
    }

    return urb->device->speed;
}

static int _complete(dwc_otg_hcd_t *hcd, void *urb_handle,
             dwc_otg_hcd_urb_t *dwc_urb, int32_t status)
{
    struct urb *urb = urb_handle;

    if (!hcd || !urb || !dwc_urb) {
        return -EINVAL;
    }

    urb->actual_length = dwc_otg_hcd_urb_get_actual_length(dwc_urb);
    urb->status = status;

    free(dwc_urb);

    DWC_SPINUNLOCK(hcd->lock);

    if (urb->complete) {
        urb->complete(urb);
    } else {
        free(urb); // FIXME unref
    }

    DWC_SPINLOCK(hcd->lock);

    return 0;
}

static struct dwc_otg_hcd_function_ops hcd_fops = {
    .hub_info = _hub_info,
    .speed = _speed,
    .complete = _complete,
};

/**
 * HSIC IRQ Handler
 *
 * @param irq IRQ number
 * @param context context of the preempted task
 * @return 0 if successful
 */
static void hsic_irq_handler(int irq, void *data)
{
    RET_IF_FAIL(g_dev,);
    RET_IF_FAIL(g_dev->hcd,);

//    kprintf("Z");

    dwc_otg_handle_common_intr(g_dev);
    dwc_otg_hcd_handle_intr(g_dev->hcd);
}

/**
 * Initialize the USB HCD controller
 * @return 0 if succesful
 */
static int hcd_init(void)
{
    int retval;

    RET_IF_FAIL(g_dev, -EINVAL);
    RET_IF_FAIL(!g_dev->hcd, -EINVAL);
    RET_IF_FAIL(g_dev->core_if, -EINVAL);

    g_dev->hcd = dwc_otg_hcd_alloc_hcd();
    if (!g_dev->hcd) {
        return -ENOMEM;
    }

    retval = dwc_otg_hcd_init(g_dev->hcd, g_dev->core_if);
    if (retval) {
        goto error_hcd_init;
    }

    g_dev->hcd->otg_dev = g_dev;
    dwc_otg_hcd_set_priv_data(g_dev->hcd, NULL);

    return 0;

error_hcd_init:
    dwc_otg_hcd_remove(g_dev->hcd);
    g_dev->hcd = NULL;
    return retval;
}

/**
 * Initialize the USB HCD core
 *
 * @return 0 if successful
 */
static int hcd_core_init(struct usb_hcd *hcd)
{
    int retval;

    RET_IF_FAIL(hcd, -EINVAL);
    RET_IF_FAIL(!g_dev, -EINVAL);

    g_dev = zalloc(sizeof(*g_dev));
    if (!g_dev) {
        return -ENOMEM;
    }

    g_dev->os_dep.reg_offset = REG_OFFSET;
    g_dev->os_dep.base = (void *) hcd->device.reg_base;
    g_dev->common_irq_installed = 1;

    g_dev->core_if = dwc_otg_cil_init(g_dev->os_dep.base);
    if (!g_dev->core_if) {
        retval = -EIO;
        goto error_cil_init;
    }

    if (set_parameters(g_dev->core_if)) {
        retval = -EIO;
        goto error_set_parameter;
    }

    dwc_otg_disable_global_interrupts(g_dev->core_if);

    dwc_otg_core_init(g_dev->core_if);

    retval = hcd_init();
    if (retval) {
        goto error_hcd_init;
    }

    if (dwc_otg_get_param_adp_enable(g_dev->core_if)) {
        dwc_otg_adp_start(g_dev->core_if, dwc_otg_is_host_mode(g_dev->core_if));
    } else {
        dwc_otg_enable_global_interrupts(g_dev->core_if);
    }

    irq_attach(hcd->device.irq, hsic_irq_handler, NULL);
    irq_enable_line(hcd->device.irq);

    return 0;

error_hcd_init:
error_set_parameter:
    dwc_otg_cil_remove(g_dev->core_if);
error_cil_init:
    free(g_dev);
    g_dev = NULL;

    return retval;
}

static void hcd_core_deinit(struct usb_hcd *hcd)
{
    RET_IF_FAIL(hcd,);
    RET_IF_FAIL(g_dev,);
    RET_IF_FAIL(g_dev->hcd,);
    RET_IF_FAIL(g_dev->core_if,);

    irq_disable_line(hcd->device.irq);
    irq_detach(hcd->device.irq);

    dwc_otg_hcd_remove(g_dev->hcd);
    dwc_otg_cil_remove(g_dev->core_if);

    free(g_dev);
    g_dev = NULL;
}

/**
 * Start the HCD
 *
 * @param dev: usb host device
 * @return 0 if successful
 */
static int hcd_start(struct usb_hcd *hcd)
{
    int retval;

    gpio_direction_out(0, 1);

    retval = hcd_core_init(hcd);
    if (retval)
        return retval;

    retval = dwc_otg_hcd_start(g_dev->hcd, &hcd_fops);
    if (retval)
        return retval;

    dwc_otg_set_hsic_connect(g_dev->hcd->core_if, hcd->has_hsic_phy);

    return 0;
}

/**
 * Stop the HCD
 *
 * @param dev: usb host device
 */
static int hcd_stop(struct usb_hcd *hcd)
{
    RET_IF_FAIL(g_dev, -EINVAL);
    RET_IF_FAIL(g_dev->hcd, -EINVAL);

    dwc_otg_hcd_stop(g_dev->hcd);

    hcd_core_deinit(hcd);
    return 0;
}

static int urb_enqueue(struct usb_hcd *hcd, struct urb *urb)
{
    int retval;
    uint8_t ep_type;
    int number_of_packets = 0;
    dwc_otg_hcd_urb_t *dwc_urb;

    switch (usb_host_pipetype(urb->pipe)) {
    case USB_HOST_PIPE_CONTROL:
        ep_type = USB_HOST_ENDPOINT_XFER_CONTROL;
        break;

    case USB_HOST_PIPE_ISOCHRONOUS:
        ep_type = USB_HOST_ENDPOINT_XFER_ISOC;
        break;

    case USB_HOST_PIPE_BULK:
        ep_type = USB_HOST_ENDPOINT_XFER_BULK;
        break;

    case USB_HOST_PIPE_INTERRUPT:
        ep_type = USB_HOST_ENDPOINT_XFER_INT;
        break;

    default:
        return -EINVAL;
    }

    dwc_urb = dwc_otg_hcd_urb_alloc(g_dev->hcd, number_of_packets, 0);
    if (!dwc_urb) {
        return -ENOMEM;
    }

    urb->hcpriv = dwc_urb;

    dwc_otg_hcd_urb_set_pipeinfo(dwc_urb,
                                 usb_host_pipedevice(urb->pipe),
                                 usb_host_pipeendpoint(urb->pipe), ep_type,
                                 usb_host_pipein(urb->pipe),
                                 urb->maxpacket);

    dwc_otg_hcd_urb_set_params(dwc_urb, urb, urb->buffer,
                               (dwc_dma_t) urb->buffer, urb->length,
                               &urb->setup_packet,
                               (dwc_dma_t) &urb->setup_packet,
                               urb->flags, urb->interval);

    retval = dwc_otg_hcd_urb_enqueue(g_dev->hcd, dwc_urb, &urb->hcpriv_ep, 0);
    if (retval) {
        goto error_enqueue;
    }

    return 0;

error_enqueue:
    free(dwc_urb);

    return retval;
}

/**
 * Send request to the root hub
 *
 * @param dev: usb host device
 * @param buf: buffer of length @a wLength where the response will get stored
 * @return 0 if successful
 *
 * @see USB Specification for the meaning of typeReq, wValue, wIndex, and
 * wLength
 */
static int hub_control(struct usb_hcd *dev, uint16_t typeReq, uint16_t wValue,
                       uint16_t wIndex, uint16_t wLength, char *buf)
{
    RET_IF_FAIL(g_dev, -EINVAL);
    RET_IF_FAIL(g_dev->hcd, -EINVAL);

    return dwc_otg_hcd_hub_control(g_dev->hcd, typeReq, wValue, wIndex,
                                   (uint8_t*) buf, wLength);
}

static struct usb_hc_driver dwc2_hcd_driver = {
    .start = hcd_start,
    .stop = hcd_stop,
    .urb_enqueue = urb_enqueue,
    .hub_control = hub_control,
};

static int dwc2_probe(struct device *device)
{
    struct usb_hcd *hcd = containerof(device, struct usb_hcd, device);

    if (device->power_on)
        device->power_on(device);

    hcd->driver = &dwc2_hcd_driver;

    return usb_hcd_register(hcd);
}

static int dwc2_remove(struct device *device)
{
    return 0;
}

__driver__ struct driver dwc2_hc_driver = {
    .name = "dw-usb2-hcd",

    .probe = dwc2_probe,
    .remove = dwc2_remove,
};
