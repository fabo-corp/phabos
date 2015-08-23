#include <phabos/driver.h>
#include <phabos/kprintf.h>
#include <phabos/hashtable.h>

#include <stdarg.h>

static struct hashtable debug_map;

void dev_log_init(void)
{
    hashtable_init_string(&debug_map);
}

void dev_debug_add_name(const char *name)
{
    hashtable_add(&debug_map, (void*) name, (void*) 1);
}

void dev_debug_add(struct device *device)
{
    dev_debug_add_name(device->name);
}

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

    if (!hashtable_has(&debug_map, (void*) device->driver))
        return;

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
