/*
 * Copyright (c) 2014-2015 Google Inc.
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
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include <asm/hwio.h>
#include <asm/irq.h>
#include <asm/tsb-irq.h>
#include <asm/machine.h>

#include <phabos/sleep.h>
#include <phabos/mutex.h>
#include <phabos/semaphore.h>
#include <phabos/i2c.h>
#include <phabos/watchdog.h>
#include <phabos/utils.h>

#include "dw.h"

/* disable the verbose debug output */
#undef lldbg
#define lldbg(x...)

static struct i2c_dev   *g_dev;        /* Generic Nuttx I2C device */
static struct i2c_msg   *g_msgs;      /* Generic messages array */
static struct mutex     g_mutex;      /* Only one thread can access at a time */
static struct semaphore g_wait;       /* Wait for state machine completion */
static struct watchdog  g_timeout;    /* Watchdog to timeout when bus hung */

static unsigned int     g_tx_index;
static unsigned int     g_tx_length;
static uint8_t          *g_tx_buffer;

static unsigned int     g_rx_index;
static unsigned int     g_rx_length;
static uint8_t          *g_rx_buffer;

static unsigned int     g_msgs_count;
static int              g_cmd_err;
static int              g_msg_err;
static unsigned int     g_status;
static uint32_t         g_abort_source;
static unsigned int     g_rx_outstanding;


/* I2C controller configuration */
#define DW_I2C_CONFIG (DW_I2C_CON_RESTART_EN | \
                       DW_I2C_CON_MASTER | \
                       DW_I2C_CON_SLAVE_DISABLE | \
                       DW_I2C_CON_SPEED_FAST)

#define DW_I2C_TX_FIFO_DEPTH   8
#define DW_I2C_RX_FIFO_DEPTH   8

/* IRQs handle by the driver */
#define DW_I2C_INTR_DEFAULT_MASK (DW_I2C_INTR_RX_FULL | \
                                  DW_I2C_INTR_TX_EMPTY | \
                                  DW_I2C_INTR_TX_ABRT | \
                                  DW_I2C_INTR_STOP_DET)

#define TIMEOUT                     20              /* 20 ms */
#define DW_I2C_TIMEOUT              1               /* 1sec */

static uint32_t dw_read(int offset)
{
    return read32(g_dev->device.reg_base + offset);
}

static void dw_write(int offset, uint32_t b)
{
    write32(g_dev->device.reg_base + offset, b);
}

static void tsb_i2c_clear_int(void)
{
    dw_read(DW_I2C_CLR_INTR);
}

static void tsb_i2c_disable_int(void)
{
    dw_write(DW_I2C_INTR_MASK, 0);
}

static void dw_set_enable(int enable)
{
    int i;

    for (i = 0; i < 50; i++) {
        dw_write(DW_I2C_ENABLE, enable);

        if ((dw_read(DW_I2C_ENABLE_STATUS) & 0x1) == enable)
            return;

        usleep(25);
    }

    lldbg("timeout!");
}

/* Enable the controller */
static void tsb_i2c_enable(void)
{
    dw_set_enable(1);
}

/* Disable the controller */
static void tsb_i2c_disable(void)
{
    dw_set_enable(0);

    tsb_i2c_disable_int();
    tsb_i2c_clear_int();
}

/**
 * Initialize the TSB I2 controller
 */
static void tsb_i2c_init(void)
{
    /* Disable the adapter */
    tsb_i2c_disable();

    /* Set timings for Standard and Fast Speed mode */
    dw_write(DW_I2C_SS_SCL_HCNT, 28);
    dw_write(DW_I2C_SS_SCL_LCNT, 52);
    dw_write(DW_I2C_FS_SCL_HCNT, 47);
    dw_write(DW_I2C_FS_SCL_LCNT, 65);

    /* Configure Tx/Rx FIFO threshold levels */
    dw_write(DW_I2C_TX_TL, DW_I2C_TX_FIFO_DEPTH - 1);
    dw_write(DW_I2C_RX_TL, 0);

    /* configure the i2c master */
    dw_write(DW_I2C_CON, DW_I2C_CONFIG);
}

static int tsb_i2c_wait_bus_ready(void)
{
    int timeout = TIMEOUT;

    while (dw_read(DW_I2C_STATUS) & DW_I2C_STATUS_ACTIVITY) {
        if (timeout <= 0) {
            lldbg("timeout\n");
            return -ETIMEDOUT;
        }
        timeout--;
        usleep(1000);
    }

    return 0;
}

static void tsb_i2c_start_transfer(void)
{
    lldbg("\n");

    /* Disable the adapter */
    tsb_i2c_disable();

    /* write target address */
    dw_write(DW_I2C_TAR, g_msgs[g_tx_index].addr);

    /* Disable the interrupts */
    tsb_i2c_disable_int();

    /* Enable the adapter */
    tsb_i2c_enable();

    /* Clear interrupts */
    tsb_i2c_clear_int();

    /* Enable interrupts */
    dw_write(DW_I2C_INTR_MASK, DW_I2C_INTR_DEFAULT_MASK);
}

/**
 * Internal function that handles the read or write transfer
 * It is called from the IRQ handler.
 */
static void tsb_i2c_transfer_msg(void)
{
    uint32_t intr_mask;
    uint32_t addr = g_msgs[g_tx_index].addr;
    uint8_t *buffer = g_tx_buffer;
    uint32_t length = g_tx_length;

    bool need_restart = false;

    int tx_avail;
    int rx_avail;

    lldbg("tx_index %d\n", g_tx_index);

    /* loop over the i2c message array */
    for (; g_tx_index < g_msgs_count; g_tx_index++) {

        if (g_msgs[g_tx_index].addr != addr) {
            lldbg("invalid target address\n");
            g_msg_err = -EINVAL;
            break;
        }

        if (g_msgs[g_tx_index].length == 0) {
            lldbg("invalid message length\n");
            g_msg_err = -EINVAL;
            break;
        }

        if (!(g_status & DW_I2C_STATUS_WRITE_IN_PROGRESS)) {
            /* init a new msg transfer */
            buffer = g_msgs[g_tx_index].buffer;
            length = g_msgs[g_tx_index].length;

            /* force a restart between messages */
            if (g_tx_index > 0)
                need_restart = true;
        }

        /* Get the amount of free space in the internal buffer */
        tx_avail = DW_I2C_TX_FIFO_DEPTH - dw_read(DW_I2C_TXFLR);
        rx_avail = DW_I2C_RX_FIFO_DEPTH - dw_read(DW_I2C_RXFLR);

        /* loop until one of the fifo is full or buffer is consumed */
        while (length > 0 && tx_avail > 0 && rx_avail > 0) {
            uint32_t cmd = 0;

            if (g_tx_index == g_msgs_count - 1 && length == 1) {
                /* Last msg, issue a STOP */
                cmd |= (1 << 9);
                lldbg("STOP\n");
            }

            if (need_restart) {
                cmd |= (1 << 10); /* RESTART */
                need_restart = false;
                lldbg("RESTART\n");
            }

            if (g_msgs[g_tx_index].flags & I2C_READ) {
                if (rx_avail - g_rx_outstanding <= 0)
                    break;

                dw_write(DW_I2C_DATA_CMD, cmd | 1 << 8); /* READ */
                lldbg("READ\n");

                rx_avail--;
                g_rx_outstanding++;
            } else {
                dw_write(DW_I2C_DATA_CMD, cmd | *buffer++);
                lldbg("WRITE\n");
            }

            tx_avail--;
            length--;
        }

        g_tx_buffer = buffer;
        g_tx_length = length;

        if (length > 0) {
            g_status |= DW_I2C_STATUS_WRITE_IN_PROGRESS;
            break;
        } else {
            g_status &= ~DW_I2C_STATUS_WRITE_IN_PROGRESS;
        }
    }

    intr_mask = DW_I2C_INTR_DEFAULT_MASK;

    /* No more data to write. Stop the TX IRQ */
    if (g_tx_index == g_msgs_count)
        intr_mask &= ~DW_I2C_INTR_TX_EMPTY;

    /* In case of error, mask all the IRQs */
    if (g_msg_err)
        intr_mask = 0;

    dw_write(DW_I2C_INTR_MASK, intr_mask);
}

static void tsb_dw_read(void)
{
    int rx_valid;

    lldbg("rx_index %d\n", g_rx_index);

    for (; g_rx_index < g_msgs_count; g_rx_index++) {
        uint32_t len;
        uint8_t *buffer;

        if (!(g_msgs[g_rx_index].flags & I2C_READ))
            continue;

        if (!(g_status & DW_I2C_STATUS_READ_IN_PROGRESS)) {
            len = g_msgs[g_rx_index].length;
            buffer = g_msgs[g_rx_index].buffer;
        } else {
            len = g_rx_length;
            buffer = g_rx_buffer;
        }

        rx_valid = dw_read(DW_I2C_RXFLR);

        for (; len > 0 && rx_valid > 0; len--, rx_valid--) {
            *buffer++ = dw_read(DW_I2C_DATA_CMD);
            g_rx_outstanding--;
        }

        if (len > 0) {
            /* start the read process */
            g_status |= DW_I2C_STATUS_READ_IN_PROGRESS;
            g_rx_length = len;
            g_rx_buffer = buffer;

            return;
        } else {
            g_status &= ~DW_I2C_STATUS_READ_IN_PROGRESS;
        }
    }
}

static int tsb_i2c_handle_tx_abort(void)
{
    unsigned long abort_source = g_abort_source;

    lldbg("%s: 0x%x\n", __func__, abort_source);

    if (abort_source & DW_I2C_TX_ABRT_NOACK) {
        lldbg("%s: DW_I2C_TX_ABRT_NOACK 0x%x\n", __func__, abort_source);
        return -EIO;
    }

    if (abort_source & DW_I2C_TX_ARB_LOST)
        return -EAGAIN;
    else if (abort_source & DW_I2C_TX_ABRT_GCALL_READ)
        return -EINVAL; /* wrong g_msgs[] data */
    else
        return -EIO;
}

/* Perform a sequence of I2C transfers */
static int dw_transfer(struct i2c_dev *dev, struct i2c_msg *msg, size_t count)
{
    int ret;

    lldbg("msgs: %d\n", count);

    mutex_lock(&g_mutex);

    g_msgs = msg;
    g_msgs_count = count;
    g_tx_index = 0;
    g_rx_index = 0;
    g_rx_outstanding = 0;

    g_cmd_err = 0;
    g_msg_err = 0;
    g_status = DW_I2C_STATUS_IDLE;
    g_abort_source = 0;

    ret = tsb_i2c_wait_bus_ready();
    if (ret < 0)
        goto done;

    /*
     * start a watchdog to timeout the transfer if
     * the bus is locked up...
     */
    watchdog_start_sec(&g_timeout, DW_I2C_TIMEOUT);

    /* start the transfers */
    tsb_i2c_start_transfer();

    semaphore_lock(&g_wait);

    watchdog_cancel(&g_timeout);

    if (g_status == DW_I2C_STATUS_TIMEOUT) {
        lldbg("controller timed out\n");

        /* Re-init the adapter */
        tsb_i2c_init();
        ret = -ETIMEDOUT;
        goto done;
    }

    tsb_i2c_disable();

    if (g_msg_err) {
        ret = g_msg_err;
        lldbg("error msg_err %x\n", g_msg_err);
        goto done;
    }

    if (!g_cmd_err) {
        ret = 0;
        lldbg("no error %d\n", count);
        goto done;
    }

    /* Handle abort errors */
    if (g_cmd_err == DW_I2C_ERR_TX_ABRT) {
        ret = tsb_i2c_handle_tx_abort();
        goto done;
    }

    /* default error code */
    ret = -EIO;
    lldbg("unknown error %x\n", ret);

done:
    mutex_unlock(&g_mutex);

    return ret;
}

static uint32_t tsb_dw_read_clear_intrbits(void)
{
    uint32_t stat = dw_read(DW_I2C_INTR_STAT);

    if (stat & DW_I2C_INTR_RX_UNDER)
        dw_read(DW_I2C_CLR_RX_UNDER);
    if (stat & DW_I2C_INTR_RX_OVER)
        dw_read(DW_I2C_CLR_RX_OVER);
    if (stat & DW_I2C_INTR_TX_OVER)
        dw_read(DW_I2C_CLR_TX_OVER);
    if (stat & DW_I2C_INTR_RD_REQ)
        dw_read(DW_I2C_CLR_RD_REQ);
    if (stat & DW_I2C_INTR_TX_ABRT) {
        /* IC_TX_ABRT_SOURCE reg is cleared upon read, store it */
        g_abort_source = dw_read(DW_I2C_TX_ABRT_SOURCE);
        dw_read(DW_I2C_CLR_TX_ABRT);
    }
    if (stat & DW_I2C_INTR_RX_DONE)
        dw_read(DW_I2C_CLR_RX_DONE);
    if (stat & DW_I2C_INTR_ACTIVITY)
        dw_read(DW_I2C_CLR_ACTIVITY);
    if (stat & DW_I2C_INTR_STOP_DET)
        dw_read(DW_I2C_CLR_STOP_DET);
    if (stat & DW_I2C_INTR_START_DET)
        dw_read(DW_I2C_CLR_START_DET);
    if (stat & DW_I2C_INTR_GEN_CALL)
        dw_read(DW_I2C_CLR_GEN_CALL);

    return stat;
}

/* I2C interrupt service routine */
static void dw_interrupt(int irq, void *data)
{
    uint32_t stat, enabled;

    enabled = dw_read(DW_I2C_ENABLE);
    stat = dw_read(DW_I2C_RAW_INTR_STAT);

    lldbg("enabled=0x%x stat=0x%x\n", enabled, stat);

    if (!enabled || !(stat & ~DW_I2C_INTR_ACTIVITY))
        return;

    stat = tsb_dw_read_clear_intrbits();

    if (stat & DW_I2C_INTR_TX_ABRT) {
        lldbg("abort\n");
        g_cmd_err |= DW_I2C_ERR_TX_ABRT;
        g_status = DW_I2C_STATUS_IDLE;

        tsb_i2c_disable_int();
        goto tx_aborted;
    }

    if (stat & DW_I2C_INTR_RX_FULL)
        tsb_dw_read();

    if (stat & DW_I2C_INTR_TX_EMPTY)
        tsb_i2c_transfer_msg();

tx_aborted:
    if (stat & DW_I2C_INTR_TX_ABRT)
        lldbg("aborted %x %x\n", stat, g_abort_source);

    if ((stat & (DW_I2C_INTR_TX_ABRT | DW_I2C_INTR_STOP_DET)) || g_msg_err) {
        lldbg("release sem\n");
        semaphore_unlock(&g_wait);
    }
}

/**
 * Watchdog handler for timeout of I2C operation
 */
static void dw_timeout(struct watchdog *wd)
{
    lldbg("\n");

    irq_disable();

    if (g_status != DW_I2C_STATUS_IDLE)
    {
        lldbg("finished\n");
        /* Mark the transfer as finished */
        g_status = DW_I2C_STATUS_TIMEOUT;
        semaphore_unlock(&g_wait);
    }

    irq_enable();
}

static struct i2c_ops dev_i2c_ops = {
    .transfer   = dw_transfer,
};

static int dw_probe(struct device *device)
{
    mutex_init(&g_mutex);
    semaphore_init(&g_wait, 0);

    /* Allocate a watchdog timer */
    watchdog_init(&g_timeout);
    g_timeout.timeout = dw_timeout;

    /* Install our operations */
    g_dev = containerof(device, struct i2c_dev, device);
    g_dev->ops = &dev_i2c_ops;

    irq_attach(device->irq, dw_interrupt, device);
    irq_enable_line(device->irq);

    tsb_i2c_init();

    return 0;
}

static int dw_remove(struct device *device)
{
    watchdog_delete(&g_timeout);
    return 0;
}

__driver__ struct driver dw_i2c_driver = {
    .name = "dw-i2c",

    .probe = dw_probe,
    .remove = dw_remove,
};
