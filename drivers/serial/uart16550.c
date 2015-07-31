#include <config.h>
#include <asm/hwio.h>
#include <asm/irq.h>
#include <phabos/driver.h>
#include <phabos/assert.h>
#include <phabos/utils.h>
#include <phabos/serial/uart16550.h>
#include <phabos/termios.h>

#include <errno.h>
#include <stdarg.h>

#define UART_RBR 0x00
#define UART_THR 0x00
#define UART_DLL 0x00
#define UART_DLM 0x04
#define UART_IER 0x04
#define UART_FCR 0x08
#define UART_IIR 0x08
#define UART_LCR 0x0c
#define UART_LSR 0x14

#define UART_LSR_DR                 (1 << 0)
#define UART_LSR_THRE               (1 << 5)

#define UART_LCR_DLS_5BITS          (0 << 0)
#define UART_LCR_DLS_6BITS          (1 << 0)
#define UART_LCR_DLS_7BITS          (2 << 0)
#define UART_LCR_DLS_8BITS          (3 << 0)
#define UART_LCR_STB_1              (0 << 2)
#define UART_LCR_STB_2              (1 << 2)
#define UART_LCR_PEN                (1 << 3)
#define UART_LCR_EPS                (1 << 4)
#define UART_LCR_STK                (1 << 5)
#define UART_LCR_BC                 (1 << 6)
#define UART_LCR_DLAB               (1 << 7)

#define UART_FCR_FIFOE              (1 << 0)
#define UART_FCR_RFIFOR             (1 << 1)
#define UART_FCR_XFIFOR             (1 << 2)
#define UART_FCR_DMAM               (1 << 3)
#define UART_FCR_RT_1CHAR           (0 << 6)
#define UART_FCR_RT_QFULL           (1 << 6)
#define UART_FCR_RT_HFULL           (2 << 6)
#define UART_FCR_RT_FULL_MINUS_2    (3 << 6)

#define UART_IER_ERBFI              (1 << 0)
#define UART_IER_ETBEI              (1 << 1)
#define UART_IER_ELSI               (1 << 2)
#define UART_IER_EDSSI              (1 << 3)

#define UART_IIR_IID_MASK           (3 << 1)

#define UART_IIR_IPEND              (1 << 0)

#define UART_IIR_IID_MS             (0 << 1)
#define UART_IIR_IID_THRE           (1 << 1)
#define UART_IIR_IID_RDA            (2 << 1)
#define UART_IIR_IID_RLS            (3 << 1)
#define UART_IIR_IID_CTI            (4 << 1)

__driver__ struct driver uart16550_driver;

static inline uint8_t uart16550_read8(struct uart16550_device *dev,
                                      unsigned int reg)
{
    RET_IF_FAIL(dev, 0);
    return read8((char*) dev->base + reg);
}

static inline void uart16550_write8(struct uart16550_device *dev,
                                    unsigned int reg, uint8_t value)
{
    RET_IF_FAIL(dev,);
    write8((char*) dev->base + reg, value);
}

static unsigned uart16550_get_next_byte(unsigned offset, size_t size)
{
    return (offset + 1) % size;
}

static unsigned uart16550_get_next_tx_byte(unsigned offset)
{
    return uart16550_get_next_byte(offset, CONFIG_UART16550_TX_BUFFER_SIZE);
}

static void uart16550_interrupt(int irq, void *data)
{
    struct uart16550_device *dev = data;

    RET_IF_FAIL(data,);

    irq_clear(dev->irq);

    uart16550_read8(dev, UART_IIR);

    while (uart16550_read8(dev, UART_LSR) & UART_LSR_DR) {
        char data = (char) uart16550_read8(dev, UART_RBR);

        if (semaphore_get_value(&dev->rx_semaphore) ==
            CONFIG_UART16550_RX_BUFFER_SIZE) // buffer is full
            continue;

        dev->rx_buffer[dev->rx_end] = data;
        dev->rx_end = (dev->rx_end + 1) % CONFIG_UART16550_RX_BUFFER_SIZE;
        semaphore_up(&dev->rx_semaphore);
    }

    if (uart16550_read8(dev, UART_LSR) & UART_LSR_THRE) {
        for (int i = 0; i < CONFIG_UART16550_FIFO_DEPTH &&
                        dev->tx_start != dev->tx_end; i++) {
            uart16550_write8(dev, UART_THR, dev->tx_buffer[dev->tx_start]);
            dev->tx_start = uart16550_get_next_tx_byte(dev->tx_start);
            semaphore_up(&dev->tx_semaphore);
        }
    }
}

static ssize_t uart16550_write(struct file *file, const void *buf, size_t count)
{
    struct device *device = devnum_get_device(file->inode->dev);
    struct uart16550_device *dev =
        containerof(device, struct uart16550_device, device);
    ssize_t nwrite = 0;
    const char *buffer = buf;

    mutex_lock(&dev->tx_mutex);

    while (nwrite < count) {
        if (dev->tx_start == uart16550_get_next_tx_byte(dev->tx_end)) {
            irq_pend(dev->irq);
            semaphore_down(&dev->tx_semaphore);
        }

        dev->tx_buffer[dev->tx_end] = buffer[nwrite++];
        dev->tx_end = uart16550_get_next_tx_byte(dev->tx_end);
    }

    irq_pend(dev->irq);

    mutex_unlock(&dev->tx_mutex);

    return nwrite;
}

static ssize_t uart16550_read(struct file *file, void *buf, size_t count)
{
    struct device *device = devnum_get_device(file->inode->dev);
    struct uart16550_device *dev =
        containerof(device, struct uart16550_device, device);
    char *buffer = buf;
    ssize_t nread = 0;

    mutex_lock(&dev->rx_mutex);
    semaphore_lock(&dev->rx_semaphore);

    do {
        buffer[nread++] = dev->rx_buffer[dev->rx_start];
        dev->rx_start = (dev->rx_start + 1) % CONFIG_UART16550_RX_BUFFER_SIZE;
    } while (--count && semaphore_trylock(&dev->rx_semaphore));

    mutex_unlock(&dev->rx_mutex);

    return nread;
}

static int uart16550_tcsetattr(struct uart16550_device *dev, int action,
                               struct termios *ios)
{
    uint8_t lcr = 0;
    uint16_t divisor;
    uint16_t baud;

    RET_IF_FAIL(dev, -EINVAL);
    RET_IF_FAIL(ios, -EINVAL);

    switch (ios->c_cflag & CSIZE) {
    case CS5:
        lcr |= UART_LCR_DLS_5BITS;
        break;

    case CS6:
        lcr |= UART_LCR_DLS_6BITS;
        break;

    case CS7:
        lcr |= UART_LCR_DLS_7BITS;
        break;

    case CS8:
        lcr |= UART_LCR_DLS_8BITS;
        break;

    default:
        return -EINVAL;
    }

    if (ios->c_cflag & CSTOPB) {
        lcr |= UART_LCR_STB_2;
    } else {
        lcr |= UART_LCR_STB_1;
    }

    if (ios->c_cflag & PARENB) {
        lcr |= UART_LCR_PEN;
    }

    if (!(ios->c_cflag & PARODD)) {
        lcr |= UART_LCR_EPS;
    }

    if (ios->c_cflag & CMSPAR) {
        lcr |= UART_LCR_STK;
    }

    switch (ios->c_cflag & CBAUD) {
#define BAUD(x) \
    case B##x: \
        baud = (uint16_t) x; \
        break

        BAUD(0);
        BAUD(50);
        BAUD(75);
        BAUD(110);
        BAUD(134);
        BAUD(150);
        BAUD(200);
        BAUD(300);
        BAUD(600);
        BAUD(1200);
        BAUD(1800);
        BAUD(2400);
        BAUD(4800);
        BAUD(9600);
        BAUD(19200);
        BAUD(38400);
        BAUD(57600);
        BAUD(115200);
        BAUD(230400);

    default:
        return -EINVAL;
        break;
    }

    if (baud != 0) {
        uart16550_write8(dev, UART_LCR, lcr | UART_LCR_DLAB);

        divisor = dev->clk / (baud * 16);
        write8(UART_DLL, (divisor >> 0) & 0xff);
        write8(UART_DLM, (divisor >> 8) & 0xff);
    }

    uart16550_write8(dev, UART_LCR, lcr);
    return 0;
}

static int uart16550_ioctl(struct file *file, unsigned long cmd, ...)
{
    va_list vl;
    struct device *device = devnum_get_device(file->inode->dev);
    struct uart16550_device *dev =
        containerof(device, struct uart16550_device, device);

    RET_IF_FAIL(file, -EINVAL);

    switch (cmd) {
    case TCSETS:
        va_start(vl, cmd);
        uart16550_tcsetattr(dev, TCSANOW, va_arg(vl, struct termios*));
        va_end(vl);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static struct file_operations uart16550_ops = {
    .ioctl = uart16550_ioctl,
    .read = uart16550_read,
    .write = uart16550_write,
};

static int uart16550_probe(struct device *device)
{
    struct uart16550_device *dev =
        containerof(device, struct uart16550_device, device);
    dev_t devnum;
    int retval;

    RET_IF_FAIL(device, -EINVAL);

    device->ops = uart16550_ops;

    retval = devnum_alloc(&uart16550_driver, device, &devnum);
    RET_IF_FAIL(!retval, retval);

    dev->rx_start = dev->rx_end = 0;
    dev->tx_start = dev->tx_end = 0;
    mutex_init(&dev->rx_mutex);
    mutex_init(&dev->tx_mutex);
    semaphore_init(&dev->rx_semaphore, 0);
    semaphore_init(&dev->tx_semaphore, 0);

    irq_attach(dev->irq, uart16550_interrupt, dev);
    irq_enable_line(dev->irq);
    uart16550_write8(dev, UART_FCR, UART_FCR_FIFOE | UART_FCR_RFIFOR |
                                    UART_FCR_XFIFOR | UART_FCR_RT_FULL_MINUS_2);
    uart16550_write8(dev, UART_IER, UART_IER_ERBFI | UART_IER_ETBEI);

    return tty_register(&dev->tty, devnum);
}

static int uart16550_remove(struct device *device)
{
    struct uart16550_device *dev =
        containerof(device, struct uart16550_device, device);

    RET_IF_FAIL(device, -EINVAL);

    irq_disable_line(dev->irq);
    irq_detach(dev->irq);

    return tty_unregister(&dev->tty);
}

__driver__ struct driver uart16550_driver = {
    .name = "uart16550",

    .probe = uart16550_probe,
    .remove = uart16550_remove,
};
