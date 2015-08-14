/**
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
 *
 * @author Perry Hung
 * @brief MIPI UniPro stack for APBridge ES1
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "unipro-es1.h" // private

#include <asm/hwio.h>
#include <asm/irq.h>
#include <asm/tsb-irq.h>

#include <phabos/unipro.h>
#include <phabos/unipro/tsb.h>
#include <phabos/unipro/unipro.h>
#include <phabos/utils.h>
#include <phabos/kprintf.h>
#include <phabos/assert.h>

#define lldbg kprintf // FIXME

static inline void putreg32(uint32_t value, uint32_t reg)
{
    write32(reg, value);
}

#ifdef UNIPRO_DEBUG
#define DBG_UNIPRO(fmt, ...) lldbg(fmt,__VA_ARGS__ )
#else
#define DBG_UNIPRO(fmt, ...)
#endif

#define TRANSFER_MODE               (1)
#define TRANSFER_MODE1_DEFAULT_MASK (0x55555555) // Set transfer mode

/*
 * CP16 and CP17 are used by CDSI, can not have flow control enabled
 */
#define E2EFC_DEFAULT_MASK          (0xFFFCFFFF)

struct cport {
    struct unipro_driver *driver;
    uint32_t *peer_rx_buf;          // Address in peer memory to place payload
                                    //     for transfer mode 1
    uint8_t *tx_buf;                // TX region for this CPort
    uint8_t *rx_buf;                // RX region for this CPort
    uint16_t cportid;
    int connected;
};

#define CPORT_RX_BUF_BASE         (0x20000000U)
#define CPORT_RX_BUF_SIZE         (CPORT_BUF_SIZE)
#define CPORT_RX_BUF(cport)       (void*)(CPORT_RX_BUF_BASE + \
                                      (CPORT_RX_BUF_SIZE * cport))
#define CPORT_TX_BUF_BASE         (0x50000000U)
#define CPORT_TX_BUF_SIZE         (0x20000U)
#define CPORT_TX_BUF(cport)       (uint8_t*)(CPORT_TX_BUF_BASE + \
                                      (CPORT_TX_BUF_SIZE * cport))
#define CPORT_EOM_BIT(cport)      (cport->tx_buf + (CPORT_TX_BUF_SIZE - 1))

#define DECLARE_CPORT(id) {            \
    .peer_rx_buf = NULL,               \
    .tx_buf      = CPORT_TX_BUF(id),   \
    .rx_buf      = CPORT_RX_BUF(id),   \
    .cportid     = id,                 \
    .connected   = 0,                  \
}

#define CPORTID_CDSI0    (16)
#define CPORTID_CDSI1    (17)

static struct cport cporttable[] = {
    DECLARE_CPORT(0),  DECLARE_CPORT(1),  DECLARE_CPORT(2),  DECLARE_CPORT(3),
    DECLARE_CPORT(4),  DECLARE_CPORT(5),  DECLARE_CPORT(6),  DECLARE_CPORT(7),
    DECLARE_CPORT(8),  DECLARE_CPORT(9),  DECLARE_CPORT(10), DECLARE_CPORT(11),
    DECLARE_CPORT(12), DECLARE_CPORT(13), DECLARE_CPORT(14), DECLARE_CPORT(15),
    DECLARE_CPORT(16), DECLARE_CPORT(17), DECLARE_CPORT(18), DECLARE_CPORT(19),
    DECLARE_CPORT(20), DECLARE_CPORT(21), DECLARE_CPORT(22), DECLARE_CPORT(23),
    DECLARE_CPORT(24), DECLARE_CPORT(25), DECLARE_CPORT(26), DECLARE_CPORT(27),
    DECLARE_CPORT(28), DECLARE_CPORT(29), DECLARE_CPORT(30), DECLARE_CPORT(31),
};

static inline struct cport *cport_handle(uint16_t cportid) {
    // 16 and 17 are reserved for CDSI
    if (cportid >= unipro_cport_count() ||
        cportid == CPORTID_CDSI0 || cportid == CPORTID_CDSI1) {
        return NULL;
    } else {
        return &cporttable[cportid];
    }
}

#define irqn_to_cport(irqn)          cport_handle((irqn - TSB_IRQ_UNIPRO_RX_EOM00))
#define cportid_to_irqn(cportid)     (TSB_IRQ_UNIPRO_RX_EOM00 + cportid)

/* Helpers */
static uint32_t cport_get_status(struct cport*);
static inline void clear_rx_interrupt(struct cport*);
static inline void enable_rx_interrupt(struct cport*);
static void enable_e2efc(void);
static void configure_transfer_mode(int);
static void dump_regs(void);

/* irq handlers */
static void irq_rx_eom(int, void*);

/**
 * @brief Read CPort status from unipro controller
 * @param cport cport to retrieve status for
 * @return 0: CPort is configured and usable
 *         1: CPort is not connected
 *         2: reserved
 *         3: CPort is connected to a TC which is not present in the peer
 *          Unipro node
 */
static uint32_t cport_get_status(struct cport *cport) {
    uint32_t val;
    uint32_t cportid = cport->cportid;

    val = read32(UNIPRO_CPORT_STATUS_REG(cportid));
    return UNIPRO_CPORT_STATUS(val, cportid);
}

/**
 * @brief Enable a CPort that has a connected connection.
 */
static int configure_connected_cport(unsigned int cportid) {
    int ret = 0;
    struct cport *cport;
    unsigned int rc;
    uint32_t peer_cportid;

    cport = cport_handle(cportid);
    if (!cport) {
        return -EINVAL;
    }
    rc = cport_get_status(cport);
    switch (rc) {
    case UNIPRO_CPORT_STATUS_CONNECTED:
        /* Point destination address at peer cport rx buffer */
        unipro_attr_read(T_PEERCPORTID,
                         &peer_cportid,
                         cport->cportid,
                         0,
                         &rc);
        cport->peer_rx_buf = CPORT_RX_BUF(peer_cportid);
        cport->connected = 1;
        break;
    case UNIPRO_CPORT_STATUS_UNCONNECTED:
        ret = -ENOTCONN;
        break;
    default:
        lldbg("Unexpected status: CP%u: status: 0x%u\n", cportid, rc);
        ret = -EIO;
    }
    return ret;
}

/**
 * @brief Clear UniPro interrupts on a given cport
 * @param cport cport
 */
static inline void clear_rx_interrupt(struct cport *cport) {
    unsigned int cportid = cport->cportid;
    write32(UNIPRO_AHM_RX_EOM_INT_BEF_REG(cportid),
            UNIPRO_AHM_RX_EOM_INT_BEF(cportid));
}

/**
 * @brief Enable UniPro RX interrupts on a given cport
 * @param cport cport
 */
static inline void enable_rx_interrupt(struct cport *cport) {
    unsigned int cportid = cport->cportid;
    uint32_t *reg = UNIPRO_AHM_RX_EOM_INT_EN_REG(cportid);
    uint32_t bit = UNIPRO_AHM_RX_EOM_INT_EN(cportid);

    read32(reg) |= bit;
}

/**
 * @brief RX EOM interrupt handler
 * @param irq irq number
 * @param context register context (unused)
 */
static void irq_rx_eom(int irq, void *data) {
    struct cport *cport = irqn_to_cport(irq);
    data = cport->rx_buf;

    clear_rx_interrupt(cport);

    if (!cport->driver) {
        lldbg("dropping message on cport %hu where no driver is registered\n",
              cport->cportid);
        return;
    }

    DBG_UNIPRO("cport: %u driver: %s payload=0x%x\n",
                cport->cportid,
                cport->driver->name,
                data);

    if (cport->driver->rx_handler) {
        /*
         * TODO: get length information from mailbox
         */
        cport->driver->rx_handler(cport->cportid, data, CPORT_BUF_SIZE);
    }
}

int unipro_unpause_rx(unsigned int cportid)
{
    return -ENOSYS;
}

/**
 * @brief Clear and disable UniPro interrupt
 */
static void clear_int(unsigned int cportid) {
    unsigned int i;
    uint32_t int_en;

    i = cportid * 2;
    if (cportid < 16) {
        putreg32(0x3 << i, UNIPRO_AHM_RX_EOM_INT_BEF_0);
        int_en = read32(UNIPRO_AHM_RX_EOM_INT_EN_0);
        int_en &= ~(0x3 << i);
        putreg32(int_en, UNIPRO_AHM_RX_EOM_INT_EN_0);
    } else {
        putreg32(0x3 << i, UNIPRO_AHM_RX_EOM_INT_BEF_1);
        int_en = read32(UNIPRO_AHM_RX_EOM_INT_EN_1);
        int_en &= ~(0x3 << i);
        putreg32(int_en, UNIPRO_AHM_RX_EOM_INT_EN_1);
    }

    irq_clear(cportid_to_irqn(cportid));
}

/**
 * @brief Enable EOM interrupt on cport
 */
static void enable_int(unsigned int cportid) {
    struct cport *cport;
    unsigned int irqn;

    cport = cport_handle(cportid);
    if (!cport || !cport->connected) {
        return;
    }

    irqn = cportid_to_irqn(cportid);
    enable_rx_interrupt(cport);
    irq_attach(irqn, irq_rx_eom, NULL);
    irq_enable_line(irqn);
}

static void enable_e2efc(void) {
    putreg32(E2EFC_DEFAULT_MASK, UNIPRO_CPB_TX_E2EFC_EN);
    putreg32(E2EFC_DEFAULT_MASK, UNIPRO_CPB_RX_E2EFC_EN);
}

static void configure_transfer_mode(int mode) {
    /*
     * Set transfer mode 1
     */
    switch (mode) {
    case 1:
        putreg32(TRANSFER_MODE1_DEFAULT_MASK, UNIPRO_AHM_MODE_CTRL_0);
        putreg32(TRANSFER_MODE1_DEFAULT_MASK, UNIPRO_AHM_MODE_CTRL_1);
        break;
    default:
        lldbg("Unsupported transfer mode: %u\n", mode);
        break;
    }
}

/**
 * @brief UniPro debug dump
 */
static void dump_regs(void) {
    struct cport *cport;
    uint32_t val;
    unsigned int rc;
    unsigned int i;

#define DBG_ATTR(attr) do {                  \
    unipro_attr_read(attr, &val, 0, 0, &rc); \
    lldbg("    [%s]: 0x%x\n", #attr, val);   \
} while (0);

#define DBG_CPORT_ATTR(attr, cportid) do {         \
    unipro_attr_read(attr, &val, cportid, 0, &rc); \
    lldbg("    [%s]: 0x%x\n", #attr, val);         \
} while (0);

#define REG_DBG(reg) do {                 \
    val = read32(reg);                  \
    lldbg("    [%s]: 0x%x\n", #reg, val); \
} while (0)

    lldbg("DME Attributes\n");
    lldbg("========================================\n");
    DBG_ATTR(PA_ACTIVETXDATALANES);
    DBG_ATTR(PA_ACTIVETXDATALANES);
    DBG_ATTR(PA_TXGEAR);
    DBG_ATTR(PA_TXTERMINATION);
    DBG_ATTR(PA_HSSERIES);
    DBG_ATTR(PA_PWRMODE);
    DBG_ATTR(PA_ACTIVERXDATALANES);
    DBG_ATTR(PA_RXGEAR);
    DBG_ATTR(PA_RXTERMINATION);
    DBG_ATTR(PA_PWRMODEUSERDATA0);
    DBG_ATTR(N_DEVICEID);
    DBG_ATTR(N_DEVICEID_VALID);
    DBG_ATTR(DME_DDBL1_REVISION);
    DBG_ATTR(DME_DDBL1_LEVEL);
    DBG_ATTR(DME_DDBL1_DEVICECLASS);
    DBG_ATTR(DME_DDBL1_MANUFACTURERID);
    DBG_ATTR(DME_DDBL1_PRODUCTID);
    DBG_ATTR(DME_DDBL1_LENGTH);
    DBG_ATTR(TSB_DME_DDBL2_A);
    DBG_ATTR(TSB_DME_DDBL2_B);
    DBG_ATTR(TSB_MAILBOX);
    DBG_ATTR(TSB_MAXSEGMENTCONFIG);
    DBG_ATTR(TSB_DEBUGTXBYTECOUNT);
    DBG_ATTR(TSB_DEBUGRXBYTECOUNT);

    lldbg("Active CPort Configuration:\n");
    lldbg("========================================\n");
    for (i = 0; i < unipro_cport_count(); i++) {
        cport = cport_handle(i);
        if (!cport) {
            continue;
        }
        if (cport->connected) {
            lldbg("CP%d:\n", i);
            DBG_CPORT_ATTR(T_CONNECTIONSTATE, i);
            DBG_CPORT_ATTR(T_PEERDEVICEID, i);
            DBG_CPORT_ATTR(T_PEERCPORTID, i);
            DBG_CPORT_ATTR(T_TRAFFICCLASS, i);
            DBG_CPORT_ATTR(T_CPORTFLAGS, i);
        }
    }

    lldbg("Unipro Interrupt Info:\n");
    lldbg("========================================\n");
    REG_DBG(UNIPRO_AHM_RX_EOM_INT_EN_0);
    REG_DBG(UNIPRO_AHM_RX_EOM_INT_BEF_0);
    REG_DBG(UNIPRO_AHM_RX_EOM_INT_AFT_0);

    REG_DBG(UNIPRO_AHM_RX_EOM_INT_EN_1);
    REG_DBG(UNIPRO_AHM_RX_EOM_INT_BEF_1);
    REG_DBG(UNIPRO_AHM_RX_EOM_INT_AFT_1);

    REG_DBG(UNIPRO_UNIPRO_INT_EN);
    REG_DBG(UNIPRO_UNIPRO_INT_BEF);
    REG_DBG(UNIPRO_UNIPRO_INT_AFT);
}

/*
 * public interfaces
 */

unsigned int unipro_cport_count(void)
{
    /* HACK: just putting this here to keep the build from breaking on
     * ES1, which will (hopefully) be gone soon. */
    return 32;
}

/**
 * @brief Print out a bunch of debug information on the console
 */
void unipro_info(void) {
    dump_regs();
}

/**
 * @brief Initialize one UniPro cport
 */
int unipro_init_cport(unsigned int cportid) {
    int ret;
    struct cport *cport = cport_handle(cportid);

    if (!cport)
        return -EINVAL;

    if (cport->connected)
        return 0;

    /*
     * Initialize cport.
     */
    ret = configure_connected_cport(cportid);
    if (ret)
        return ret;

    /*
     * Clear any pending EOM interrupts, then enable them.
     * TODO: Defer interrupt enable until driver registration?
     */
    irq_disable();
    clear_int(cportid);
    enable_int(cportid);
    irq_enable();

#ifdef UNIPRO_DEBUG
    unipro_info();
#endif

    return 0;
}

/**
 * @brief Initialize the UniPro core
 */
void unipro_init(void) {
    unsigned int i;

    /*
     * Unipro controller automatically issues a link startup request
     * out of reset. It is assumed that the SVC has configured the M-PHY
     * and CPort configuration.
     */
    RET_IF_FAIL(ARRAY_SIZE(cporttable) <= 32,);

    /*
     * Enable end-to-end flow control on all cports except CSI/DSI
     */
    enable_e2efc();

    /*
     * Set transfer mode 1 on all cports
     * Sender chooses destination address in peer memory
     * Header is deleted by destination receiver
     */
    RET_IF_FAIL(TRANSFER_MODE == 1,);
    configure_transfer_mode(TRANSFER_MODE);

    /*
     * Initialize connected cports.
     */
    putreg32(0x0, UNIPRO_UNIPRO_INT_EN);
    for (i = 0; i < unipro_cport_count(); i++) {
        unipro_init_cport(i);
    }
    putreg32(0x1, UNIPRO_UNIPRO_INT_EN);

#ifdef UNIPRO_DEBUG
    unipro_info();
#endif
    lldbg("UniPro enabled\n");
}

int unipro_send_async(unsigned int cportid, const void *buf, size_t len,
                      unipro_send_completion_t callback, void *priv)
{
    int retval;

    retval = unipro_send(cportid, buf, len);

    if (callback)
        callback(retval, buf, priv);

    return 0;
}

/**
 * @brief send data down a CPort
 * @param cportid cport to send down
 * @param buf data buffer
 * @param len size of data to send
 * @param 0 on success, <0 on error
 */
int unipro_send(unsigned int cportid, const void *buf, size_t len) {
    unsigned int i;
    struct cport *cport;
    char *data = (char*)buf;

    if (cportid >= unipro_cport_count() || len > CPORT_BUF_SIZE) {
        return -EINVAL;
    }

    cport = cport_handle(cportid);
    if (!cport) {
        return -EINVAL;
    }

    if (!cport->connected) {
        lldbg("CP%d unconnected\n", cport->cportid);
        return -EPIPE;
    }

    /*
     * Send ES1 packet header:
     * peer_rx_buf (4): Address of the peer cport receive buffer
     * reserved    (4): Toshiba requires this header in mode 1 transfer.
     *                  It is deleted by hardware on the receiver end.
     */
    RET_IF_FAIL(TRANSFER_MODE == 1, -EINVAL);
    DBG_UNIPRO("Sending %u bytes to CP%d peer_rx_buf=0x%08x\n",
               len,
               cport->cportid,
               (unsigned int)cport->peer_rx_buf);
    write32(&cport->tx_buf[0], (uint32_t)cport->peer_rx_buf);
    write32(&cport->tx_buf[4], 0xDEADBEEF); // Reserved header, unused

    /*
     * Data payload
     */
    for (i = 0; i < len; i++) {
        DBG_UNIPRO("\t[%u]: %02x\n", i, data[i]);
        write8(&cport->tx_buf[i], data[i]);
    }

    /* Hit EOM */
    write8(CPORT_EOM_BIT(cport), 1);

    return 0;
}

/**
 * @brief Perform a DME get request
 * @param attr DME attribute address
 * @param val destination to read into
 * @param peer 1 if peer access, 0 if local
 * @param result_code destination for access result
 * @return 0
 */
int unipro_attr_read(uint16_t attr,
                     uint32_t *val,
                     uint16_t selector,
                     int peer,
                     uint32_t *result_code) {
    uint32_t ctrl;
    ctrl = REG_PEERENA(peer)    |    // Peer or local access
           REG_SELECT(selector) |    // Selector index
           attr;
    write32(UNIPRO_A2D_ATTRACS_CTRL_00, ctrl);

    /* Start the access */
    write32(UNIPRO_A2D_ATTRACS_MSTR_CTRL, REG_ATTRACS_CNT(1) | REG_ATTRACS_UPD);

    while (!read32(UNIPRO_A2D_ATTRACS_INT_BEF))
        ;

    /* Clear status bit */
    write32(UNIPRO_A2D_ATTRACS_INT_BEF, 1);

    *result_code = read32(UNIPRO_A2D_ATTRACS_STS_00);
    *val         = read32(UNIPRO_A2D_ATTRACS_DATA_STS_00);

    return 0;
}

int unipro_attr_write(uint16_t attr,
                      uint32_t val,
                      uint16_t selector,
                      int peer,
                      uint32_t *result_code) {
    return -ENOSYS;
}

/**
 * @brief Register a driver with the unipro core
 * @param drv unipro driver to register
 * @param cportid cport number to associate this driver to
 * @return 0 on success, <0 on error
 */
int unipro_driver_register(struct unipro_driver *driver, unsigned int cportid) {
    struct cport *cport = cport_handle(cportid);
    if (!cport) {
        return -ENODEV;
    }

    if (cport->driver) {
        lldbg("ERROR: Already registered by: %s\n", cport->driver->name);
        return -EEXIST;
    }

    cport->driver = driver;

    lldbg("Registered driver %s on CP%u\n", cport->driver->name, cport->cportid);
    return 0;
}

int unipro_driver_unregister(unsigned int cportid)
{
    struct cport *cport = cport_handle(cportid);
    if (!cport) {
        return -ENODEV;
    }

    cport->driver = NULL;

    return 0;
}
