/*
 * Copyright (c) 2014 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <apps/shell.h>

#define DBG_COMP DBG_DBG
#include "../up_debug.h"

static int svc_debug_main(int argc, char *argv[])
{
    uint32_t comp, level;

    switch (argc) {
    case 1:
        /* Get the debug parameters */
        dbg_get_config(&comp, &level);
        printk("%s(): got comp=0x%x, level=%u\n", __func__, comp, level);
        break;
    case 3:
        /* Set the debug parameters */
        comp = strtol(argv[1], NULL, 16);
        level = strtol(argv[2], NULL, 10);
        dbg_set_config(comp, level);
        printk("%s(): set comp=0x%x, level=%u\n", __func__, comp, level);
        break;
    default:
        printk("%s(): Usage: debug [comp_bit_mask level]\n", __func__);
        return -EINVAL;
    }

    return 0;
}

__shell_command__ struct shell_command svc_debug_command = {
    .name = "debug",
    .description = "",
    .entry = svc_debug_main,
};
