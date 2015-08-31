#include <phabos/unipro.h>
#include <phabos/driver.h>
#include <phabos/usb.h>
#include <phabos/usb/driver.h>
#include <phabos/greybus.h>
#include <phabos/greybus/ap.h>

#include <errno.h>

struct apba_device {
    struct usb_device *usbdev;
    struct unipro_device *unipro;
    void *ep2;
    void *ep4;
};

static struct apba_device apba; // FIXME

static struct usb_device_id apba_unipro_usb_id[] = {
    {
        .match = USB_DRIVER_MATCH_VENDOR | USB_DRIVER_MATCH_PRODUCT,
        .vid = 0xffff,
        .pid = 0x2,
    },
    {},
};

static void apba_unipro_send_cb(struct urb *urb)
{
    apba.ep2 = urb->hcpriv_ep;
    //dwc_otg_hcd_qh_free(urb->hcpriv_ep);
    urb_destroy(urb);
}

static ssize_t apba_unipro_send(struct unipro_cport *cport, const void *buf,
                                size_t len)
{
    struct gb_operation_hdr *hdr = (void*) buf;
    struct urb *urb;
    ssize_t sent = len;
    int retval;

    hdr->pad[0] = cport->id & 0xff;

    urb = urb_create(apba.usbdev);
    if (!urb)
        return -ENOMEM;

    urb->complete = apba_unipro_send_cb;
    urb->pipe.direction = USB_HOST_DIR_OUT;
    urb->pipe.endpoint = 2;
    urb->pipe.device = apba.usbdev->address;
    urb->pipe.type = USB_HOST_PIPE_BULK;

    urb->flags = USB_URB_GIVEBACK_ASAP;
    urb->maxpacket = 0x200;
    urb->hcpriv_ep = apba.ep2;

    urb->length = len;
    urb->buffer = (void*) buf;

    retval = urb_submit(urb);
    if (retval) {
        kprintf("failed to send urb %d\n", retval);
        goto out;
    }

    return sent;

out:
    urb_destroy(urb);
    return retval;
}

static int apba_unipro_init_cport(struct unipro_cport *cport)
{
    return 0;
}

static int apba_unipro_unpause_rx(struct unipro_cport *cport)
{
    return 0;
}

static struct unipro_ops apba_unipro_ops = {
    .cport = {
        .send = apba_unipro_send,
        .unpause_rx = apba_unipro_unpause_rx,
        .init = apba_unipro_init_cport,
    },
};

static void apba_unipro_rx_cb(struct urb *urb)
{
    struct gb_operation_hdr *hdr;
    struct unipro_cport *cport;
    uint16_t cportid;

    if (urb->status) {
        urb->status = 0;
        goto out;
    }

    if (urb->actual_length < sizeof(*hdr)) {
        kprintf("%s(): dropping short urb\n", __func__);
        goto out;
    }

    hdr = urb->buffer;
    cportid = *(uint16_t*) &hdr->pad;

    if (cportid >= apba.unipro->cport_count) {
        goto out;
    }

    cport = &apba.unipro->cports[cportid];

    if (cport->driver && cport->driver->rx_handler) {
        cport->driver->rx_handler(cport->driver, cport->id, urb->buffer,
                                  urb->actual_length);
    }

out:
    urb->actual_length = 0;
    urb_submit(urb);
}

static struct greybus greybus = {
    .device = {
        .name = "greybus",
        .description = "Greybus",
        .driver = "greybus",
    },
};

static struct gb_ap gb_ap_device = {
    .bus = &greybus,

    .device = {
        .name = "gb-ap",
        .description = "Greybus AP",
        .driver = "gb-ap",
    },
};

static int test(struct usb_device *dev, unsigned endpoint)
{
    struct urb *urb;
    int retval;
    urb = urb_create(dev);
    if (!urb)
        return -ENOMEM;

    urb->complete = apba_unipro_rx_cb;
    urb->pipe.direction = USB_HOST_DIR_IN;
    urb->pipe.endpoint = endpoint;
    urb->pipe.device = dev->address;
    urb->pipe.type = USB_HOST_PIPE_BULK;

    urb->flags = USB_URB_GIVEBACK_ASAP;
    urb->maxpacket = 0x200;

    urb->length = 4096;
    urb->buffer = page_alloc(size_to_order(urb->length / PAGE_SIZE), MM_DMA);
    if (!urb->buffer) {
        retval = -ENOMEM;
        goto error_buffer_alloc;
    }

    retval = urb_submit(urb);
    if (retval) {
        goto error_urb_submit;
    }

    return 0;

error_urb_submit:
    page_free(urb->buffer, size_to_order(urb->length / PAGE_SIZE));
error_buffer_alloc:
    urb_destroy(urb);

    return retval;
}

static int apba_unipro_probe_device(struct usb_device *dev,
                                    struct usb_device_id *id)
{
    apba.usbdev = dev;

    test(dev, 3);

    greybus.unipro = apba.unipro;
    device_register(&greybus.device);
    device_register(&gb_ap_device.device);

    return 0;
}

static struct usb_driver apba_unipro_device_driver = {
    .id_table = apba_unipro_usb_id,
    .probe = apba_unipro_probe_device,
};

static int apba_unipro_probe(struct device *device)
{
    int retval;
    struct unipro_device *dev = to_unipro_device(device);

    apba.unipro = dev;

    retval = usb_register_driver(&apba_unipro_device_driver);
    if (retval)
        return retval;

    return unipro_register_device(dev, &apba_unipro_ops);
}

__driver__ struct driver apba_unipro_driver = {
    .name = "apba-unipro",
    .probe = apba_unipro_probe,
};
