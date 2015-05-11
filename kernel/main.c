/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <config.h>
#include <stdlib.h>
#include <phabos/kprintf.h>
#include <kernel/scheduler.h>

void shell_main(int argc, char **argv);

void main(void)
{
    scheduler_init();

    char* argv[] = {
        "shell_main",
        NULL
    };
    shell_main(1, argv);
}
