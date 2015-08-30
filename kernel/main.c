/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <config.h>

#include <phabos/kprintf.h>
#include <phabos/scheduler.h>
#include <phabos/syscall.h>
#include <phabos/driver.h>
#include <phabos/task.h>
#include <phabos/panic.h>

#include <errno.h>
#include <string.h>

int CONFIG_INIT_TASK_NAME(int argc, char **argv);

#define xstr(s) str(s)
#define str(s) #s

static void rootfs_init(void)
{
    int retval;

    retval = mount(NULL, NULL, "ramfs", 0, NULL);
    if (retval < 0) {
        kprintf("failed to mount the ramfs: %s\n", strerror(errno));
        panic("Cannot initialize kernel\n");
    }

    retval = mkdir("/dev", 0);
    if (retval) {
        kprintf("mkdir: %s\n", strerror(errno));
        panic("Cannot initialize kernel\n");
    }

    retval = mount(NULL, "/dev", "devfs", 0, NULL);
    if (retval < 0) {
        kprintf("failed to mount devfs: %s\n", strerror(errno));
        panic("Cannot initialize kernel\n");
    }

#ifdef CONFIG_PROCFS
    retval = mkdir("/proc", 0);
    if (retval) {
        kprintf("mkdir: %s\n", strerror(errno));
        kprintf("cannot create /proc/\n");
    } else {
        retval = mount(NULL, "/proc", "procfs", 0, NULL);
        if (retval < 0)
            kprintf("failed to mount procfs: %s\n", strerror(errno));
    }
#endif
}

static void open_std_fds(void)
{
    open("/dev/ttyS0", 0);
    open("/dev/ttyS0", 0);
    open("/dev/ttyS0", 0);
}

static void init(void *data)
{
    char* argv[] = {
        xstr(CONFIG_INIT_TASK_NAME),
        NULL
    };

    syscall_init();
    devfs_init();
    fs_init();
    driver_init();
    rootfs_init();

    device_driver_probe_all();
    open_std_fds();

    CONFIG_INIT_TASK_NAME(1, argv);

    while (1);
}

static void clear_screen(void)
{
    kprintf("\r%c[2J",27);
}

void main(void)
{
    clear_screen();
    kprintf("booting phabos...\n");

    task_init();
    sched_init();

    task_run("init", init, NULL, 0);
}
