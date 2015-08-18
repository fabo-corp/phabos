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

#include <string.h>
#include <errno.h>

#include <phabos/list.h>
#include <phabos/greybus/unipro.h>
#include <phabos/unipro/unipro.h>
#include <phabos/unipro/tsb.h>
#include <phabos/greybus/tsb_unipro.h>
#include <phabos/semaphore.h>
#include <phabos/scheduler.h>
#include <phabos/kprintf.h>
#include <phabos/utils.h>

#include <asm/hwio.h>
#include <asm/tsb-irq.h>
#include <asm/irq.h>
#include <asm/delay.h>

#include "chip.h"
#include "scm.h"
#include "unipro-es2.h"
#include "es2-mphy-fixups.h"

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

struct cport {
    struct unipro_driver *driver;
    uint8_t *tx_buf;                // TX region for this CPort
    uint8_t *rx_buf;                // RX region for this CPort
    unsigned int cportid;
    int connected;

    struct list_head tx_fifo;
};

struct worker {
    struct task *thread;
    struct semaphore tx_fifo_lock;
};

static struct worker worker;

struct unipro_buffer {
    struct list_head list;
    unipro_send_completion_t callback;
    void *priv;
    bool som;
    int byte_sent;
    int len;
    const void *data;
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

#define CPORTID_CDSI0    (16)
#define CPORTID_CDSI1    (17)

#define CPB_TX_BUFFER_SPACE_MASK        ((uint32_t) 0x0000007F)
#define CPB_TX_BUFFER_SPACE_OFFSET_MASK CPB_TX_BUFFER_SPACE_MASK

static struct cport *cporttable;

#define APBRIDGE_CPORT_MAX 44 // number of CPorts available on the APBridges
#define GPBRIDGE_CPORT_MAX 16 // number of CPorts available on the GPBridges

unsigned int unipro_cport_count(void) {
    /*
     * Reduce the run-time CPort count to what's available on the
     * GPBridges, unless we can determine that we're running on an
     * APBridge.
     */
    return ((tsb_get_product_id() == tsb_pid_apbridge) ?
            APBRIDGE_CPORT_MAX : GPBRIDGE_CPORT_MAX);
}

static inline struct cport *cport_handle(unsigned int cportid) {
    if (cportid >= unipro_cport_count() || cportid == CPORTID_CDSI0 ||
        cportid == CPORTID_CDSI1) {
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
static void configure_transfer_mode(int);
static uint16_t unipro_get_tx_free_buffer_space(struct cport *cport);
static inline void unipro_set_eom_flag(struct cport *cport);
static int unipro_send_sync(unsigned int cportid,
                            const void *buf, size_t len, bool som);
static void dump_regs(void);

/* irq handlers */
static void irq_rx_eom(int, void*);
static void irq_unipro(int, void*);

/*
 * "Map" constants for M-PHY fixups.
 */
#define TSB_MPHY_MAP (0x7F)
    #define TSB_MPHY_MAP_TSB_REGISTER_1 (0x01)
    #define TSB_MPHY_MAP_NORMAL         (0x00)
    #define TSB_MPHY_MAP_TSB_REGISTER_2 (0x81)

static uint32_t unipro_read(uint32_t offset)
{
    return read32(AIO_UNIPRO_BASE + offset);
}

static void unipro_write(uint32_t offset, uint32_t v)
{
    write32(AIO_UNIPRO_BASE + offset, v);
}

static int es2_fixup_mphy(void)
{
    uint32_t debug_0720 = tsb_get_debug_reg(0x0720);
    uint32_t urc;
    const struct tsb_mphy_fixup *fu;

    /*
     * Apply the "register 2" map fixups.
     */
    unipro_attr_local_write(TSB_MPHY_MAP, TSB_MPHY_MAP_TSB_REGISTER_2, 0,
                            &urc);
    if (urc) {
        kprintf("%s: failed to switch to register 2 map: %u\n", __func__, urc);
        return urc;
    }
    fu = tsb_register_2_map_mphy_fixups;
    do {
        unipro_attr_local_write(fu->attrid, fu->value, fu->select_index,
                                &urc);
        if (urc) {
            kprintf("%s: failed to apply register 1 map fixup: %u\n",
                    __func__, urc);
            return urc;
        }
    } while (!tsb_mphy_fixup_is_last(fu++));

    /*
     * Switch to "normal" map.
     */
    unipro_attr_local_write(TSB_MPHY_MAP, TSB_MPHY_MAP_NORMAL, 0,
                            &urc);
    if (urc) {
        kprintf("%s: failed to switch to normal map: %u\n",
                __func__, urc);
        return urc;
    }

    /*
     * Apply the "register 1" map fixups.
     */
    unipro_attr_local_write(TSB_MPHY_MAP, TSB_MPHY_MAP_TSB_REGISTER_1, 0,
                            &urc);
    if (urc) {
        kprintf("%s: failed to switch to register 1 map: %u\n", __func__, urc);
        return urc;
    }
    fu = tsb_register_1_map_mphy_fixups;
    do {
        if (tsb_mphy_r1_fixup_is_magic(fu)) {
            /* The magic R1 fixups come from the mysterious and solemn
             * debug register 0x0720. */
            unipro_attr_local_write(0x8002, (debug_0720 >> 1) & 0x1f, 0, &urc);
        } else {
            unipro_attr_local_write(fu->attrid, fu->value, fu->select_index,
                                    &urc);
        }
        if (urc) {
            kprintf("%s: failed to apply register 1 map fixup: %u\n",
                  __func__, urc);
            return urc;
        }
    } while (!tsb_mphy_fixup_is_last(fu++));

    /*
     * Switch to "normal" map.
     */
    unipro_attr_local_write(TSB_MPHY_MAP, TSB_MPHY_MAP_NORMAL, 0,
                            &urc);
    if (urc) {
        kprintf("%s: failed to switch to normal map: %u\n",
              __func__, urc);
        return urc;
    }

    return 0;
}

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
    unsigned int cportid = cport->cportid;
    uint32_t reg;

    reg = CPORT_STATUS_0 + ((cportid/16) << 2);
    val = unipro_read(reg);
    val >>= ((cportid % 16) << 1);
    return (val & (0x3));
}

/**
 * @brief Enable a CPort that has a connected connection.
 */
static int configure_connected_cport(unsigned int cportid) {
    int ret = 0;
    struct cport *cport;
    unsigned int rc;

    cport = cport_handle(cportid);
    if (!cport) {
        return -EINVAL;
    }
    rc = cport_get_status(cport);
    switch (rc) {
    case CPORT_STATUS_CONNECTED:
        cport->connected = 1;
        break;
    case CPORT_STATUS_UNCONNECTED:
        ret = -ENOTCONN;
        break;
    default:
        kprintf("Unexpected status: CP%u: status: 0x%u\n", cportid, rc);
        ret = -EIO;
    }
    return ret;
}

/**
 * @brief perform a DME access
 * @param attr attribute to access
 * @param val pointer to value to either read or write
 * @param peer 0 for local access, 1 for peer
 * @param write 0 for read, 1 for write
 * @param result_code unipro return code, optional
 */
int unipro_attr_access(uint16_t attr,
                       uint32_t *val,
                       uint16_t selector,
                       int peer,
                       int write,
                       uint32_t *result_code) {

    uint32_t ctrl = (REG_ATTRACS_CTRL_PEERENA(peer) |
                     REG_ATTRACS_CTRL_SELECT(selector) |
                     REG_ATTRACS_CTRL_WRITE(write) |
                     attr);

    unipro_write(A2D_ATTRACS_CTRL_00, ctrl);
    if (write) {
        unipro_write(A2D_ATTRACS_DATA_CTRL_00, *val);
    }

    /* Start the access */
    unipro_write(A2D_ATTRACS_MSTR_CTRL,
                 REG_ATTRACS_CNT(1) | REG_ATTRACS_UPD);

    while (!unipro_read(A2D_ATTRACS_INT_BEF))
        ;

    /* Clear status bit */
    unipro_write(A2D_ATTRACS_INT_BEF, 0x1);

    if (result_code) {
        *result_code = unipro_read(A2D_ATTRACS_STS_00);
    }

    if (!write) {
        *val = unipro_read(A2D_ATTRACS_DATA_STS_00);
    }

    return 0;
}

/**
 * @brief Clear UniPro interrupts on a given cport
 * @param cport cport
 */
static inline void clear_rx_interrupt(struct cport *cport) {
    unsigned int cportid = cport->cportid;
    unipro_write(AHM_RX_EOM_INT_BEF_REG(cportid),
                 AHM_RX_EOM_INT_BEF(cportid));
}

/**
 * @brief Enable UniPro RX interrupts on a given cport
 * @param cport cport
 */
static inline void enable_rx_interrupt(struct cport *cport) {
    unsigned int cportid = cport->cportid;
    uint32_t reg = AHM_RX_EOM_INT_EN_REG(cportid);
    uint32_t bit = AHM_RX_EOM_INT_EN(cportid);

    unipro_write(reg, unipro_read(reg) | bit);
}

/**
 * @brief RX EOM interrupt handler
 * @param irq irq number
 * @param context register context (unused)
 */
static void irq_rx_eom(int irq, void *priv)
{
    struct cport *cport = irqn_to_cport(irq);
    void *data = cport->rx_buf;
    uint32_t transferred_size;

    clear_rx_interrupt(cport);

    if (!cport->driver) {
        kprintf("dropping message on cport %u where no driver is registered\n",
              cport->cportid);
        return;
    }

    transferred_size = unipro_read(CPB_RX_TRANSFERRED_DATA_SIZE_00 +
                                   (cport->cportid * sizeof(uint32_t)));
    DBG_UNIPRO("cport: %u driver: %s size=%u payload=0x%p\n",
                cport->cportid,
                cport->driver->name, transferred_size,
                data);

    if (cport->driver->rx_handler) {
        cport->driver->rx_handler(cport->cportid, data,
                                  (size_t)transferred_size);
    }
}

/**
 * @brief See ENG-376.
 *
 * We use a mailbox notification from the SVC to solve a race condition
 * involving FCT transmission. When the SVC makes a connection, it sets all the
 * relevant DME parameters as defined in MIPI UniPro 1.6, then pokes the
 * bridge mailbox, telling it that it is safe to send FCTs on a given CPort.
 */
static void irq_unipro(int irq, void *context) {
    uint32_t cportid;
    int rc;
    uint32_t e2efc;
    uint32_t val;
    DBG_UNIPRO("mailbox interrupt received irq: %d \n", irq);

    /*
     * Clear the initial interrupt
     */
    rc = unipro_attr_local_read(TSB_INTERRUPTSTATUS, &val, 0, NULL);
    if (rc) {
        goto done;
    }

    /*
     * Figure out which CPort to turn on FCT. The desired CPort is always
     * the mailbox value - 1.
     */
    rc = unipro_attr_local_read(TSB_MAILBOX, &cportid, 0, NULL);
    if (rc) {
        goto done;
    }
    cportid--;

    DBG_UNIPRO("Enabling E2EFC on cport %u\n", cportid);
    if (cportid < 32) {
        e2efc = unipro_read(CPB_RX_E2EFC_EN_0);
        e2efc |= (1 << cportid);
        unipro_write(CPB_RX_E2EFC_EN_0, e2efc);
    } else if (cportid < APBRIDGE_CPORT_MAX) {
        e2efc = unipro_read(CPB_RX_E2EFC_EN_1);
        e2efc |= (1 << (cportid - 32));
        unipro_write(CPB_RX_E2EFC_EN_1, e2efc);
    }

    configure_connected_cport(cportid);

    /*
     * Clear the mailbox. This triggers another a local interrupt which we have
     * to clear.
     */
    rc = unipro_attr_local_write(TSB_MAILBOX, 0, 0, NULL);
    if (rc) {
        goto done;
    }

    rc = unipro_attr_local_read(TSB_INTERRUPTSTATUS, &val, 0, NULL);
    if (rc) {
        goto done;
    }

    rc = unipro_attr_local_read(TSB_MAILBOX, &cportid, 0, NULL);
    if (rc) {
        goto done;
    }

done:
    irq_clear(TSB_IRQ_UNIPRO);
}

int unipro_unpause_rx(unsigned int cportid)
{
    struct cport *cport;

    cport = cport_handle(cportid);
    if (!cport || !cport->connected) {
        return -EINVAL;
    }

    /* Restart the flow of received data */
    unipro_write(REG_RX_PAUSE_SIZE_00 + (cport->cportid * sizeof(uint32_t)),
                 (1 << 31) | CPORT_BUF_SIZE);

    return 0;
}

/**
 * @brief Clear and disable UniPro interrupt
 */
static void clear_int(unsigned int cportid) {
    unsigned int i;
    uint32_t int_en;

    i = cportid * 2;
    if (cportid < 16) {
        unipro_write(AHM_RX_EOM_INT_BEF_0, 0x3 << i);
        int_en = unipro_read(AHM_RX_EOM_INT_EN_0);
        int_en &= ~(0x3 << i);
        unipro_write(AHM_RX_EOM_INT_EN_0, int_en);
    } else if (cportid < 32) {
        unipro_write(AHM_RX_EOM_INT_BEF_1, 0x3 << i);
        int_en = unipro_read(AHM_RX_EOM_INT_EN_1);
        int_en &= ~(0x3 << i);
        unipro_write(AHM_RX_EOM_INT_EN_1, int_en);
    } else {
        unipro_write(AHM_RX_EOM_INT_BEF_2, 0x3 << i);
        int_en = unipro_read(AHM_RX_EOM_INT_EN_2);
        int_en &= ~(0x3 << i);
        unipro_write(AHM_RX_EOM_INT_EN_2, int_en);
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
    if (!cport) {
        return;
    }

    irqn = cportid_to_irqn(cportid);
    enable_rx_interrupt(cport);
    irq_attach(irqn, irq_rx_eom, NULL);
    irq_enable_line(irqn);
}

static void configure_transfer_mode(int mode) {
    /*
     * Set transfer mode 2
     */
    switch (mode) {
    case 2:
        unipro_write(AHM_MODE_CTRL_0, TRANSFER_MODE_2_CTRL_0);
        if (tsb_get_product_id() == tsb_pid_apbridge) {
            unipro_write(AHM_MODE_CTRL_1, TRANSFER_MODE_2_CTRL_1);
            unipro_write(AHM_MODE_CTRL_2, TRANSFER_MODE_2_CTRL_2);
        }
        break;
    default:
        kprintf("Unsupported transfer mode: %u\n", mode);
        break;
    }
}

/**
 * @brief           Return the free space in Unipro TX FIFO (in bytes)
 * @return          free space in Unipro TX FIFO (in bytes)
 * @param[in]       cport: CPort handle
 */
static uint16_t unipro_get_tx_free_buffer_space(struct cport *cport)
{
    unsigned int cportid = cport->cportid;
    uint32_t tx_space;

    tx_space = 8 * (unipro_read(CPB_TX_BUFFER_SPACE_REG(cportid)) &
                    CPB_TX_BUFFER_SPACE_MASK);
    tx_space -= 8 * (unipro_read(REG_TX_BUFFER_SPACE_OFFSET_REG(cportid)) &
                     CPB_TX_BUFFER_SPACE_OFFSET_MASK);

    return tx_space;
}

/**
 * @brief           Set EOM (End Of Message) flag
 * @param[in]       cport: CPort handle
 */
static inline void unipro_set_eom_flag(struct cport *cport)
{
    write8(CPORT_EOM_BIT(cport), 1);
}

/**
 * @brief UniPro debug dump
 */
static void dump_regs(void) {
    uint32_t val;
    unsigned int rc;
    unsigned int i;

#define DBG_ATTR(attr) do {                  \
    unipro_attr_local_read(attr, &val, 0, &rc); \
    kprintf("    [%s]: 0x%x\n", #attr, val);   \
} while (0);

#define DBG_CPORT_ATTR(attr, cportid) do {         \
    unipro_attr_local_read(attr, &val, cportid, &rc); \
    kprintf("    [%s]: 0x%x\n", #attr, val);         \
} while (0);

#define REG_DBG(reg) do {                 \
    val = unipro_read(reg);               \
    kprintf("    [%s]: 0x%x\n", #reg, val); \
} while (0)

    kprintf("DME Attributes\n");
    kprintf("========================================\n");
    DBG_ATTR(PA_ACTIVETXDATALANES);
    DBG_ATTR(PA_ACTIVERXDATALANES);
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
    DBG_ATTR(TSB_DME_POWERMODEIND);

    kprintf("Unipro Interrupt Info:\n");
    kprintf("========================================\n");
    REG_DBG(UNIPRO_INT_EN);
    REG_DBG(AHM_RX_EOM_INT_EN_0);
    REG_DBG(AHM_RX_EOM_INT_EN_1);

    REG_DBG(UNIPRO_INT_BEF);
    REG_DBG(AHS_TIMEOUT_INT_BEF_0);
    REG_DBG(AHS_TIMEOUT_INT_BEF_1);
    REG_DBG(AHM_HRESP_ERR_INT_BEF_0);
    REG_DBG(AHM_HRESP_ERR_INT_BEF_1);
    REG_DBG(CPB_RX_E2EFC_RSLT_ERR_INT_BEF_0);
    REG_DBG(CPB_RX_E2EFC_RSLT_ERR_INT_BEF_1);
    REG_DBG(CPB_TX_RSLTCODE_ERR_INT_BEF_0);
    REG_DBG(CPB_TX_RSLTCODE_ERR_INT_BEF_1);
    REG_DBG(CPB_RX_MSGST_ERR_INT_BEF_0);
    REG_DBG(CPB_RX_MSGST_ERR_INT_BEF_1);
    REG_DBG(LUP_INT_BEF);
    REG_DBG(A2D_ATTRACS_INT_BEF);
    REG_DBG(AHM_RX_EOM_INT_BEF_0);
    REG_DBG(AHM_RX_EOM_INT_BEF_1);
    REG_DBG(AHM_RX_EOM_INT_BEF_2);
    REG_DBG(AHM_RX_EOT_INT_BEF_0);
    REG_DBG(AHM_RX_EOT_INT_BEF_1);

    kprintf("Unipro Registers:\n");
    kprintf("========================================\n");
    REG_DBG(AHM_MODE_CTRL_0);
    if (tsb_get_product_id() == tsb_pid_apbridge) {
        REG_DBG(AHM_MODE_CTRL_1);
        REG_DBG(AHM_MODE_CTRL_2);
    }
    REG_DBG(AHM_ADDRESS_00);
    REG_DBG(REG_RX_PAUSE_SIZE_00);
    REG_DBG(CPB_RX_TRANSFERRED_DATA_SIZE_00);
    REG_DBG(CPB_TX_BUFFER_SPACE_00);
    REG_DBG(CPB_TX_RESULTCODE_0);
    REG_DBG(AHS_HRESP_MODE_0);
    REG_DBG(AHS_TIMEOUT_00);
    REG_DBG(CPB_TX_E2EFC_EN_0);
    REG_DBG(CPB_TX_E2EFC_EN_1);
    REG_DBG(CPB_RX_E2EFC_EN_0);
    REG_DBG(CPB_RX_E2EFC_EN_1);
    REG_DBG(CPORT_STATUS_0);
    REG_DBG(CPORT_STATUS_1);
    REG_DBG(CPORT_STATUS_2);

    kprintf("Connected CPorts:\n");
    kprintf("========================================\n");
    for (i = 0; i < unipro_cport_count(); i++) {
        struct cport *cport = cport_handle(i);
        if (!cport)
            continue;

        val = cport_get_status(cport);

        if (val == CPORT_STATUS_CONNECTED) {
            kprintf("CPORT %u:\n", i);
            DBG_CPORT_ATTR(T_PEERDEVICEID, i);
            DBG_CPORT_ATTR(T_PEERCPORTID, i);
            DBG_CPORT_ATTR(T_TRAFFICCLASS, i);
            DBG_CPORT_ATTR(T_CPORTFLAGS, i);
            DBG_CPORT_ATTR(T_LOCALBUFFERSPACE, i);
            DBG_CPORT_ATTR(T_PEERBUFFERSPACE, i);
            DBG_CPORT_ATTR(T_CREDITSTOSEND, i);
            DBG_CPORT_ATTR(T_RXTOKENVALUE, i);
            DBG_CPORT_ATTR(T_TXTOKENVALUE, i);
            DBG_CPORT_ATTR(T_CONNECTIONSTATE, i);
        }
    }
}

/*
 * public interfaces
 */

/**
 * @brief Print out a bunch of debug information on the console
 */
void unipro_info(void)
{
    dump_regs();
}

/**
 * @brief Initialize one UniPro cport
 */
int unipro_init_cport(unsigned int cportid)
{
    struct cport *cport = cport_handle(cportid);

    if (!cport) {
        return -EINVAL;
    }

    if (cport->connected)
        return 0;


    /*
     * FIXME: We presently specify a fixed receive buffer address
     *        for each CPort.  That approach won't work for a
     *        pipelined zero-copy system.
     */
    unipro_write(AHM_ADDRESS_00 + (cportid * sizeof(uint32_t)),
                 (uint32_t)cport->rx_buf);

    /* Start the flow of received data */
    unipro_write(REG_RX_PAUSE_SIZE_00 + (cportid * sizeof(uint32_t)),
                 (1 << 31) | CPORT_BUF_SIZE);

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

static void unipro_dequeue_tx_buffer(struct unipro_buffer *buffer, int status)
{
    RET_IF_FAIL(buffer,);

    irq_disable();
    list_del(&buffer->list);
    irq_enable();

    if (buffer->callback) {
        buffer->callback(status, buffer->data, buffer->priv);
    }

    kfree(buffer);
}

/**
 * @brief           send data over given UniPro CPort
 * @return          0 on success, -EINVAL on invalid parameter,
 *                  -EBUSY when buffer could not be completely transferred
 *                  (unipro_send_tx_buffer() shall be called again until
 *                  buffer is entirely sent (return value == 0)).
 * @param[in]       operation: greybus loopback operation
 */
static int unipro_send_tx_buffer(struct cport *cport)
{
    struct unipro_buffer *buffer;
    int retval;

    if (!cport) {
        return -EINVAL;
    }

    irq_disable();

    if (list_is_empty(&cport->tx_fifo)) {
        irq_enable();
        return 0;
    }

    buffer = list_entry(cport->tx_fifo.next, struct unipro_buffer, list);

    irq_enable();

    retval = unipro_send_sync(cport->cportid,
                              buffer->data + buffer->byte_sent,
                              buffer->len - buffer->byte_sent, buffer->som);
    if (retval < 0) {
        unipro_dequeue_tx_buffer(buffer, retval);
        kprintf("unipro_send_sync failed. Dropping message...\n");
        return -EINVAL;
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

/**
 * @brief           Send data buffer(s) on CPort whenever ready.
 *                  Ensure that TX queues are reinspected until
 *                  all CPorts have no work available.
 *                  Then suspend again until new data is available.
 */
static void unipro_tx_worker(void *data)
{
    int i;
    bool is_busy;
    int retval;

    while (1) {
        /* Block until a buffer is pending on any CPort */
        semaphore_down(&worker.tx_fifo_lock);

        do {
            is_busy = false;

            for (i = 0; i < unipro_cport_count(); i++) {
                /* Browse all CPorts sending any pending buffers */
                retval = unipro_send_tx_buffer(cport_handle(i));
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

/**
 * @brief Initialize the UniPro core
 */
void unipro_init(void)
{
    unsigned int i;
    int retval;
    struct cport *cport;
    size_t table_size = sizeof(struct cport) * unipro_cport_count();


    semaphore_init(&worker.tx_fifo_lock, 0);

    worker.thread = task_run(unipro_tx_worker, NULL, 0);
    if (!worker.thread) {
        kprintf("Failed to create worker thread: %s.\n", strerror(errno));
        return;
    }

    cporttable = zalloc(table_size);
    if (!cporttable) {
        return;
    }

    for (i = 0; i < unipro_cport_count(); i++) {
        cport = &cporttable[i];
        cport->tx_buf = CPORT_TX_BUF(i);
        cport->rx_buf = CPORT_RX_BUF(i);
        cport->cportid = i;
        cport->connected = 0;
        list_init(&cport->tx_fifo);
    }

    if (es2_fixup_mphy()) {
        kprintf("Failed to apply M-PHY fixups (results in link instability at HS-G1).\n");
    }

    /*
     * Set transfer mode 2 on all cports
     * Receiver choses address for received message
     * Header is delivered transparently to receiver (and used to carry the first eight
     * L4 payload bytes)
     */
    RET_IF_FAIL(TRANSFER_MODE == 2,);
    configure_transfer_mode(TRANSFER_MODE);

    /*
     * Initialize cports.
     */
    unipro_write(UNIPRO_INT_EN, 0x0);
    for (i = 0; i < unipro_cport_count(); i++) {
        unipro_init_cport(i);
    }
    unipro_write(UNIPRO_INT_EN, 0x1);


    /*
     * Disable FCT transmission. See ENG-376.
     */
    unipro_write(CPB_RX_E2EFC_EN_0, 0x0);
    if (tsb_get_product_id() == tsb_pid_apbridge) {
        unipro_write(CPB_RX_E2EFC_EN_1, 0x0);
    }

    /*
     * Enable the mailbox interrupt
     */
    retval = unipro_attr_local_write(TSB_INTERRUPTENABLE, 0x1 << 15, 0, NULL);
    if (retval) {
        kprintf("Failed to enable mailbox interrupt\n");
    }
    irq_attach(TSB_IRQ_UNIPRO, irq_unipro, NULL);
    irq_enable_line(TSB_IRQ_UNIPRO);

#ifdef UNIPRO_DEBUG
    unipro_info();
#endif
    kprintf("UniPro enabled\n");
}

/**
 * @brief           send data over UniPro asynchronously (not blocking)
 * @return          0 on success, <0 otherwise
 * @param[in]       cportid: target CPort ID
 * @param[in]       buf: data buffer
 * @param[in]       len: data buffer length (in bytes)
 * @param[in]       callback: function called upon Tx completion
 * @param[in]       priv: optional argument passed to callback
 */
int unipro_send_async(unsigned int cportid, const void *buf, size_t len,
                      unipro_send_completion_t callback, void *priv)
{
    struct cport *cport;
    struct unipro_buffer *buffer;

    if (len > CPORT_BUF_SIZE) {
        return -EINVAL;
    }

    cport = cport_handle(cportid);
    if (!cport) {
        return -EINVAL;
    }

    if (!cport->connected) {
        kprintf("CP%u unconnected\n", cport->cportid);
        return -EPIPE;
    }

    RET_IF_FAIL(TRANSFER_MODE == 2, -EINVAL);

    buffer = zalloc(sizeof(*buffer));
    if (!buffer) {
        return -ENOMEM;
    }
    list_init(&buffer->list);
    buffer->som = true;
    buffer->len = len;
    buffer->callback = callback;
    buffer->priv = priv;
    buffer->data = buf;

    irq_disable();
    list_add(&cport->tx_fifo, &buffer->list);
    irq_enable();

    semaphore_up(&worker.tx_fifo_lock);
    return 0;
}

/**
 * @brief send data down a CPort
 * @param cportid cport to send down
 * @param buf data buffer
 * @param len size of data to send
 * @param 0 on success, <0 on error
 */
int unipro_send(unsigned int cportid, const void *buf, size_t len)
{
    int ret, sent;
    bool som;
    struct cport *cport;

    if (len > CPORT_BUF_SIZE) {
        return -EINVAL;
    }

    cport = cport_handle(cportid);
    if (!cport) {
        return -EINVAL;
    }

    for (som = true, sent = 0; sent < len;) {
        ret = unipro_send_sync(cportid, buf + sent, len - sent, som);
        if (ret < 0) {
            return ret;
        } else if (ret == 0) {
            continue;
        }
        sent += ret;
        som = false;
    }
    unipro_set_eom_flag(cport);

    return 0;
}

/**
 * @brief           Send data down to a CPort
 * @return          number of bytes effectively sent (>= 0), or error code (< 0)
 * @param[in]       cportid: cport to send down
 * @param[in]       buf: data buffer
 * @param[in]       len: size of data to send
 * @param[in]       som: "start of message" flag
 */
static int unipro_send_sync(unsigned int cportid,
                            const void *buf, size_t len, bool som)
{
    struct cport *cport;
    uint16_t count;
    uint8_t *tx_buf;

    if (len > CPORT_BUF_SIZE) {
        return -EINVAL;
    }

    cport = cport_handle(cportid);
    if (!cport) {
        return -EINVAL;
    }

    if (!cport->connected) {
        kprintf("CP%d unconnected\n", cport->cportid);
        return -EPIPE;
    }

    RET_IF_FAIL(TRANSFER_MODE == 2, -EINVAL);

    /*
     * If this is not the start of a new message,
     * message data must be written to first address of CPort Tx Buffer + 1.
     */
    if (!som) {
        tx_buf = cport->tx_buf + sizeof(uint32_t);
    } else {
        tx_buf = cport->tx_buf;
    }

    count = unipro_get_tx_free_buffer_space(cport);
    if (!count) {
        /* No free space in TX FIFO, cannot send anything. */
        DBG_UNIPRO("No free space in CP%d Tx Buffer\n", cportid);
        return 0;
    } else if (count > len) {
        count = len;
    }
    /* Copy message data in CPort Tx FIFO */
    DBG_UNIPRO("Sending %u bytes to CP%d\n", count, cportid);
    memcpy(tx_buf, buf, count);

    return (int) count;
}

/**
 * @brief Perform a DME get request
 * @param attr DME attribute address
 * @param val destination to read into
 * @param selector attribute selector index, or NCP_SELINDEXNULL if none
 * @param peer 1 if peer access, 0 if local
 * @param result_code destination for access result
 * @return 0
 */
int unipro_attr_read(uint16_t attr,
                     uint32_t *val,
                     uint16_t selector,
                     int peer,
                     uint32_t *result_code)
{
    return unipro_attr_access(attr, val, selector, peer, 0, result_code);
}

/**
 * @brief Perform a DME set request
 * @param attr DME attribute address
 * @param val value to write
 * @param selector attribute selector index, or NCP_SELINDEXNULL if none
 * @param peer 1 if peer access, 0 if local
 * @param result_code destination for access result
 * @return 0
 */
int unipro_attr_write(uint16_t attr,
                      uint32_t val,
                      uint16_t selector,
                      int peer,
                      uint32_t *result_code)
{
    return unipro_attr_access(attr, &val, selector, peer, 1, result_code);
}

/**
 * @brief Register a driver with the unipro core
 * @param drv unipro driver to register
 * @param cportid cport number to associate this driver to
 * @return 0 on success, <0 on error
 */
int unipro_driver_register(struct unipro_driver *driver, unsigned int cportid)
{
    struct cport *cport = cport_handle(cportid);
    if (!cport) {
        return -ENODEV;
    }

    if (cport->driver) {
        kprintf("ERROR: Already registered by: %s\n",
              cport->driver->name);
        return -EEXIST;
    }

    cport->driver = driver;

    kprintf("Registered driver %s on %sconnected CP%u\n",
          cport->driver->name, cport->connected ? "" : "un",
          cport->cportid);
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

/**
 * @brief Set the mailbox value and wait for it to be cleared.
 */
int tsb_unipro_mbox_set(uint32_t val, int peer) {
    int rc;

    rc = unipro_attr_write(TSB_MAILBOX, val, 0, peer, NULL);
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
        rc = unipro_attr_read(TSB_MAILBOX, &val, 0, peer, NULL);
        if (rc) {
            kprintf("%s(): TSB_MAILBOX poll failed: %d\n", __func__, rc);
        }
    } while (!rc && val != TSB_MAIL_RESET);

    return rc;
}
