#ifndef __PHABOS_SPI_H__
#define __PHABOS_SPI_H__

#include <stdlib.h>
#include <phabos/driver.h>

#define SPI_MODE_0 0
#define SPI_MODE_1 1
#define SPI_MODE_2 2
#define SPI_MODE_3 3

struct spi_master_ops;

struct spi_msg {
    const void *tx_buffer;
    void *rx_buffer;
    size_t length;
};

struct spi_master {
    struct device device;
    unsigned bpw;

    struct spi_master_ops *ops;
};

struct spi_master_ops {
    int (*transfer)(struct spi_master *spi, struct spi_msg *msg, size_t count);
    int (*set_max_freq)(struct spi_master *spi, unsigned freq);
    int (*set_bpw)(struct spi_master *spi, unsigned bpw);
    int (*set_mode)(struct spi_master *spi, unsigned mode);
};

static inline int spi_transfer(struct spi_master *spi, struct spi_msg *msg,
                               size_t count)
{
    return spi->ops->transfer(spi, msg, count);
}

static inline int spi_set_max_freq(struct spi_master *spi, unsigned freq)
{
    return spi->ops->set_max_freq(spi, freq);
}

static inline int spi_set_mode(struct spi_master *spi, unsigned mode)
{
    return spi->ops->set_mode(spi, mode);
}

static inline int spi_set_bpw(struct spi_master *spi, unsigned bpw)
{
    return spi->ops->set_bpw(spi, bpw);
}
#endif /* __PHABOS_SPI_H__ */

