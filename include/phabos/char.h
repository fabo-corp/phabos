#ifndef __PHABOS_CHAR_H__
#define __PHABOS_CHAR_H__

#include <phabos/driver.h>
#include <phabos/fs.h>

int chrdev_register(struct device *device, dev_t devnum, const char *name,
                    struct file_operations *ops);

#endif /* __PHABOS_CHAR_H__ */

