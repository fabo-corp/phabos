#include <phabos/driver.h>
#include <phabos/fs.h>
#include <phabos/kprintf.h>

int chrdev_register(struct device *device, dev_t devnum, const char *const name,
                    struct file_operations *ops)
{
    struct driver *drv = driver_lookup(device->driver);

    device->ops = *ops;

    devfs_mknod(name, S_IFCHR, devnum);
    kprintf("%s: new device %s\n", drv->name, name);

    return 0;
}
