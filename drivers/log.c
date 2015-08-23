#include <phabos/driver.h>
#include <phabos/kprintf.h>

#include <stdarg.h>

void dev_error(struct device *device, const char *format, ...)
{
    va_list vl;

    va_start(vl, format);

    kprintf("%s %s: \e[1;31m", device->driver, device->name);
    kvprintf(format, vl);
    kprintf("\e[m");

    va_end(vl);
}

void dev_debug(struct device *device, const char *format, ...)
{
    va_list vl;

    va_start(vl, format);

    kprintf("%s %s: \e[0;36m", device->driver, device->name);
    kvprintf(format, vl);
    kprintf("\e[m");

    va_end(vl);
}

void dev_warn(struct device *device, const char *format, ...)
{
    va_list vl;

    va_start(vl, format);

    kprintf("%s %s: \e[1;33m", device->driver, device->name);
    kvprintf(format, vl);
    kprintf("\e[m");

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
