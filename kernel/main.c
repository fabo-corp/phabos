/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <config.h>
#include <stdlib.h>
#include <phabos/kprintf.h>
#include <phabos/scheduler.h>

void shell_main(int argc, char **argv);

void init(void *data)
{
    char* argv[] = {
        "shell_main",
        NULL
    };
    shell_main(1, argv);

    while (1);
}

void main(void)
{
    kprintf("booting phabos...\n");

    scheduler_init();
    task_run(init, NULL, 0);
}
