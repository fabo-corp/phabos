#include <config.h>
#include <asm/hwio.h>
#include <asm/irq.h>
#include <phabos/driver.h>
#include <phabos/assert.h>
#include <phabos/utils.h>
#include <phabos/serial/uart16550.h>
#include <phabos/serial/tty.h>
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

static inline uint8_t uart16550_read8(struct tty_device *tty, unsigned int reg)
{
    RET_IF_FAIL(tty, 0);
    return read8(tty->device.reg_base + reg);
}

static inline void uart16550_write8(struct tty_device *tty, unsigned int reg,
                                    uint8_t value)
{
    RET_IF_FAIL(tty,);
    write8(tty->device.reg_base + reg, value);
}

static void uart16550_interrupt(int irq, void *data)
{
    struct tty_device *tty = data;

    RET_IF_FAIL(data,);

    irq_clear(tty->device.irq);

    uart16550_read8(tty, UART_IIR);

    while (uart16550_read8(tty, UART_LSR) & UART_LSR_DR)
        tty_push_to_input_queue(tty, (char) uart16550_read8(tty, UART_RBR));

    if (uart16550_read8(tty, UART_LSR) & UART_LSR_THRE) {
        for (int i = 0; i < CONFIG_UART16550_FIFO_DEPTH &&
                        tty->tx_start != tty->tx_end; i++) {
            uart16550_write8(tty, UART_THR, tty->tx_buffer[tty->tx_start++]);
            if (tty->tx_start >= TTY_MAX_OUTPUT)
                tty->tx_start = 0;
            semaphore_up(&tty->tx_semaphore);
        }
    }
}

static ssize_t uart16550_write(struct tty_device *tty, const char *buffer,
                               size_t len)
{
    ssize_t nwrite = 0;

    mutex_lock(&tty->tx_mutex);

    while (nwrite < len) {
        unsigned next_pos = (tty->tx_end + 1) % TTY_MAX_OUTPUT;
        while (tty->tx_start == next_pos) {
            irq_pend(tty->device.irq);
            semaphore_down(&tty->tx_semaphore);
        }

        tty->tx_buffer[tty->tx_end++] = buffer[nwrite++];
        if (tty->tx_end >= TTY_MAX_OUTPUT)
            tty->tx_end = 0;
    }

    irq_pend(tty->device.irq);

    mutex_unlock(&tty->tx_mutex);

    return nwrite;
}

static int uart16550_tcsetattr(struct tty_device *tty, int action,
                               const struct termios *ios)
{
    struct uart16550_pdata *pdata = tty->device.pdata;
    uint8_t lcr = 0;
    uint16_t divisor;
    uint16_t baud;

    RET_IF_FAIL(tty, -EINVAL);
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

    if (ios->c_cflag & CSTOPB)
        lcr |= UART_LCR_STB_2;
    else
        lcr |= UART_LCR_STB_1;

    if (ios->c_cflag & PARENB)
        lcr |= UART_LCR_PEN;

    if (!(ios->c_cflag & PARODD))
        lcr |= UART_LCR_EPS;

    if (ios->c_cflag & CMSPAR)
        lcr |= UART_LCR_STK;

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
        uart16550_write8(tty, UART_LCR, lcr | UART_LCR_DLAB);

        divisor = pdata->clk / (baud * 16);
        write8(UART_DLL, (divisor >> 0) & 0xff);
        write8(UART_DLM, (divisor >> 8) & 0xff);
    }

    uart16550_write8(tty, UART_LCR, lcr);
    return 0;
}

static struct tty_ops uart16550_ops = {
    .write = uart16550_write,

    .tcsetattr = uart16550_tcsetattr,
};

static int uart16550_probe(struct device *device)
{
    struct tty_device *tty = to_tty(device);
    int retval;

    RET_IF_FAIL(device, -EINVAL);

    retval = tty_register(tty, &uart16550_ops);
    if (retval)
        return retval;

    irq_attach(device->irq, uart16550_interrupt, tty);
    irq_enable_line(device->irq);

    uart16550_write8(tty, UART_FCR, UART_FCR_FIFOE | UART_FCR_RFIFOR |
                          UART_FCR_XFIFOR | UART_FCR_RT_FULL_MINUS_2);
    uart16550_write8(tty, UART_IER, UART_IER_ERBFI | UART_IER_ETBEI);

    return 0;
}

static int uart16550_remove(struct device *device)
{
    RET_IF_FAIL(device, -EINVAL);

    irq_disable_line(device->irq);
    irq_detach(device->irq);

    return tty_unregister(to_tty(device));
}

__driver__ struct driver uart16550_driver = {
    .name = "uart16550",

    .probe = uart16550_probe,
    .remove = uart16550_remove,
};
