/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

.syntax unified
.thumb

.global atomic_add, atomic_inc, atomic_dec

.thumb_func
atomic_add:
    mov r2, r0
atomic_add_retry:
    ldrex r0, [r2]
    add r0, r0, r1
    strex r3, r0, [r2]
    cmp r3, #1
    beq atomic_add_retry
    dsb
    bx lr

.thumb_func
atomic_inc:
    mov r1, #1
    b atomic_add

.thumb_func
atomic_dec:
    mov r1, #-1
    b atomic_add
