#include <phabos/unipro.h>
#include <phabos/driver.h>

#include <apps/svc/svc.h> // FIXME
#include <apps/svc/tsb_switch.h> // FIXME

#include <errno.h>

static ssize_t t6wt_unipro_send(struct unipro_cport *cport, const void *buf,
                                size_t len)
{
    struct tsb_switch *sw = svc->sw;
    if (!sw)
        return -ENODEV;

    switch_data_send(sw, (void*) buf, len);
    return len;
}

static int t6wt_unipro_init_cport(struct unipro_cport *cport)
{
    /* Only cports 4 and 5 are supported on ES2 silicon */
    switch (cport->id) {
    case 4:
    case 5:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

/*
 * Packet entry point into SVC unipro stack. This is usually called
 * by the switch driver when a packet is received.
 */
void unipro_if_rx(unsigned int cportid, void *data, size_t len)
{
    extern struct unipro_device t6wt_device;
    struct unipro_cport *cport = &t6wt_device.cports[cportid];

    irq_disable();
    cport->driver->rx_handler(cport->driver, cportid, data, len);
    irq_enable();
}

static int t6wt_unipro_unpause_rx(struct unipro_cport *cport)
{
    return 0;
}

static struct unipro_ops t6wt_unipro_ops = {
    .cport = {
        .send = t6wt_unipro_send,
        .unpause_rx = t6wt_unipro_unpause_rx,
        .init = t6wt_unipro_init_cport,
    },
};

static int t6wt_probe(struct device *device)
{
    struct unipro_device *dev = to_unipro_device(device);
    return unipro_register_device(dev, &t6wt_unipro_ops);
}

__driver__ struct driver t6wt_unipro_driver = {
    .name = "t6wt0xbg-unipro",
    .probe = t6wt_probe,
};
