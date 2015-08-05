#include <phabos/driver.h>
#include <phabos/kprintf.h>

#include <stdarg.h>

void dev_error(struct device *device, const char *format, ...)
{
    va_list vl;

    va_start(vl, format);

    kprintf("%s %s: ", device->driver, device->name);
    kvprintf(format, vl);

    va_end(vl);
}

void dev_debug(struct device *device, const char *format, ...)
{
    va_list vl;

    va_start(vl, format);

    kprintf("%s %s: ", device->driver, device->name);
    kvprintf(format, vl);

    va_end(vl);
}

void dev_info(struct device *device, const char *format, ...)
{
    va_list vl;

    va_start(vl, format);

    kprintf("%s %s: ", device->driver, device->name);
    kvprintf(format, vl);

    va_end(vl);
}
