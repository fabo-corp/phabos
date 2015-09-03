/**
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
 * @brief MIPI UniPro stack for ES2 Bridges
 */


#include <asm/hwio.h>
#include <asm/delay.h>

#include <phabos/driver.h>
#include <phabos/assert.h>
#include <phabos/scheduler.h>
#include <phabos/unipro.h>
#include <phabos/unipro/tsb.h>

#include "unipro-tsb-es2.h"
#include "unipro-tsb-es2-mphy-fixups.h"

#include <errno.h>

// See ENG-436
#define MBOX_RACE_HACK_DELAY    100000

#ifdef UNIPRO_DEBUG
#define DBG_UNIPRO(fmt, ...) kprintf(fmt, __VA_ARGS__)
#else
#define DBG_UNIPRO(fmt, ...) ((void)0)
#endif

#define TRANSFER_MODE          (2)
#define TRANSFER_MODE_2_CTRL_0 (0xAAAAAAAA) // Transfer mode 2 for CPorts 0-15
/*
 * CPorts 16-43 are present on the AP Bridge only.  CPorts 16 and 17 are
 * reserved for camera and display use, and we leave their transfer mode
 * at the power-on default of Mode 1.
 */
#define TRANSFER_MODE_2_CTRL_1 (0xAAAAAAA5) // Transfer mode 2 for CPorts 18-31
#define TRANSFER_MODE_2_CTRL_2 (0x00AAAAAA) // Transfer mode 2 for CPorts 32-43

/*
 * "Map" constants for M-PHY fixups.
 */
#define TSB_MPHY_MAP (0x7F)
    #define TSB_MPHY_MAP_TSB_REGISTER_1 (0x01)
    #define TSB_MPHY_MAP_NORMAL         (0x00)
    #define TSB_MPHY_MAP_TSB_REGISTER_2 (0x81)

#define CPORT_RX_BUF_ORDER        size_to_order(CPORT_BUF_SIZE / PAGE_SIZE)
#define CPORT_TX_BUF_BASE         (0x50000000U)
#define CPORT_TX_BUF_SIZE         (0x20000U)

#define CPB_TX_BUFFER_SPACE_MASK        ((uint32_t) 0x0000007F)
#define CPB_TX_BUFFER_SPACE_OFFSET_MASK CPB_TX_BUFFER_SPACE_MASK

struct tsb_unipro_priv {
    struct task *tx_worker;
    struct semaphore tx_fifo_lock;
};

struct tsb_unipro_buffer {
    struct list_head list;
    unipro_send_completion_t callback;
    void *priv;
    bool som;
    int byte_sent;
    int len;
    const void *data;
};

static ssize_t _tsb_unipro_send(struct unipro_cport *cport, const void *buf,
                                size_t len, bool som);

static uint32_t tsb_unipro_read(struct device *device, uint32_t offset)
{
    return read32(device->reg_base + offset);
}

static void tsb_unipro_write(struct device *device, uint32_t offset, uint32_t v)
{
    write32(device->reg_base + offset, v);
}

static inline uint8_t *cportid_to_tx_buffer(unsigned cport)
{
    return (uint8_t*) (CPORT_TX_BUF_BASE + (CPORT_TX_BUF_SIZE * cport));
}

static inline void unipro_set_eom_flag(struct unipro_cport *cport)
{
    write8(cportid_to_tx_buffer(cport->id) + CPORT_TX_BUF_SIZE - 1, 1);
}

static uint16_t unipro_get_tx_free_buffer_space(struct device *device,
                                                struct unipro_cport *cport)
{
    unsigned int cportid = cport->id;
    uint32_t tx_space;

    uint32_t tx_buf_space =
        tsb_unipro_read(device, CPB_TX_BUFFER_SPACE_REG(cportid));
    uint32_t tx_buf_space_offset =
        tsb_unipro_read(device, REG_TX_BUFFER_SPACE_OFFSET_REG(cportid));

    tx_space = 8 * (tx_buf_space & CPB_TX_BUFFER_SPACE_MASK);
    tx_space -= 8 * (tx_buf_space_offset & CPB_TX_BUFFER_SPACE_OFFSET_MASK);

    return tx_space;
}

static int es2_fixup_mphy(struct device *device)
{
    struct unipro_device *dev = to_unipro_device(device);
    struct tsb_unipro_pdata *pdata = device->pdata;
    uint32_t debug_0720 = pdata->debug_0720;
    uint32_t urc;
    const struct tsb_mphy_fixup *fu;

    /*
     * Apply the "register 2" map fixups.
     */
    unipro_attr_local_write(dev, TSB_MPHY_MAP, TSB_MPHY_MAP_TSB_REGISTER_2, 0,
                            &urc);
    if (urc) {
        dev_error(device, "%s: failed to switch to register 2 map: %u\n",
                  __func__, urc);
        return urc;
    }

    fu = tsb_register_2_map_mphy_fixups;
    do {
        unipro_attr_local_write(dev, fu->attrid, fu->value, fu->select_index,
                                &urc);
        if (urc) {
            dev_error(device, "%s: failed to apply register 1 map fixup: %u\n",
                      __func__, urc);
            return urc;
        }
    } while (!tsb_mphy_fixup_is_last(fu++));

    /*
     * Switch to "normal" map.
     */
    unipro_attr_local_write(dev, TSB_MPHY_MAP, TSB_MPHY_MAP_NORMAL, 0, &urc);
    if (urc) {
        dev_error(device, "%s: failed to switch to normal map: %u\n", __func__,
                  urc);
        return urc;
    }

    /*
     * Apply the "register 1" map fixups.
     */
    unipro_attr_local_write(dev, TSB_MPHY_MAP, TSB_MPHY_MAP_TSB_REGISTER_1, 0,
                            &urc);
    if (urc) {
        dev_error(device, "%s: failed to switch to register 1 map: %u\n",
                  __func__, urc);
        return urc;
    }

    fu = tsb_register_1_map_mphy_fixups;
    do {
        if (tsb_mphy_r1_fixup_is_magic(fu)) {
            /* The magic R1 fixups come from the mysterious and solemn
             * debug register 0x0720. */
            unipro_attr_local_write(dev, 0x8002, (debug_0720 >> 1) & 0x1f, 0,
                                    &urc);
        } else {
            unipro_attr_local_write(dev, fu->attrid, fu->value,
                                    fu->select_index, &urc);
        }
        if (urc) {
            dev_error(device, "%s: failed to apply register 1 map fixup: %u\n",
                      __func__, urc);
            return urc;
        }
    } while (!tsb_mphy_fixup_is_last(fu++));

    /*
     * Switch to "normal" map.
     */
    unipro_attr_local_write(dev, TSB_MPHY_MAP, TSB_MPHY_MAP_NORMAL, 0, &urc);
    if (urc) {
        dev_error(device, "%s: failed to switch to normal map: %u\n", __func__,
                  urc);
        return urc;
    }

    return 0;
}

static int tsb_unipro_attr_access(struct unipro_device *device, uint16_t attr,
                                  uint32_t *val, uint16_t selector, int peer,
                                  int write, uint32_t *result_code)
{
    uint32_t ctrl = (REG_ATTRACS_CTRL_PEERENA(peer) |
                     REG_ATTRACS_CTRL_SELECT(selector) |
                     REG_ATTRACS_CTRL_WRITE(write) |
                     attr);

    tsb_unipro_write(&device->device, A2D_ATTRACS_CTRL_00, ctrl);
    if (write)
        tsb_unipro_write(&device->device, A2D_ATTRACS_DATA_CTRL_00, *val);

    /* Start the access */
    tsb_unipro_write(&device->device, A2D_ATTRACS_MSTR_CTRL,
                 REG_ATTRACS_CNT(1) | REG_ATTRACS_UPD);

    while (!tsb_unipro_read(&device->device, A2D_ATTRACS_INT_BEF))
        ;

    /* Clear status bit */
    tsb_unipro_write(&device->device, A2D_ATTRACS_INT_BEF, 0x1);

    if (result_code) {
        *result_code = tsb_unipro_read(&device->device, A2D_ATTRACS_STS_00);
    }

    if (!write) {
        *val = tsb_unipro_read(&device->device, A2D_ATTRACS_DATA_STS_00);
    }

    return 0;
}

static void unipro_dequeue_tx_buffer(struct tsb_unipro_buffer *buffer,
                                     int status)
{
    RET_IF_FAIL(buffer,);

    irq_disable();
    list_del(&buffer->list);
    irq_enable();

    if (buffer->callback)
        buffer->callback(status, buffer->data, buffer->priv);

    kfree(buffer);
}

static ssize_t tsb_unipro_send_tx_buffer(struct unipro_cport *cport)
{
    struct device *device = &cport->device->device;
    struct tsb_unipro_buffer *buffer;
    ssize_t retval;

    if (!cport)
        return -EINVAL;

    irq_disable();

    if (list_is_empty(&cport->tx_fifo)) {
        irq_enable();
        return 0;
    }

    buffer = list_entry(cport->tx_fifo.next, struct tsb_unipro_buffer, list);

    irq_enable();

    retval = _tsb_unipro_send(cport, buffer->data + buffer->byte_sent,
                              buffer->len - buffer->byte_sent, buffer->som);
    if (retval < 0) {
        unipro_dequeue_tx_buffer(buffer, retval);
        dev_error(device, "unipro_send_sync failed. Dropping message...\n");
        return retval;
    }

    buffer->som = false;
    buffer->byte_sent += retval;

    if (buffer->byte_sent >= buffer->len) {
        unipro_set_eom_flag(cport);
        unipro_dequeue_tx_buffer(buffer, 0);
        return 0;
    }

    return -EBUSY;
}

static void tsb_unipro_tx_worker(void *data)
{
    struct device *device = data;
    struct unipro_device *unipro_dev = to_unipro_device(device);
    struct tsb_unipro_priv *priv = device->priv;
    int retval;
    bool is_busy;

    while (1) {
        /* Block until a buffer is pending on any CPort */
        semaphore_down(&priv->tx_fifo_lock);

        do {
            is_busy = false;

            for (unsigned i = 0; i < unipro_dev->cport_count; i++) {
                /* Browse all CPorts sending any pending buffers */
                retval = tsb_unipro_send_tx_buffer(&unipro_dev->cports[i]);
                if (retval == -EBUSY) {
                    /*
                     * Buffer only partially sent, have to try again for
                     * remaining part.
                     */
                    is_busy = true;
                }
            }
        } while (is_busy); /* exit when CPort(s) current pending buffer sent */
    }
}


static int configure_transfer_mode(struct device *device, int mode)
{
    struct tsb_unipro_pdata *pdata = device->pdata;

    switch (mode) {
    case 2:
        tsb_unipro_write(device, AHM_MODE_CTRL_0, TRANSFER_MODE_2_CTRL_0);
        if (pdata && pdata->product_id == TSB_UNIPRO_APBRIDGE) {
            tsb_unipro_write(device, AHM_MODE_CTRL_1, TRANSFER_MODE_2_CTRL_1);
            tsb_unipro_write(device, AHM_MODE_CTRL_2, TRANSFER_MODE_2_CTRL_2);
        }

        return 0;

    default:
        dev_error(device, "Unsupported transfer mode: %d\n", mode);
        return -EINVAL;
    }
}

static ssize_t _tsb_unipro_send(struct unipro_cport *cport, const void *buf,
                                size_t len, bool som)
{
    struct device *device = &cport->device->device;
    size_t count;
    uint8_t *tx_buf;

    if (!cport->is_connected) {
        dev_error(device, "sending to unconnected CPort %u\n", cport->id);
        return -EPIPE;
    }

    RET_IF_FAIL(TRANSFER_MODE == 2, -EINVAL);

    /*
     * If this is not the start of a new message,
     * message data must be written to first address of CPort Tx Buffer + 1.
     */
    tx_buf = cportid_to_tx_buffer(cport->id) + (som ? 0 : sizeof(uint32_t));

    count = unipro_get_tx_free_buffer_space(device, cport);
    if (!count) {
        /* No free space in TX FIFO, cannot send anything. */
        DBG_UNIPRO("No free space in CP%d Tx Buffer\n", cportid);
        return 0;
    }

    if (count > len)
        count = len;

    /* Copy message data in CPort Tx FIFO */
    DBG_UNIPRO("Sending %zu bytes to CP%u\n", count, cport->id);
    memcpy(tx_buf, buf, count);

    return count;
}

static ssize_t tsb_unipro_send(struct unipro_cport *cport, const void *buf,
                               size_t len)
{
    ssize_t retval;
    ssize_t sent = 0;
    bool som = true;

    while (sent < len) {
        retval = _tsb_unipro_send(cport, buf + sent, len - sent, som);
        if (retval < 0)
            return retval;

        sent += retval;
        if (retval != 0)
            som = false;
    }

    unipro_set_eom_flag(cport);

    return sent;
}

static ssize_t tsb_unipro_send_async(struct unipro_cport *cport,
                                     const void *buf, size_t len,
                                     unipro_send_completion_t callback,
                                     void *user_priv)
{
    struct device *device = &cport->device->device;
    struct tsb_unipro_priv *priv = device->priv;
    struct tsb_unipro_buffer *buffer;

    if (!cport->is_connected) {
        dev_error(device, "sending to unconnected CPort %u\n", cport->id);
        return -EPIPE;
    }

    RET_IF_FAIL(TRANSFER_MODE == 2, -EINVAL);

    buffer = kzalloc(sizeof(*buffer), MM_KERNEL);
    if (!buffer)
        return -ENOMEM;

    list_init(&buffer->list);
    buffer->som = true;
    buffer->len = len;
    buffer->callback = callback;
    buffer->priv = user_priv;
    buffer->data = buf;

    irq_disable();
    list_add(&cport->tx_fifo, &buffer->list);
    irq_enable();

    semaphore_up(&priv->tx_fifo_lock);
    return 0;
}

static int tsb_unipro_unpause_rx(struct unipro_cport *cport)
{
    if (!cport->is_connected)
        return -EINVAL;

    /* Restart the flow of received data */
    tsb_unipro_write(&cport->device->device,
                     REG_RX_PAUSE_SIZE_00 + (cport->id * sizeof(uint32_t)),
                     (1 << 31) | CPORT_BUF_SIZE);

    return 0;
}

static inline void clear_rx_interrupt(struct unipro_cport *cport)
{
    struct device *device = &cport->device->device;
    tsb_unipro_write(device, AHM_RX_EOM_INT_BEF_REG(cport->id),
                     AHM_RX_EOM_INT_BEF(cport->id));
}

static inline void enable_rx_interrupt(struct unipro_cport *cport)
{
    struct device *device = &cport->device->device;
    uint32_t reg = AHM_RX_EOM_INT_EN_REG(cport->id);
    uint32_t bit = AHM_RX_EOM_INT_EN(cport->id);

    tsb_unipro_write(device, reg, tsb_unipro_read(device, reg) | bit);
}

static void irq_rx_eom(int irq, void *priv)
{
    struct unipro_cport *cport = priv;
    struct device *device = &cport->device->device;
    size_t xfer_size;
    void *data;

    clear_rx_interrupt(cport);

    if (!cport->driver) {
        dev_error(device,
                  "dropping message on cport %u where no driver is registered\n",
                  cport->id);
        return;
    }

    uint32_t buffer_addr = AHM_ADDRESS_00 + (cport->id * sizeof(uint32_t));
    data = (void*) tsb_unipro_read(device, buffer_addr);

    xfer_size = tsb_unipro_read(device, CPB_RX_TRANSFERRED_DATA_SIZE_00 +
                                        cport->id * sizeof(uint32_t));
    DBG_UNIPRO("cport: %u driver: %s size=%zu payload=0x%p\n", cport->id,
               cport->driver->name, xfer_size, data);

    if (cport->driver->rx_handler)
        cport->driver->rx_handler(cport->driver, cport->id, data, xfer_size);
}

static void clear_int(struct unipro_cport *cport)
{
    struct device *device = &cport->device->device;
    struct tsb_unipro_pdata *pdata = device->pdata;
    unsigned int i;
    uint32_t int_en;
    unsigned offset = (cport->id / 16) * sizeof(uint32_t);

    i = cport->id * 2;

    tsb_unipro_write(device, AHM_RX_EOM_INT_BEF_0 + offset, 0x3 << i);
    int_en = tsb_unipro_read(device, AHM_RX_EOM_INT_EN_0 + offset);
    int_en &= ~(0x3 << i);
    tsb_unipro_write(device, AHM_RX_EOM_INT_EN_0 + offset, int_en);

    irq_clear(pdata->cport_irq_base + cport->id);
}

static void enable_int(struct unipro_cport *cport)
{
    struct tsb_unipro_pdata *pdata = cport->device->device.pdata;
    unsigned int irqn = pdata->cport_irq_base + cport->id;

    enable_rx_interrupt(cport);

    irq_attach(irqn, irq_rx_eom, cport);
    irq_enable_line(irqn);
}

static int tsb_unipro_switch_buffer(struct unipro_cport *cport, void *buffer)
{
    struct device *device = &cport->device->device;
    tsb_unipro_write(device, AHM_ADDRESS_00 + (cport->id * sizeof(uint32_t)),
                     (uint32_t) buffer);
    return 0;
}

static int tsb_unipro_init_cport(struct unipro_cport *cport)
{
    struct device *device = &cport->device->device;
    void *buffer;

    buffer = cport->driver->get_buffer();
    if (!buffer)
        return -ENOMEM;

    /*
     * FIXME: We presently specify a fixed receive buffer address
     *        for each CPort.  That approach won't work for a
     *        pipelined zero-copy system.
     */
    tsb_unipro_write(device, AHM_ADDRESS_00 + (cport->id * sizeof(uint32_t)),
                     (uint32_t) buffer);

#ifdef UNIPRO_DEBUG
    unipro_info();
#endif

    return 0;
}

static int tsb_unipro_local_attr_read(struct unipro_device *device,
                                      uint16_t attr, uint32_t *val,
                                      uint16_t selector, uint32_t *result_code)
{
    return tsb_unipro_attr_access(device, attr, val, selector, 0, 0,
                                  result_code);
}

static int tsb_unipro_local_attr_write(struct unipro_device *device,
                                       uint16_t attr, uint32_t val,
                                       uint16_t selector, uint32_t *result_code)
{
    return tsb_unipro_attr_access(device, attr, &val, selector, 0, 1,
                                  result_code);
}

static int tsb_unipro_peer_attr_read(struct unipro_device *device,
                                     uint16_t attr, uint32_t *val,
                                     uint16_t selector, uint32_t *result_code)
{
    return tsb_unipro_attr_access(device, attr, val, selector, 1, 0,
                                  result_code);
}

static int tsb_unipro_peer_attr_write(struct unipro_device *device,
                                      uint16_t attr, uint32_t val,
                                      uint16_t selector, uint32_t *result_code)
{
    return tsb_unipro_attr_access(device, attr, &val, selector, 1, 1,
                                  result_code);
}

static struct unipro_ops tsb_unipro_ops = {
    .local_attr_read = tsb_unipro_local_attr_read,
    .local_attr_write = tsb_unipro_local_attr_write,
    .peer_attr_read = tsb_unipro_peer_attr_read,
    .peer_attr_write = tsb_unipro_peer_attr_write,

    .cport = {
        .send = tsb_unipro_send,
        .send_async = tsb_unipro_send_async,
        .unpause_rx = tsb_unipro_unpause_rx,
        .switch_buffer = tsb_unipro_switch_buffer,
        .init = tsb_unipro_init_cport,
    },
};

static uint32_t cport_get_status(struct device *device, unsigned int cportid)
{
    uint32_t val;
    uint32_t reg;

    reg = CPORT_STATUS_0 + ((cportid / 16) << 2);
    val = tsb_unipro_read(device, reg);
    val >>= ((cportid % 16) << 1);
    return (val & (0x3));
}

static int configure_connected_cport(struct unipro_device *dev,
                                     unsigned int cportid)
{
    int ret = 0;
    unsigned int rc;

    rc = cport_get_status(&dev->device, cportid);

    switch (rc) {
    case CPORT_STATUS_CONNECTED:
        dev->cports[cportid].is_connected = 1;

        /* Start the flow of received data */
        tsb_unipro_write(&dev->device,
                         REG_RX_PAUSE_SIZE_00 + (cportid * sizeof(uint32_t)),
                         (1 << 31) | CPORT_BUF_SIZE);

        /* Clear any pending EOM interrupts, then enable them. */
        irq_disable();
        clear_int(&dev->cports[cportid]);
        enable_int(&dev->cports[cportid]);
        irq_enable();
        break;

    case CPORT_STATUS_UNCONNECTED:
        ret = -ENOTCONN;
        break;

    default:
        dev_error(&dev->device, "Unexpected status: CP%u: status: 0x%u\n",
                  cportid, rc);
        ret = -EIO;
    }

    return ret;
}

static void irq_unipro(int irq, void *data)
{
    struct device *device = data;
    struct unipro_device *dev = to_unipro_device(device);
    uint32_t cportid;
    int rc;
    uint32_t e2efc;
    uint32_t val;

    irq_clear(device->irq);

    /*
     * Clear the initial interrupt
     */
    rc = unipro_attr_local_read(dev, TSB_INTERRUPTSTATUS, &val, 0, NULL);
    if (rc)
        return;

    /*
     * Figure out which CPort to turn on FCT. The desired CPort is always
     * the mailbox value - 1.
     */
    rc = unipro_attr_local_read(dev, TSB_MAILBOX, &cportid, 0, NULL);
    if (rc || !cportid)
        return;
    cportid--;

    uint32_t reg = CPB_RX_E2EFC_EN_0 + 4 * (cportid / 32);
    unsigned offset = cportid % 32;

    e2efc = tsb_unipro_read(device, reg);
    e2efc |= 1 << offset;
    tsb_unipro_write(device, reg, e2efc);

    configure_connected_cport(dev, cportid);

    /*
     * Clear the mailbox. This triggers another a local interrupt which we have
     * to clear.
     */
    rc = unipro_attr_local_write(dev, TSB_MAILBOX, 0, 0, NULL);
    if (rc)
        return;

    rc = unipro_attr_local_read(dev, TSB_INTERRUPTSTATUS, &val, 0, NULL);
    if (rc)
        return;

    rc = unipro_attr_local_read(dev, TSB_MAILBOX, &cportid, 0, NULL);
    if (rc)
        return;
}

int tsb_unipro_mbox_set(struct unipro_device *device, uint32_t val, int peer)
{
    int rc;

    rc = tsb_unipro_attr_access(device, TSB_MAILBOX, &val, 0, peer, 1, NULL);
    if (rc) {
        kprintf("TSB_MAILBOX write failed: %d\n", rc);
        return rc;
    }

    /*
     * Silicon bug?: There seems to be a problem in the switch regarding
     * lost mailbox sets. It seems to happen when a bridge writes to the
     * mailbox and immediately reads the value back.
     *
     * Workaround for now by inserting a small delay.
     *
     * @jira{ENG-436}
     */
    udelay(MBOX_RACE_HACK_DELAY);

    do {
        rc = tsb_unipro_attr_access(device, TSB_MAILBOX, &val, 0, peer,
                                    0, NULL);
        if (rc) {
            kprintf("%s(): TSB_MAILBOX poll failed: %d\n", __func__, rc);
        }
    } while (!rc && val != TSB_MAIL_RESET);

    return rc;
}

static int tsb_unipro_probe(struct device *device)
{
    struct unipro_device *dev = to_unipro_device(device);
    struct tsb_unipro_pdata *pdata = device->pdata;
    struct tsb_unipro_priv *priv;
    int retval;

    RET_IF_FAIL(TRANSFER_MODE == 2, -EINVAL);

    priv = kzalloc(sizeof(*priv), MM_KERNEL);
    if (!priv)
        return -ENOMEM;

    device->priv = priv;

    retval = unipro_register_device(dev, &tsb_unipro_ops);
    if (retval)
        goto error_register_device;

    semaphore_init(&priv->tx_fifo_lock, 0);
    priv->tx_worker = task_run("tsb-unipro-tx-worker", tsb_unipro_tx_worker,
                               device, 0);
    if (!priv->tx_worker) {
        retval = -ENOMEM;
        goto error_task_run;
    }

    if (es2_fixup_mphy(device))
        dev_error(device, "Failed to apply M-PHY fixups (results in link instability at HS-G1).\n");

    /*
     * Set transfer mode 2 on all cports
     * Receiver choses address for received message
     * Header is delivered transparently to receiver (and used to carry the first eight
     * L4 payload bytes)
     */
    retval = configure_transfer_mode(device, TRANSFER_MODE);
    if (retval)
        goto error_configure_xfer_mode;

    /*
     * Disable FCT transmission. See ENG-376.
     */
    tsb_unipro_write(device, CPB_RX_E2EFC_EN_0, 0x0);
        if (pdata && pdata->product_id == TSB_UNIPRO_APBRIDGE)
        tsb_unipro_write(device, CPB_RX_E2EFC_EN_1, 0x0);

    /*
     * Enable the mailbox interrupt
     */
    tsb_unipro_write(device, UNIPRO_INT_EN, 0x1);
    retval = unipro_attr_local_write(dev, TSB_INTERRUPTENABLE, 1 << 15,
                                     0, NULL);
    if (retval)
        dev_error(device, "Failed to enable mailbox interrupt\n");

    irq_attach(device->irq, irq_unipro, device);
    irq_enable_line(device->irq);

    return 0;

error_configure_xfer_mode:
error_task_run:
error_register_device:
    kfree(priv);
    device->priv = NULL;
    return retval;
}

__driver__ struct driver tsb_es2_unipro_driver = {
    .name = "tsb-unipro-es2",
    .probe = tsb_unipro_probe,
};
