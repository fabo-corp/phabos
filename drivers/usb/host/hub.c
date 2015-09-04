#include <asm/delay.h>
#include <phabos/workqueue.h>
#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/usb/hcd.h>
#include <phabos/usb/std-requests.h>
#include <phabos/usb/driver.h>

#include "hub.h"
#include "device.h"

static struct usb_device_id hub_usb_id[] = {
    {
        .match = USB_DRIVER_MATCH_DEVICE_CLASS,
        .class = 0x9,
    },
    {},
};

static struct workqueue *hub_wq;

struct usb_hub_device {
    struct usb_device *self;

    struct usb_device port[0];
};

static int hub_check_changes(struct usb_device *dev, struct urb *urb);

static void hub_update_status(void *data)
{
    struct urb *urb = data;
    uint8_t change_status = *(uint8_t*) urb->buffer;
    uint32_t status;

    if (!urb->status)
        kprintf("hub changes: %X\n", change_status);

    change_status >>= 1;

    for (int i = 1; change_status; i++, change_status >>= 1) {
        if (!(change_status & 1))
            continue;

        usb_control_msg(urb->device, USB_GET_PORT_STATUS, 0, i, sizeof(status),
                        &status);

        kprintf("port status: %X\n", status);

        if (status & (1 << 16)) {
            usb_control_msg(urb->device, USB_CLEAR_PORT_FEATURE, C_PORT_CONNECTION, i,
                            0, NULL);
            kprintf("Port connection changed\n");

            if (status & (1 << PORT_CONNECTION)) {
                struct usb_device *dev = usb_device_create(urb->device->hcd, urb->device);
                if (!dev)
                    continue;

                dev->port = i;
                dev->speed = USB_SPEED_FULL;
                if (status & (1 << 9))
                    dev->speed = USB_SPEED_LOW;
                if (status & (1 << 10))
                    dev->speed = USB_SPEED_HIGH;

                usb_control_msg(urb->device, USB_SET_PORT_FEATURE, PORT_RESET, i, 0, NULL);

                mdelay(1000);

                usb_control_msg(urb->device, USB_GET_PORT_STATUS, 0, i, sizeof(status),
                                &status);

                enumerate_device(dev);
            } else {
                kprintf("Port disconnected\n");
            }

            usb_control_msg(urb->device, USB_GET_PORT_STATUS, 0, i, sizeof(status),
                            &status);

            kprintf("port status: %X\n", status);
        }

        if (status & (1 << 19)) {
            usb_control_msg(urb->device, USB_CLEAR_PORT_FEATURE,
                            C_PORT_OVER_CURRENT, i, 0, NULL);
            kprintf("Port over current\n");
        }

        if (status & (1 << 20)) {
            usb_control_msg(urb->device, USB_CLEAR_PORT_FEATURE, C_PORT_RESET, i, 0,
                            NULL);
            kprintf("Port reset\n");
        }
    }

    hub_check_changes(urb->device, urb);
}

static void hub_state_changed(struct urb *urb)
{
    kprintf("%s() = %d\n", __func__, urb->status);
    workqueue_queue(hub_wq, hub_update_status, urb);
}

static int hub_check_changes(struct usb_device *dev, struct urb *urb)
{
    RET_IF_FAIL(dev, -EINVAL);
    RET_IF_FAIL(dev->hcd, -EINVAL);
    RET_IF_FAIL(dev->hcd->driver, -EINVAL);
    RET_IF_FAIL(dev->hcd->driver->urb_enqueue, -EINVAL);

    if (!urb) {
        urb = urb_create(dev);
        RET_IF_FAIL(urb, -ENOMEM);

        urb->length = 4;
        urb->buffer = kmalloc(urb->length, MM_DMA);
        if (!urb->buffer) {
            urb_destroy(urb);
            return -ENOMEM;
        }
    }

    urb->status = 0;
    urb->actual_length = 0;
    urb->complete = hub_state_changed;
    urb->pipe.type = USB_HOST_PIPE_INTERRUPT;
    urb->pipe.endpoint = 1;
    urb->pipe.device = dev->address;
    urb->pipe.direction = USB_HOST_DIR_IN;
    urb->maxpacket = 0x40;
    urb->flags = 1;

    return dev->hcd->driver->urb_enqueue(dev->hcd, urb);
}

static int enumerate_hub(struct usb_device *hub)
{
    int retval;
    struct usb_hub_descriptor *desc;
    struct usb_device *dev;
    uint32_t status;

    retval = usb_control_msg(hub, USB_DEVICE_SET_CONFIGURATION, 1,
                             0, 0, NULL);
    if (retval < 0)
        return retval; // FIXME: unpower device port

    desc = kmalloc(sizeof(*desc), MM_KERNEL);
    RET_IF_FAIL(desc, -ENOMEM);

    retval = usb_control_msg(hub, USB_GET_HUB_DESCRIPTOR, 0, 0, sizeof(*desc),
                             desc);
    if (retval < 0)
        return retval;

    kprintf("%s: found new hub with %u ports.\n", hub->hcd->device.name,
                                                  desc->bNbrPorts);

    for (int i = 1; i <= desc->bNbrPorts; i++) {
        usb_control_msg(hub, USB_SET_PORT_FEATURE, PORT_POWER, i, 0, NULL);

        mdelay(desc->bPwrOn2PwrGood * 2);

        usb_control_msg(hub, USB_GET_PORT_STATUS, 0, i, sizeof(status),
                        &status);

        if (!(status & (1 << PORT_CONNECTION)))
            continue;

        dev = usb_device_create(hub->hcd, hub);
        if (!dev)
            continue;

        usb_control_msg(hub, USB_SET_PORT_FEATURE, PORT_RESET, i, 0, NULL);

        mdelay(1000);

        usb_control_msg(hub, USB_GET_PORT_STATUS, 0, i, sizeof(status),
                        &status);

        if (status & (1 << 16)) {
            usb_control_msg(hub, USB_CLEAR_PORT_FEATURE, C_PORT_CONNECTION, i,
                            0, NULL);
        }

        if (status & (1 << 20)) {
            usb_control_msg(hub, USB_CLEAR_PORT_FEATURE, C_PORT_RESET, i, 0,
                            NULL);
        }

        dev->port = i;
        dev->speed = USB_SPEED_FULL;
        if (status & (1 << 9))
            dev->speed = USB_SPEED_LOW;
        if (status & (1 << 10))
            dev->speed = USB_SPEED_HIGH;

        enumerate_device(dev);
    }

    //hub_check_changes(hub, NULL);

    return 0;
}

static int hub_probe_device(struct usb_device *dev, struct usb_device_id *id)
{
    return enumerate_hub(dev);
}

static struct usb_driver hub_device_driver = {
    .id_table = hub_usb_id,
    .probe = hub_probe_device,
};

static int hub_init(struct driver *driver)
{
    hub_wq = workqueue_create("usb-hubd");
    return usb_register_driver(&hub_device_driver);
}

__driver__ struct driver usb_hub_driver = {
    .name = "usb2-hub",
    .init = hub_init,
};
