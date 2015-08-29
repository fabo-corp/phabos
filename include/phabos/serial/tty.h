#ifndef __PHABOS_TTY_H__
#define __PHABOS_TTY_H__

#include <config.h>
#include <phabos/driver.h>
#include <phabos/termios.h>
#include <sys/types.h>

#define TTY_MAX_INPUT   CONFIG_TTY_MAX_INPUT
#define TTY_MAX_OUTPUT  CONFIG_TTY_MAX_OUTPUT
#define TTY_MAX_CANON   CONFIG_TTY_MAX_CANON

#if TTY_MAX_INPUT < TTY_MAX_CANON
#   error CONFIG_TTY_MAX_INPUT must be at least equal to CONFIG_TTY_MAX_CANON
#endif

struct tty_ops;

struct tty_device {
    struct device device;

    unsigned int id;
    struct tty_ops *ops;

    char rx_buffer[TTY_MAX_INPUT];
    char tx_buffer[TTY_MAX_OUTPUT];

    unsigned rx_start;
    unsigned rx_end;

    unsigned tx_start;
    unsigned tx_end;

    struct mutex rx_mutex;
    struct mutex tx_mutex;

    struct semaphore rx_semaphore;
    struct semaphore tx_semaphore;
};

struct tty_ops {
    ssize_t (*read)(struct tty_device *tty, char *buffer, size_t len);
    ssize_t (*write)(struct tty_device *tty, const char *buffer, size_t len);

    int (*tcdrain)(struct tty_device *tty);
    int (*tcflow)(struct tty_device *tty, int action);
    int (*tcflush)(struct tty_device *tty, int queue_selector);
    int (*tcgetattr)(struct tty_device *tty, struct termios *termios);
    int (*tcsendbreak)(struct tty_device *tty, int duration);
    int (*tcsetattr)(struct tty_device *tty, int optional_actions,
                     const struct termios *termios);
};

int tty_register(struct tty_device *dev, struct tty_ops *ops);
int tty_unregister(struct tty_device *dev);

static inline struct tty_device *to_tty(struct device *device)
{
    return containerof(device, struct tty_device, device);
}

#endif /* __PHABOS_TTY_H__ */

