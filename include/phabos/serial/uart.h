#ifndef __UART_H__
#define __UART_H__

#include <config.h>
#include <phabos/mutex.h>
#include <phabos/semaphore.h>
#include <phabos/serial/tty.h>
#include <phabos/utils.h>

#define CONFIG_UART_RX_BUFFER_SIZE 60
#define CONFIG_UART_TX_BUFFER_SIZE 60

struct uart_device {
    struct device device;
    struct tty_device tty;

    char rx_buffer[CONFIG_UART_RX_BUFFER_SIZE];
    char tx_buffer[CONFIG_UART_TX_BUFFER_SIZE];

    unsigned rx_start;
    unsigned rx_end;

    unsigned tx_start;
    unsigned tx_end;

    struct mutex rx_mutex;
    struct mutex tx_mutex;
    struct semaphore rx_semaphore;
    struct semaphore tx_semaphore;
};

static inline struct uart_device *to_uart(struct device *device)
{
    return containerof(device, struct uart_device, device);
}

#endif /* __UART_H__ */

