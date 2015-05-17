/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

.global syscall
.global _svcall_handler
.extern syscall_find
.syntax unified
.thumb

#define USER_THREAD_BIT 0

.thumb_func
syscall_handler:
    push {r8, lr}
    mov r8, r0
    mov r0, r7

    push {r1 - r3}
    bl syscall_find
    pop {r1 - r3}

    ldr r12, [r0, #8]
    mov r0, r8

    push {r4 - r6}
    blx r12
    pop {r4 - r6}

    pop {r8, lr}

    orr r12, lr, #1
    pop {r1 - r2, lr}
    msr xpsr, r1
    msr control, r2

    ands r1, r1, #(1 << 9)
    it ne
    addne sp, sp, #4 // FIXME: is it here the best place to do it?

    bx r12

.thumb_func
_svcall_handler:
    mrs r0, control
    mov r1, r0
    bfc r0, #USER_THREAD_BIT, #1
    msr control, r0

    mov r12, sp
    sub sp, sp, #12
    mov r0, sp
    push {r4 - r9}

    ldm r12, {r2 - r9}
    stm r0, {r2 - r9}

    str r1, [r0, #36]

    ldr r1, [r0, #20]
    str r1, [r0, #40]

    ldr r1, [r0, #28]
    str r1, [r0, #32]

    ldr r1, [r0, #24]
    str r1, [r0, #20]

    ldr r1, =syscall_handler
    str r1, [r0, #24]

    pop {r4 - r9}
    bx lr

.thumb_func
syscall:
    push {r4 - r7}

    cmp r0, #0
    it le
    movle r0, #-1
    ble syscall_exit

    push {r1 - r3, lr}
    bl syscall_find
    pop {r1 - r3, lr}

    cmp r0, #0
    it le
    movle r0, #-1
    ble syscall_exit

    mov r12, r0
    mov r0, r1
    mov r1, r2
    mov r2, r3

    add r3, sp, #28

    ldr r7, [r12, #4]
    cmp r7, #4
    blt syscall_execute

    sub r7, r7, #4
    tbb [pc, r7]
syscall_params:
    .byte (syscall_param_4 - syscall_params) / 2
    .byte (syscall_param_5 - syscall_params) / 2
    .byte (syscall_param_6 - syscall_params) / 2
    .byte (syscall_param_7 - syscall_params) / 2
syscall_param_7:
    ldr r6, [r3], #-4
syscall_param_6:
    ldr r5, [r3], #-4
syscall_param_5:
    ldr r4, [r3], #-4
syscall_param_4:
    ldr r3, [r3]

syscall_execute:
    ldr r7, [r12, #0]
    svc #0

syscall_exit:
    pop {r4 - r7}
    bx lr
