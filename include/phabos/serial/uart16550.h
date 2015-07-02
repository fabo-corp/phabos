#ifndef __UART16550_H__
#define __UART16550_H__

#include <config.h>
#include <phabos/mutex.h>
#include <phabos/semaphore.h>
#include <phabos/serial/tty.h>

struct uart16550_device {
    struct device device;
    struct tty_device tty;

#ifdef CONFIG_UART16550
    char rx_buffer[CONFIG_UART16550_RX_BUFFER_SIZE];
    char tx_buffer[CONFIG_UART16550_TX_BUFFER_SIZE];
#endif

    unsigned rx_start;
    unsigned rx_end;

    unsigned tx_start;
    unsigned tx_end;

    struct mutex rx_mutex;
    struct mutex tx_mutex;
    struct semaphore rx_semaphore;
    struct semaphore tx_semaphore;

    void *base;
    unsigned long clk;
    int irq;
};

#endif /* __UART16550_H__ */

