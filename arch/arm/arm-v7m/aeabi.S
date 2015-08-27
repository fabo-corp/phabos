/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

.global __aeabi_memclr4
.extern memset
.syntax unified

__aeabi_memclr4:
    mov r2, r1
    mov r1, #0
    b memset
