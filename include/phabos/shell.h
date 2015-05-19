/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __SHELL_H__
#define __SHELL_H__

#define __shell_command__ __attribute__((section(".shell_cmd")))

struct shell_command {
    const char *const name;
    const char *const description;
    int (*entry)(int argc, char **argv);
};

#endif /* __SHELL_H__ */

