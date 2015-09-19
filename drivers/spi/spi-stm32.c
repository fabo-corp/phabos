/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/hwio.h>
#include <phabos/spi.h>
#include <phabos/spi/spi-stm32.h>
#include <phabos/driver.h>
#include <phabos/utils.h>

#include <errno.h>

#define SPI_CR1 0x00
#define SPI_CR2 0x04
#define SPI_SR  0x08
#define SPI_DR  0x0c

#define SPI_CR1_CPHA        (1 << 1)
#define SPI_CR1_CPOL        (1 << 1)
#define SPI_CR1_MSTR        (1 << 2)
#define SPI_CR1_BR_DIV2     (0 << 3)
#define SPI_CR1_BR_DIV4     (1 << 3)
#define SPI_CR1_BR_DIV8     (2 << 3)
#define SPI_CR1_BR_DIV16    (3 << 3)
#define SPI_CR1_BR_DIV32    (4 << 3)
#define SPI_CR1_BR_DIV64    (5 << 3)
#define SPI_CR1_BR_DIV128   (6 << 3)
#define SPI_CR1_BR_DIV256   (7 << 3)
#define SPI_CR1_SPE         (1 << 6)
#define SPI_CR1_LSBFIRST    (1 << 7)
#define SPI_CR1_SSI         (1 << 8)
#define SPI_CR1_SSM         (1 << 9)
#define SPI_CR1_DFF         (1 << 11)
#define SPI_CR1_BIDIMODE    (1 << 15)

#define SPI_SR_TXE          (1 << 1)
#define SPI_SR_RXNE         (1 << 0)

#define SPI_FREQ_MASK       (3 << 3)
#define SPI_BPW_MASK        SPI_CR1_DFF
#define SPI_MODE_MASK       (SPI_CR1_CPHA | SPI_CR1_CPOL)

static void spi_dump(void)
{
    kprintf("CR1: %X\n", read32(0x40013000));
    kprintf("SR: %X\n", read32(0x40013008));
}

static int stm32_spi_set_mode(struct spi_master *spi, unsigned mode)
{
    uint32_t cr1 = read32(spi->device.reg_base + SPI_CR1);
    cr1 &= ~SPI_MODE_MASK;

    switch (mode) {
    case SPI_MODE_0:
        break;

    case SPI_MODE_1:
        cr1 |= SPI_CR1_CPHA;
        break;

    case SPI_MODE_2:
        cr1 |= SPI_CR1_CPOL;
        break;

    case SPI_MODE_3:
        cr1 |= SPI_CR1_CPHA | SPI_CR1_CPOL;
        break;

    default:
        return -EINVAL;
    }

    write32(spi->device.reg_base + SPI_CR1, cr1);
    return 0;
}

static int stm32_spi_set_bpw(struct spi_master *spi, unsigned bpw)
{
    uint32_t cr1 = read32(spi->device.reg_base + SPI_CR1);
    cr1 &= ~SPI_BPW_MASK;

    switch (bpw) {
    case 8:
        break;

    case 16:
        cr1 |= SPI_CR1_DFF;
        break;

    default:
        return -EINVAL;
    }

    spi->bpw = bpw;

    write32(spi->device.reg_base + SPI_CR1, cr1);
    return 0;
}

static int stm32_spi_set_max_freq(struct spi_master *spi, unsigned freq)
{
    struct stm32_spi_master_platform *pdata = spi->device.pdata;
    uint32_t cr1 = read32(spi->device.reg_base + SPI_CR1);
    cr1 &= ~SPI_FREQ_MASK;

    unsigned quot = pdata->clk / freq;
    unsigned rem = pdata->clk % freq;
    if (rem)
        quot += 1;

    if (quot <= 2)
        cr1 |= SPI_CR1_BR_DIV2;
    else if (quot > 2 && quot <= 4)
        cr1 |= SPI_CR1_BR_DIV4;
    else if (quot > 4 && quot <= 8)
        cr1 |= SPI_CR1_BR_DIV8;
    else if (quot > 8 && quot <= 16)
        cr1 |= SPI_CR1_BR_DIV16;
    else if (quot > 16 && quot <= 32)
        cr1 |= SPI_CR1_BR_DIV32;
    else if (quot > 32 && quot <= 64)
        cr1 |= SPI_CR1_BR_DIV64;
    else if (quot > 64 && quot <= 128)
        cr1 |= SPI_CR1_BR_DIV128;
    else if (quot > 128 && quot <= 256)
        cr1 |= SPI_CR1_BR_DIV256;
    else
        return -EINVAL;

    write32(spi->device.reg_base + SPI_CR1, cr1);
    return 0;
}

static uint16_t stm32_spi_receive_word(struct spi_master *spi)
{
    while (!(read32(spi->device.reg_base + SPI_SR) & SPI_SR_RXNE));
    return read16(spi->device.reg_base + SPI_DR);
}

static void stm32_spi_send_word(struct spi_master *spi, uint16_t word)
{
    while (!(read32(spi->device.reg_base + SPI_SR) & SPI_SR_TXE));
    write16(spi->device.reg_base + SPI_DR, word);
}

static int stm32_spi_transfer_one8(struct spi_master *spi, struct spi_msg *msg)
{
    uint8_t *rx_buf = msg->rx_buffer;
    const uint8_t *tx_buf = msg->tx_buffer;

    for (unsigned i = 0; i < msg->length; i++) {
        uint8_t word = 0xff;

        if (msg->tx_buffer)
            word = tx_buf[i];
        stm32_spi_send_word(spi, word);

        word = stm32_spi_receive_word(spi);
        if (msg->rx_buffer)
            rx_buf[i] = word;
    }

    return 0;
}

static int stm32_spi_transfer_one16(struct spi_master *spi, struct spi_msg *msg)
{
    uint16_t *rx_buf = msg->rx_buffer;
    const uint16_t *tx_buf = msg->tx_buffer;

    if (msg->length & 1)
        dev_warn(&spi->device, "odd buffer length, skipping last byte\n");

    for (unsigned i = 0; i < msg->length / 2; i++) {
        if (msg->tx_buffer)
            stm32_spi_send_word(spi, tx_buf[i]);

        if (msg->rx_buffer)
            rx_buf[i] = stm32_spi_receive_word(spi);
    }

    return 0;
}

static inline void stm32_spi_enable(struct spi_master *spi)
{
    uint32_t cr1 = read32(spi->device.reg_base + SPI_CR1);
    write32(spi->device.reg_base + SPI_CR1, cr1 | SPI_CR1_SPE);
}

static inline void stm32_spi_disable(struct spi_master *spi)
{
    uint32_t cr1 = read32(spi->device.reg_base + SPI_CR1);
    write32(spi->device.reg_base + SPI_CR1, cr1 & ~SPI_CR1_SPE);
}

static ssize_t stm32_spi_transfer(struct spi_master *spi, struct spi_msg *msg,
                                  size_t count)
{
    typedef int (*transfer_one_t)(struct spi_master *spi, struct spi_msg *msg);
    transfer_one_t transfer_one = stm32_spi_transfer_one8;

    if (spi->bpw == 16)
        transfer_one = stm32_spi_transfer_one16;

    stm32_spi_enable(spi);

    for (unsigned i = 0; i < count; i++)
        transfer_one(spi, &msg[i]);

//    stm32_spi_disable(spi);

    return 0;
}

static struct spi_master_ops stm32_spi_master_ops = {
    .transfer = stm32_spi_transfer,
    .set_max_freq = stm32_spi_set_max_freq,
    .set_bpw = stm32_spi_set_bpw,
    .set_mode = stm32_spi_set_mode,
};

static int stm32_spi_probe(struct device *device)
{
    struct spi_master *spi = containerof(device, struct spi_master, device);
    spi->ops = &stm32_spi_master_ops;

    write32(device->reg_base + SPI_CR1, SPI_CR1_MSTR | SPI_CR1_SSI |
                                        SPI_CR1_SSM);
    return 0;
}

static int stm32_spi_remove(struct device *device)
{
    return 0;
}

__driver__ struct driver stm32_spi_driver = {
    .name = "stm32-spi",

    .probe = stm32_spi_probe,
    .remove = stm32_spi_remove,
};
