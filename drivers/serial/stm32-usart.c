#include <config.h>
#include <asm/hwio.h>
#include <asm/irq.h>
#include <phabos/driver.h>
#include <phabos/assert.h>
#include <phabos/utils.h>
#include <phabos/termios.h>
#include <phabos/serial/uart.h>

#include <errno.h>
#include <stdarg.h>

#define STM32_USART1_SR         0x00
#define STM32_USART1_DR         0x04
#define STM32_USART1_CR1        0x0c

#define STM32_USART1_CR1_RE     (1 << 2)
#define STM32_USART1_CR1_TE     (1 << 3)
#define STM32_USART1_CR1_RXNEIE (1 << 5)
#define STM32_USART1_CR1_TXEIE  (1 << 7)
#define STM32_USART1_CR1_UE     (1 << 13)

#define STM32_USART_SR_RXNE     (1 << 5)
#define STM32_USART1_SR_TXE     (1 << 7)

static inline uint32_t stm32_usart_read32(struct tty_device *tty,
                                          unsigned int reg)
{
    RET_IF_FAIL(tty, 0);
    return read32(tty->device.reg_base + reg);
}

static inline void stm32_usart_write32(struct tty_device *tty, unsigned int reg,
                                       uint32_t value)
{
    RET_IF_FAIL(tty,);
    write32(tty->device.reg_base + reg, value);
}

static void stm32_usart_interrupt(int irq, void *data)
{
    struct tty_device *tty = data;

    RET_IF_FAIL(data,);

    irq_clear(tty->device.irq);

    while (stm32_usart_read32(tty, STM32_USART1_SR) & STM32_USART_SR_RXNE) {
        char data = (char) stm32_usart_read32(tty, STM32_USART1_DR);

        if (semaphore_get_value(&tty->rx_semaphore) == TTY_MAX_INPUT)
            continue;

        tty->rx_buffer[tty->rx_end] = data;
        tty->rx_end = (tty->rx_end + 1) % TTY_MAX_INPUT;
        semaphore_up(&tty->rx_semaphore);
    }

    while (tty->tx_start != tty->tx_end &&
           (stm32_usart_read32(tty, STM32_USART1_SR) & STM32_USART1_SR_TXE) ==
                STM32_USART1_SR_TXE) {
        stm32_usart_write32(tty, STM32_USART1_DR,
                            tty->tx_buffer[tty->tx_start++]);
        if (tty->tx_start >= TTY_MAX_OUTPUT)
            tty->tx_start = 0;
        semaphore_up(&tty->tx_semaphore);
    }

    if (tty->tx_start == tty->tx_end) {
        stm32_usart_write32(tty, STM32_USART1_CR1,STM32_USART1_CR1_UE |
                                 STM32_USART1_CR1_RXNEIE | STM32_USART1_CR1_TE |
                                 STM32_USART1_CR1_RE);
    }

    stm32_usart_write32(tty, STM32_USART1_SR, 0);
}

static void stm32_usart_interrupt_enable(struct tty_device *tty)
{
    stm32_usart_write32(tty, STM32_USART1_CR1, STM32_USART1_CR1_UE |
                             STM32_USART1_CR1_RXNEIE | STM32_USART1_CR1_TXEIE |
                             STM32_USART1_CR1_TE | STM32_USART1_CR1_RE);
}

static void stm32_usart_interrupt_disable(struct tty_device *tty)
{
    stm32_usart_write32(tty, STM32_USART1_CR1, STM32_USART1_CR1_UE |
                             STM32_USART1_CR1_TE | STM32_USART1_CR1_RE);
}

static ssize_t stm32_usart_write(struct tty_device *tty, const char *buffer,
                                 size_t count)
{
    ssize_t nwrite = 0;

    mutex_lock(&tty->tx_mutex);

    while (nwrite < count) {
        unsigned next_pos = (tty->tx_end + 1) % TTY_MAX_OUTPUT;
        while (tty->tx_start == next_pos) {
            irq_pend(tty->device.irq);
            stm32_usart_interrupt_enable(tty);
            semaphore_down(&tty->tx_semaphore);
            stm32_usart_interrupt_disable(tty);
        }

        tty->tx_buffer[tty->tx_end++] = buffer[nwrite++];
        if (tty->tx_end >= TTY_MAX_OUTPUT)
            tty->tx_end = 0;
    }

    stm32_usart_interrupt_enable(tty);

    mutex_unlock(&tty->tx_mutex);

    return nwrite;
}

static ssize_t stm32_usart_read(struct tty_device *tty, char *buffer,
                                size_t len)
{
    ssize_t nread = 0;

    mutex_lock(&tty->rx_mutex);
    semaphore_down(&tty->rx_semaphore);

    do {
        buffer[nread++] = tty->rx_buffer[tty->rx_start++];
        if (tty->rx_start >= TTY_MAX_INPUT)
            tty->rx_start = 0;
    } while (nread < len && semaphore_trydown(&tty->rx_semaphore));

    mutex_unlock(&tty->rx_mutex);

    return nread;
}

static struct tty_ops stm32_usart_ops = {
    .read = stm32_usart_read,
    .write = stm32_usart_write,
};

static int stm32_usart_probe(struct device *device)
{
    struct tty_device *tty = to_tty(device);
    int retval;

    RET_IF_FAIL(device, -EINVAL);

    retval = tty_register(tty, &stm32_usart_ops);
    if (retval)
        return retval;

    irq_attach(device->irq, stm32_usart_interrupt, tty);
    irq_enable_line(device->irq);

    stm32_usart_write32(tty, STM32_USART1_CR1, STM32_USART1_CR1_UE |
                             STM32_USART1_CR1_RXNEIE | STM32_USART1_CR1_TE |
                             STM32_USART1_CR1_RE);

    return 0;
}

static int stm32_usart_remove(struct device *device)
{
    RET_IF_FAIL(device, -EINVAL);

    irq_disable_line(device->irq);
    irq_detach(device->irq);

    return tty_unregister(to_tty(device));
}

__driver__ struct driver stm32_usart_driver = {
    .name = "stm32-usart",

    .probe = stm32_usart_probe,
    .remove = stm32_usart_remove,
};
