#include <errno.h>
#include <phabos/greybus.h>
#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/usb/hcd.h>
#include <phabos/usb/std-requests.h>
#include <phabos/usb/driver.h>

static struct usb_device_id greybus_usb_id[] = {
    {
        .match = USB_DRIVER_MATCH_VENDOR | USB_DRIVER_MATCH_PRODUCT,
        .vid = 0xffff,
        .pid = 0x0001,
    },
    {},
};

static struct usb_device *usbdev; // FIXME

int gb_ap_init(void);

void gb_usb_send_complete(struct urb *urb)
{
    kprintf("%s() = %d\n", __func__, urb->status);
}

void gb_usb_rx_complete(struct urb *urb)
{
    kprintf("%s() = %d\n", __func__, urb->status);
    urb->device->hcd->driver->urb_enqueue(usbdev->hcd, urb);
}

int gb_usb_send(unsigned int cportid, const void *buf, size_t len)
{
    struct urb *urb;

    kprintf("%s()\n", __func__);

    urb = urb_create(usbdev);

    urb->complete = gb_usb_send_complete;
    urb->pipe = (USB_HOST_PIPE_BULK << 30) | (2 << 15) |
                (usbdev->address << 8) | USB_HOST_DIR_OUT;
    urb->maxpacket = 0x40;
    urb->flags = USB_URB_GIVEBACK_ASAP;
    urb->buffer = (void*) buf;
    urb->length = len;

    struct gb_operation_hdr *hdr = (struct gb_operation_hdr*) buf;
    hdr->pad[1] = cportid >> 8;
    hdr->pad[0] = cportid & 0xff;

    usbdev->hcd->driver->urb_enqueue(usbdev->hcd, urb);

    return 0;
}

int gb_in(const void *buf, size_t len)
{
    struct urb *urb = urb_create(usbdev);

    kprintf("%s()\n", __func__);

    urb->complete = gb_usb_rx_complete;
    urb->pipe = (USB_HOST_PIPE_BULK << 30) | (3 << 15) |
                (usbdev->address << 8) | USB_HOST_DIR_IN;
    urb->buffer = (void*) buf;
    urb->length = len;
    urb->maxpacket = 0x40;
    urb->flags = USB_URB_GIVEBACK_ASAP;

    usbdev->hcd->driver->urb_enqueue(usbdev->hcd, urb);

    return 0;
}

void gb_usb_dev(void)
{
    kprintf("%s()\n", __func__);
}

static struct gb_transport_backend gb_usb_backend = {
    .init = gb_usb_dev,
    .send = gb_usb_send,
};

static int gb_usb_probe(struct usb_device *device, struct usb_device_id *id)
{
    usbdev = device;
    kprintf("%s()\n", __func__);
    print_descriptor(usbdev->config);
    return gb_ap_init();
}

static struct usb_driver greybus_usb_driver = {
    .id_table = greybus_usb_id,
    .probe = gb_usb_probe,
};

static int gb_usb_init(struct driver *driver)
{
    int retval;

    retval = usb_register_driver(&greybus_usb_driver);
    if (retval)
        return retval;

    retval = gb_init(&gb_usb_backend);
    if (retval)
        return retval;

    return 0;
}

__driver__ struct driver gb_usb_driver = {
    .name = "gb-usb",
    .init = gb_usb_init,
};
