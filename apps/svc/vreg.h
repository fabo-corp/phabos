/*
 * Copyright (c) 2015 Google Inc.
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

/**
 * @brief: Manage a voltage regulator
 */

#ifndef  _VREG_H_
#define  _VREG_H_

#include <asm/atomic.h>

#include <errno.h>
#include <stdbool.h>


/* Regulator control GPIO */
struct vreg_data {
    unsigned int gpio;        // GPIO number, conforming to GPIO Chip
    unsigned int hold_time;   // Assertion duration, in us
    unsigned int active_high; // Active-high to assert
    unsigned int def_val;     // Default value at init
};

/* Voltage regulator management struct */
struct vreg {
    const char *name;
    struct vreg_data *vregs;
    size_t nr_vregs;
    bool power_state;
    atomic_t use_count;
};

int vreg_config(struct vreg *);
int vreg_get(struct vreg *);
int vreg_put(struct vreg *);
bool vreg_get_pwr_state(struct vreg *);

/*
 * Macro magic.
 */

/* Helper for the generic active high regulator */
#define INIT_ACTIVE_HIGH_VREG_DATA(g, t)                       \
    {                                                          \
        .gpio = g,                                             \
        .hold_time = t,                                        \
        .active_high = 1,                                      \
        .def_val = 0,                                          \
    }

/* Helper for the generic active low regulator */
#define INIT_ACTIVE_LOW_VREG_DATA(g, t)                        \
    {                                                          \
        .gpio = g,                                             \
        .hold_time = t,                                        \
        .active_high = 0,                                      \
        .def_val = 1,                                          \
    }

/* vreg build helper */
#define __MAKE_VREG(n) n ## _vreg
#define MAKE_VREG(n) __MAKE_VREG(n)
#define DECLARE_VREG(_name, vreg_data)                         \
    static struct vreg MAKE_VREG(_name) = {                    \
        .name = #_name,                                        \
        .vregs = vreg_data,                                    \
        .nr_vregs = ARRAY_SIZE(vreg_data),                     \
        .use_count = 0,                                        \
    };

#endif
