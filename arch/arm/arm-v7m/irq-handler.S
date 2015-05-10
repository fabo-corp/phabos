/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

.syntax unified
.thumb

.extern hardfault_handler
.extern pendsv_handler
.extern systick_handler
.extern _impure_ptr

.global _pendsv_handler
.global _systick_handler
.global _hardfault_handler

.macro SAVE_CONTEXT
    push {lr}
    mrs r0, basepri
    mrs r1, control
    push {r0 - r1}
    push {r4 - r11}
    ldr r0, =_impure_ptr
    ldr r0, [r0]
    push {r0}

    mov r0, r13
    push {r0}
.endm

.macro RESTORE_CONTEXT
    pop {r1}
    ldr r0, =_impure_ptr
    str r1, [r0]
    pop {r4 - r11}
    pop {r0 - r1}
    msr control, r1
    msr basepri, r0
    pop {pc}
.endm

.thumb_func
_pendsv_handler:
    SAVE_CONTEXT
    mov r0, r13
    mov r1, #1
    bl pendsv_handler
    mov r13, r0
    RESTORE_CONTEXT

.thumb_func
_systick_handler:
    SAVE_CONTEXT
    mov r0, r13
    mov r1, #0
    bl systick_handler
    mov r13, r0
    RESTORE_CONTEXT

.thumb_func
_hardfault_handler:
    SAVE_CONTEXT
    mov r0, r13
    bl hardfault_handler
    RESTORE_CONTEXT
