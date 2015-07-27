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

__driver__ struct driver stm32_usart_driver;

static inline uint32_t stm32_usart_read32(struct device *dev, unsigned int reg)
{
    RET_IF_FAIL(dev, 0);
    return read32((char*) dev->reg_base + reg);
}

static inline void stm32_usart_write32(struct device *dev, unsigned int reg,
                                       uint32_t value)
{
    RET_IF_FAIL(dev,);
    write32((char*) dev->reg_base + reg, value);
}

static unsigned uart16550_get_next_byte(unsigned offset, size_t size)
{
    return (offset + 1) % size;
}

static unsigned uart16550_get_next_tx_byte(unsigned offset)
{
    return uart16550_get_next_byte(offset, CONFIG_UART_TX_BUFFER_SIZE);
}

static void stm32_usart_interrupt(int irq, void *data)
{
    struct uart_device *dev = data;

    irq_clear(irq);

    while (stm32_usart_read32(&dev->device, STM32_USART1_SR) & STM32_USART_SR_RXNE) {
        char data = (char) stm32_usart_read32(&dev->device, STM32_USART1_DR);

        if (semaphore_get_value(&dev->rx_semaphore) ==
            CONFIG_UART_RX_BUFFER_SIZE) // buffer is full
            continue;

        dev->rx_buffer[dev->rx_end] = data;
        dev->rx_end = (dev->rx_end + 1) % CONFIG_UART_RX_BUFFER_SIZE;
        semaphore_up(&dev->rx_semaphore);
    }

    while (dev->tx_start != dev->tx_end && (stm32_usart_read32(&dev->device, STM32_USART1_SR) & STM32_USART1_SR_TXE) == STM32_USART1_SR_TXE) {
        stm32_usart_write32(&dev->device, STM32_USART1_DR,
                            dev->tx_buffer[dev->tx_start]);
        dev->tx_start = uart16550_get_next_tx_byte(dev->tx_start);
        semaphore_up(&dev->tx_semaphore);
    }

    if (dev->tx_start == dev->tx_end) {
        stm32_usart_write32(&dev->device, STM32_USART1_CR1,STM32_USART1_CR1_UE |
                            STM32_USART1_CR1_RXNEIE | STM32_USART1_CR1_TE |
                            STM32_USART1_CR1_RE);
    }

    stm32_usart_write32(&dev->device, STM32_USART1_SR, 0);
}

static ssize_t stm32_usart_write(struct file *file, const void *buf,
                                 size_t count)
{
    struct device *device = devnum_get_device(file->inode->dev);
    struct uart_device *dev = to_uart(device);
    ssize_t nwrite = 0;
    const char *buffer = buf;

    mutex_lock(&dev->tx_mutex);

    while (nwrite < count) {
        if (dev->tx_start == uart16550_get_next_tx_byte(dev->tx_end)) {
            irq_pend(device->irq);
            semaphore_down(&dev->tx_semaphore);
        }

        dev->tx_buffer[dev->tx_end] = buffer[nwrite++];
        dev->tx_end = uart16550_get_next_tx_byte(dev->tx_end);
    }

    stm32_usart_write32(&dev->device, STM32_USART1_CR1,STM32_USART1_CR1_UE |
                        STM32_USART1_CR1_RXNEIE | STM32_USART1_CR1_TXEIE |
                        STM32_USART1_CR1_TE | STM32_USART1_CR1_RE);

    mutex_unlock(&dev->tx_mutex);

    return nwrite;
}

static ssize_t stm32_usart_read(struct file *file, void *buf, size_t count)
{
    struct device *device = devnum_get_device(file->inode->dev);
    struct uart_device *dev = to_uart(device);
    char *buffer = buf;
    ssize_t nread = 0;

    mutex_lock(&dev->rx_mutex);
    semaphore_lock(&dev->rx_semaphore);

    do {
        buffer[nread++] = dev->rx_buffer[dev->rx_start];
        dev->rx_start = (dev->rx_start + 1) % CONFIG_UART_RX_BUFFER_SIZE;
    } while (--count && semaphore_trylock(&dev->rx_semaphore));

    mutex_unlock(&dev->rx_mutex);

    return nread;
}

static struct file_operations stm32_usart_ops = {
//    .ioctl = uart16550_ioctl,
    .read = stm32_usart_read,
    .write = stm32_usart_write,
};

static int stm32_usart_probe(struct device *device)
{
    dev_t devnum;
    int retval;
    struct uart_device *uart = to_uart(device);

    RET_IF_FAIL(device, -EINVAL);

    device->ops = stm32_usart_ops;

    retval = devnum_alloc(&stm32_usart_driver, device, &devnum);
    RET_IF_FAIL(!retval, retval);

    uart->rx_start = uart->rx_end = 0;
    uart->tx_start = uart->tx_end = 0;
    mutex_init(&uart->rx_mutex);
    mutex_init(&uart->tx_mutex);
    semaphore_init(&uart->rx_semaphore, 0);
    semaphore_init(&uart->tx_semaphore, 0);

    irq_attach(device->irq, stm32_usart_interrupt, uart);
    irq_enable_line(device->irq);

    stm32_usart_write32(device, STM32_USART1_CR1, STM32_USART1_CR1_UE |
                        STM32_USART1_CR1_RXNEIE | STM32_USART1_CR1_TE |
                        STM32_USART1_CR1_RE);

    return tty_register(&uart->tty, devnum);
}

static int stm32_usart_remove(struct device *device)
{
    struct uart_device *uart = to_uart(device);

    RET_IF_FAIL(device, -EINVAL);

    irq_disable_line(device->irq);
    irq_detach(device->irq);

    return tty_unregister(&uart->tty);
}

__driver__ struct driver stm32_usart_driver = {
    .name = "stm32-usart",

    .probe = stm32_usart_probe,
    .remove = stm32_usart_remove,
};
