#include <asm/byteordering.h>
#include <asm/spinlock.h>
#include <asm/delay.h>
#include <phabos/usb/hcd.h>
#include <phabos/usb/std-requests.h>
#include <phabos/usb/driver.h>
#include <phabos/utils.h>
#include <phabos/assert.h>
#include <phabos/list.h>

#include "hub.h"
#include "device.h"

#include <errno.h>
#include <string.h>

static atomic_t dev_id;
static struct list_head usbdev_driver = LIST_INIT(usbdev_driver);
static struct spinlock usbdev_driver_lock = SPINLOCK_INIT(spinlock);

static int device_probe(struct usb_device *device);

void print_descriptor(void *raw_descriptor)
{
    char *header = raw_descriptor;
    struct usb_device_descriptor *dev_desc = raw_descriptor;
    struct usb_string_descriptor *str_desc = raw_descriptor;
    struct usb_config_descriptor *config_desc = raw_descriptor;
    struct usb_interface_descriptor *iface_desc = raw_descriptor;
    struct usb_endpoint_descriptor *endpoint_desc = raw_descriptor;

    switch (header[1]) {
    case 1:
        kprintf("Device:\n");
        kprintf("\tdevice class: %u:%u\n", dev_desc->bDeviceClass, dev_desc->bDeviceSubClass);
        kprintf("\tdevice protocol: %u\n", dev_desc->bDeviceProtocol);
        kprintf("\tmax packet size: %u\n", dev_desc->bMaxPacketSize0);
        kprintf("\tvendor id: %X\n", dev_desc->idVendor);
        kprintf("\tproduct id: %X\n", dev_desc->idProduct);
        kprintf("\tbcd device: %X\n", dev_desc->bcdDevice);
        kprintf("\tmanufacturer index: %u\n", dev_desc->iManufacturer);
        kprintf("\tproduct index: %u\n", dev_desc->iProduct);
        kprintf("\tserial number index: %u\n", dev_desc->iSerialNumber);
        kprintf("\t# configuration: %u\n", dev_desc->bNumConfigurations);
        break;

    case 2:
        kprintf("Configuration:\n");
        kprintf("\ttotal descriptor length: %u\n", config_desc->wTotalLength);
        kprintf("\t# interface: %u\n", config_desc->bNumInterfaces);
        kprintf("\tconfig value: %u\n", config_desc->bConfigurationValue);
        kprintf("\tiConfiguration: %u\n", config_desc->iConfiguration);
        kprintf("\tattributes: %X\n", config_desc->bmAttributes);
        kprintf("\tmax power: %X\n", config_desc->bMaxPower);

        int size = config_desc->wTotalLength;
        while (size > 0) {
            dev_desc = (struct usb_device_descriptor*)
                ((char*) dev_desc + dev_desc->bLength);
            size -= dev_desc->bLength;
            print_descriptor(dev_desc);
        }
        break;

    case 3:
        kprintf("String: ");
        for (int i = 0; i < str_desc->bLength; i += 2)
            kprintf("%c", ((char*) str_desc->bString)[i]);
        kprintf("\n");
        break;

    case 4:
        kprintf("Interface:\n");
        kprintf("\tinterface #: %u\n", iface_desc->bInterfaceNumber);
        kprintf("\talternate setting: %u\n", iface_desc->bAlternateSetting);
        kprintf("\t# of endpoints: %u\n", iface_desc->bNumEndpoints);
        kprintf("\tinterface class: %u:%u\n", iface_desc->bInterfaceClass, iface_desc->bInterfaceSubClass);
        kprintf("\tinterface protocol: %u\n", iface_desc->bInterfaceProtocol);
        kprintf("\tiInterface: %u\n", iface_desc->iInterface);
        break;

    case 5:
        kprintf("Endpoint:\n");
        kprintf("\tendpoint address: %u\n", endpoint_desc->bEndpointAddress);
        kprintf("\tattributes: %X\n", endpoint_desc->bmAttributes);
        break;
    }
}

static void usb_control_msg_callback(struct urb *urb)
{
    RET_IF_FAIL(urb,);
    semaphore_up(&urb->semaphore);
}

ssize_t usb_control_msg(struct usb_device *dev, uint16_t typeReq,
                        uint16_t wValue, uint16_t wIndex, uint16_t wLength,
                        void *buffer)
{
    int retval;
    struct urb *urb;
    int direction;

    RET_IF_FAIL(dev, -EINVAL);
    RET_IF_FAIL(dev->hcd, -EINVAL);
    RET_IF_FAIL(dev->hcd->driver, -EINVAL);
    RET_IF_FAIL(dev->hcd->driver->urb_enqueue, -EINVAL);

    urb = urb_create(dev);
    RET_IF_FAIL(urb, -ENOMEM);

    direction = (typeReq & 0x8000) ? USB_HOST_DIR_IN : USB_HOST_DIR_OUT;

    urb->complete = usb_control_msg_callback;
    urb->pipe = (USB_HOST_PIPE_CONTROL << 30) | (dev->address << 8) | direction;
    urb->buffer = buffer;
    urb->maxpacket = 0x40;
    urb->length = wLength;

    urb->setup_packet[0] = typeReq >> 8;
    urb->setup_packet[1] = typeReq & 0xff;
    urb->setup_packet[2] = wValue & 0xff;
    urb->setup_packet[3] = wValue >> 8;
    urb->setup_packet[4] = wIndex & 0xff;
    urb->setup_packet[5] = wIndex >> 8;
    urb->setup_packet[6] = wLength & 0xff;
    urb->setup_packet[7] = wLength >> 8;

    retval = dev->hcd->driver->urb_enqueue(dev->hcd, urb);
    if (retval)
        goto out;

    semaphore_down(&urb->semaphore);
    retval = urb->status ? urb->status : urb->actual_length;

out:
    urb_destroy(urb);
    return retval;
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

        dev->port = i;
        dev->speed = USB_SPEED_FULL;
        if (status & (1 << 9))
            dev->speed = USB_SPEED_LOW;
        if (status & (1 << 10))
            dev->speed = USB_SPEED_HIGH;

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

        enumerate_device(dev);
    }

    return 0;
}

int enumerate_device(struct usb_device *dev)
{
    int retval;
    ssize_t size;
    int address;

    address = atomic_inc(&dev_id);
    kprintf("new usb device: %u (speed: %d)\n", dev->address, dev->speed);

    size = usb_control_msg(dev, USB_DEVICE_SET_ADDRESS, address, 0, 0, NULL);
    if (size < 0) {
        retval = size;
        goto out; // FIXME: unpower device port, and cleanup everything about
                  // device
    }

    dev->address = address;

    retval = device_probe(dev);
    if (retval)
        goto out; // FIXME: unpower device port, and cleanup everything about
                  // device

#if 0
    if (!retval) {
        print_descriptor(&dev->desc);
        print_descriptor(dev->interface);
    }
#endif

out:
    return retval;
}

struct usb_device *usb_device_create(struct usb_hcd *hcd,
                                     struct usb_device *hub)
{
    struct usb_device *dev = kzalloc(sizeof(*dev), 0);
    RET_IF_FAIL(dev, NULL);

    dev->speed = USB_SPEED_HIGH;
    dev->hcd = hcd;
    dev->hub = hub;

    return dev;
}

static int enumerate_bus(struct usb_hcd *hcd)
{
    int retval;
    struct usb_hub_descriptor desc;
    struct usb_device *dev;
    uint32_t status;

    RET_IF_FAIL(hcd, -EINVAL);
    RET_IF_FAIL(hcd->driver, -EINVAL);
    RET_IF_FAIL(hcd->driver->start, -EINVAL);
    RET_IF_FAIL(hcd->driver->hub_control, -EINVAL);

    retval = hcd->driver->start(hcd);
    if (retval)
        return retval;

    retval = hcd->driver->hub_control(hcd, USB_GET_HUB_DESCRIPTOR, 0, 0,
                                      sizeof(desc), (char*) &desc);
    if (retval)
        return retval;

    kprintf("%s: found new hub with %u ports.\n", hcd->device.name,
                                                  desc.bNbrPorts);

    for (int i = 1; i <= desc.bNbrPorts; i++) {
        hcd->driver->hub_control(hcd, USB_SET_PORT_FEATURE, PORT_POWER, i, 0,
                                 NULL);

        mdelay(desc.bPwrOn2PwrGood * 2);

        retval = hcd->driver->hub_control(hcd, USB_GET_PORT_STATUS, 0, i, 4,
                                          (char*) &status);
        if (retval)
            continue;

        if (!(status & (1 << PORT_CONNECTION))) {
            hcd->driver->hub_control(hcd, USB_CLEAR_PORT_FEATURE, PORT_POWER,
                                     i, 0, NULL);
            continue;
        }

        dev = usb_device_create(hcd, NULL);
        if (!dev)
            continue;

        dev->speed = USB_SPEED_FULL;
        if (status & (1 << 9))
            dev->speed = USB_SPEED_LOW;
        if (status & (1 << 10))
            dev->speed = USB_SPEED_HIGH;

        hcd->driver->hub_control(hcd, USB_SET_PORT_FEATURE, PORT_RESET, i, 0,
                                 NULL);

        enumerate_device(dev);
    }

    return 0;
}

void enumerate_everything(void *data)
{
    enumerate_bus(data);
    while (1);
}

int usb_hcd_register(struct usb_hcd *hcd)
{
    atomic_init(&dev_id, 1);
    task_run(enumerate_everything, hcd, 0);
    return 0;
}

static int device_driver_probe(struct usb_device *device)
{
    ssize_t size;
    struct usb_driver *driver = NULL;
    struct usb_device_id *id;

    spinlock_lock(&usbdev_driver_lock);

    list_foreach(&usbdev_driver, iter) {
        struct usb_driver *drv = containerof(iter, struct usb_driver, list);

        for (unsigned i = 0; drv->id_table[i].match; i++) {
            id = &drv->id_table[i];

            if (id->match & USB_DRIVER_MATCH_VENDOR) {
                if (le16_to_cpu(device->desc.idVendor) != id->vid)
                    continue;
            }

            if (id->match & USB_DRIVER_MATCH_PRODUCT) {
                if (le16_to_cpu(device->desc.idProduct) != id->pid)
                    continue;
            }

            if (id->match & USB_DRIVER_MATCH_DEVICE_CLASS) {
                if (device->desc.bDeviceClass != id->class)
                    continue;
            }

            if (id->match & USB_DRIVER_MATCH_DEVICE_SUBCLASS) {
                if (device->desc.bDeviceSubClass != id->subclass)
                    continue;
            }

            if (id->match & USB_DRIVER_MATCH_DEVICE_PROTOCOL) {
                if (device->desc.bDeviceProtocol != id->protocol)
                    continue;
            }

            if (id->match & USB_DRIVER_MATCH_INTERFACE_CLASS) {
                if (device->interface->bInterfaceClass != id->iclass)
                    continue;
            }

            if (id->match & USB_DRIVER_MATCH_INTERFACE_SUBCLASS) {
                if (device->interface->bInterfaceSubClass != id->isubclass)
                    continue;
            }

            if (id->match & USB_DRIVER_MATCH_INTERFACE_PROTOCOL) {
                if (device->interface->bInterfaceProtocol != id->iprotocol)
                    continue;
            }

            driver = drv;
            break;
        }
    }

    spinlock_unlock(&usbdev_driver_lock);

    if (!driver)
        return -ENODEV;

    size = usb_control_msg(device, USB_DEVICE_SET_CONFIGURATION,
                           device->config->bConfigurationValue, 0, 0, NULL);
    if (size < 0)
        return (int) size;

    return driver->probe(device, id);
}

static int device_probe_interface(struct usb_device *device, unsigned id)
{
    struct usb_interface_descriptor *desc;
    uintptr_t ptr = (uintptr_t) device->config;
    uintptr_t end = ptr + le16_to_cpu(device->config->wTotalLength);
    struct usb_descriptor_header *hdr = (struct usb_descriptor_header*) ptr;

    do {
        ptr += hdr->bLength;
        hdr = (struct usb_descriptor_header*) ptr;
        if (ptr + hdr->bLength > end)
            return -EINVAL;

        if (hdr->bDescriptorType != USB_DESCRIPTOR_INTERFACE)
            continue;

        desc = (struct usb_interface_descriptor*) hdr;

        if (desc->bInterfaceNumber != id)
            continue;

        device->interface = desc;

        return device_driver_probe(device);
    } while (ptr + hdr->bLength + sizeof(*hdr) < end);

    return -ENODEV;
}

static int device_probe_configuration(struct usb_device *device, unsigned id)
{
    struct usb_config_descriptor hdr;
    size_t config_size;
    ssize_t size;
    int retval;

    size = usb_control_msg(device, USB_DEVICE_GET_DESCRIPTOR,
                           USB_DESCRIPTOR_CONFIGURATION << 8 | id, 0,
                           sizeof(hdr), &hdr);
    if (size != sizeof(hdr) || size != hdr.bLength)
        return -EIO;

    config_size = le16_to_cpu(hdr.wTotalLength);

    device->config = kmalloc(config_size, MM_KERNEL);
    if (!device->config)
        return -ENOMEM;

    size = usb_control_msg(device, USB_DEVICE_GET_DESCRIPTOR,
                           USB_DESCRIPTOR_CONFIGURATION << 8 | id, 0,
                           config_size, device->config);
    if (size != config_size || size != config_size)
        return -EIO;

    for (unsigned i = 0; i < device->config->bNumInterfaces; i++) {
        retval = device_probe_interface(device, i);

        /* found a driver, return */
        if (!retval)
            break;
    }

    /* Free config only if no driver support this config */
    if (retval) {
        kfree(device->config);
        device->config = NULL;
    }

    return retval;
}

static int device_probe(struct usb_device *device)
{
    ssize_t size;
    int retval;

    size = usb_control_msg(device, USB_DEVICE_GET_DESCRIPTOR,
                           USB_DESCRIPTOR_DEVICE << 8, 0,
                           sizeof(device->desc), &device->desc);
    if (size != sizeof(device->desc) || size != device->desc.bLength)
        return -EIO;

    for (unsigned i = 0; i < device->desc.bNumConfigurations; i++) {
        retval = device_probe_configuration(device, i);
        if (!retval)
            break;
    }

    return retval;
}

int usb_register_driver(struct usb_driver *driver)
{
    spinlock_lock(&usbdev_driver_lock);
    list_add(&usbdev_driver, &driver->list);
    spinlock_unlock(&usbdev_driver_lock);

    return 0;
}
