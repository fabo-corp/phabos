/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

.global bootstrap
.global reset_handler
.extern _text_lma, _text_vma, _text_size
.extern _boot_text_lma, _boot_text_vma, _boot_text_size
.extern _eor
.extern _reset_handler
.syntax unified
.thumb

.section .text, "ax"
.align 4

reset_handler:
    ldr r13, =_eor
    b _reset_handler

.section .boot.text, "ax"
.align 4

.thumb_func
copy_to_ram:
    ldr r3, [r1], #4
    str r3, [r2], #4
    cbz r0, done_copy_to_ram
    sub r0, r0, #4
    b copy_to_ram
done_copy_to_ram:
    bx lr

.align 4
_copy_to_ram:

.section .boot
.thumb_func
bootstrap:
    push {r4, lr}

    ldr r0, =_boot_text_lma
    ldr r1, =_boot_text_size
    ldr r2, =_boot_text_vma

bootstrap_copy:

    cbz r1, done_bootstrap_copy
    ldr r3, [r0], #4
    str r3, [r2], #4
    sub r1, r1, #4
    b bootstrap_copy

done_bootstrap_copy:
    ldr r0, =_text_size
    add r0, r0, #3
    bic r0, r0, #3
    ldr r1, =_text_lma
    ldr r2, =_text_vma
    ldr r4, =copy_to_ram
    orr r4, r4, #1
    blx r4

    pop {r4, pc}
