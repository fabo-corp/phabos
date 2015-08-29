#include <asm/spinlock.h>
#include <phabos/driver.h>
#include <phabos/hashtable.h>
#include <phabos/serial/tty.h>
#include <phabos/char.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#define DRIVER_NAME "tty"

#define FILE_OP_READ_ASSERT(file, buffer)   \
    RET_IF_FAIL(file, -EINVAL);             \
    RET_IF_FAIL(file->inode, -EINVAL);      \
    RET_IF_FAIL(buffer, -EINVAL)

enum {
    ETX = 0x3,
};

__driver__ struct driver tty_driver;

static ssize_t tty_read(struct file *file, void *buffer, size_t len)
{
    FILE_OP_READ_ASSERT(file, buffer);

    struct device *device = devnum_get_device(file->inode->dev);
    struct tty_device *tty = containerof(device, struct tty_device, device);

    RET_IF_FAIL(device, -EINVAL);
    RET_IF_FAIL(tty->ops, -EINVAL);

    if (!tty->ops->read)
        return -ENOSYS;

    return tty->ops->read(tty, buffer, len);
}

static ssize_t tty_write(struct file *file, const void *buffer, size_t len)
{
    FILE_OP_READ_ASSERT(file, buffer);

    struct device *device = devnum_get_device(file->inode->dev);
    struct tty_device *tty = containerof(device, struct tty_device, device);

    RET_IF_FAIL(device, -EINVAL);
    RET_IF_FAIL(tty->ops, -EINVAL);

    if (!tty->ops->write)
        return -ENOSYS;

    return tty->ops->write(tty, buffer, len);
}

static int tty_ioctl(struct file *file, unsigned long cmd, va_list vl)
{
    RET_IF_FAIL(file, -EINVAL);
    RET_IF_FAIL(file->inode, -EINVAL);
    RET_IF_FAIL(file->inode->dev, -EINVAL);

    struct device *device = devnum_get_device(file->inode->dev);
    struct tty_device *tty = containerof(device, struct tty_device, device);

    RET_IF_FAIL(device, -EINVAL);
    RET_IF_FAIL(tty->ops, -EINVAL);

    switch (cmd) {
    case TCSETS:
        if (!tty->ops->tcsetattr)
            return -ENOSYS;
        return tty->ops->tcsetattr(tty, TCSANOW, va_arg(vl, struct termios*));

    case TCSETSW:
        if (!tty->ops->tcsetattr)
            return -ENOSYS;
        return tty->ops->tcsetattr(tty, TCSADRAIN, va_arg(vl, struct termios*));

    case TCSETSF:
        if (!tty->ops->tcsetattr)
            return -ENOSYS;
        return tty->ops->tcsetattr(tty, TCSAFLUSH, va_arg(vl, struct termios*));

    case TCGETS:
        if (!tty->ops->tcgetattr)
            return -ENOSYS;
        return tty->ops->tcgetattr(tty, va_arg(vl, struct termios*));

    default:
        return -EINVAL;
    }

    return 0;
}

static struct file_operations tty_operations = {
    .ioctl = tty_ioctl,
    .read = tty_read,
    .write = tty_write,
};

int tty_register(struct tty_device *tty, struct tty_ops *ops)
{
    int retval;
    char *name;
    dev_t devnum;

    RET_IF_FAIL(tty, -EINVAL);

    retval = devnum_alloc(&tty_driver, &tty->device, &devnum);
    if (retval)
        return retval;

    tty->id = minor(devnum);
    tty->ops = ops;

    tty->rx_start = tty->rx_end = 0;
    tty->tx_start = tty->tx_end = 0;
    mutex_init(&tty->rx_mutex);
    mutex_init(&tty->tx_mutex);
    semaphore_init(&tty->rx_semaphore, 0);
    semaphore_init(&tty->tx_semaphore, 0);

    retval = asprintf(&name, "ttyS%u", tty->id);
    if (retval < 0)
        return -ENOMEM;

    retval = chrdev_register(&tty->device, devnum, name, &tty_operations);
    if (retval)
        goto error_chdev_register;

    free(name);
    return 0;

error_chdev_register:
    free(name);
    return retval;
}

int tty_unregister(struct tty_device *dev)
{
    RET_IF_FAIL(dev, -EINVAL);

    return 0;
}

int tty_init(struct driver *driver)
{
    return 0;
}

__driver__ struct driver tty_driver = {
    .name = DRIVER_NAME,

    .init = tty_init,
};
