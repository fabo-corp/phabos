#ifndef __PHABOS_TTY_H__
#define __PHABOS_TTY_H__

#include <phabos/driver.h>
#include <sys/types.h>

struct tty_device {
    struct device device;
    unsigned int id;
};

int tty_register(struct tty_device *dev, dev_t devnum);
int tty_unregister(struct tty_device *dev);

#endif /* __PHABOS_TTY_H__ */

