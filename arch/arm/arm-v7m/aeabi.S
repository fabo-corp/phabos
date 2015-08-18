/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

.global __aeabi_uidivmod, __aeabi_idivmod
.global __aeabi_memcpy
.global __aeabi_memclr4
.extern memcpy
.extern memset
.syntax unified

__aeabi_uidivmod:
.thumb_func
    udiv r2, r0, r1
    umull r3, r2, r2, r1
    sub r1, r0, r3
    mov r0, #0
    bx lr

__aeabi_idivmod:
.thumb_func
    sdiv r2, r0, r1
    smull r3, r2, r2, r1
    sub r1, r0, r3
    mov r0, #0
    bx lr

__aeabi_memcpy:
    b memcpy

__aeabi_memclr4:
    mov r2, r1
    mov r1, #0
    b memset
