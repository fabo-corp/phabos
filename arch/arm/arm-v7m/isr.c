/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/machine.h>
#include <asm/hwio.h>
#include <asm/irq.h>
#include <asm/semihosting.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <config.h>

#define ARM_CM_NUM_EXCEPTION 16

#ifndef CONFIG_BOOT_COPYTORAM
#define __boot__
#else
#define __boot__ __attribute__((section(".boot"))) __attribute__((unused))
#endif

enum exception_handler {
    STACK = 0,
    RESET_HANDLER,
    NMI_HANDLER,
    HARD_FAULT_HANDLER,
    MEM_FAULT_HANDLER,
    BUS_FAULT_HANDLER,
    USAGE_FAULT_HANDLER,

    PENDSV_HANDLER = 14,
    SYSTICK_HANDLER = 15,
    IRQ0_HANDLER = 16,
    LAST_HANDLER = CPU_NUM_IRQ + ARM_CM_NUM_EXCEPTION - 1,
};

typedef void (*intr_handler_t)(void);

extern void _eor(void);
void reset_handler(void) __boot__;
void default_handler(void);
void main(void);

#define __vector__ __attribute__((section(".isr_vector")))
__vector__ intr_handler_t boot_vector[] = {
    [STACK] = _eor,
    [RESET_HANDLER] = reset_handler,
};

#define __vector_align__ __attribute__((aligned(VTOR_ALIGNMENT)))
__vector_align__ intr_handler_t intr_vector[LAST_HANDLER + 1] = {
    [STACK] = _eor,
    [RESET_HANDLER] = reset_handler,
    [RESET_HANDLER + 1 ... LAST_HANDLER] = default_handler,
};

static void clear_bss_section(void)
{
    extern uint32_t _bss;
    extern uint32_t _ebss;
    memset(&_bss, 0, (uint32_t)&_ebss - (uint32_t)&_bss);
}

static void copy_data_section(void)
{
    extern uint32_t _data_lma;
    extern uint32_t _data_vma;
    extern uint32_t _data_size;
    memcpy(&_data_vma, &_data_lma, (size_t)&_data_size);
}

static void move_isr(void)
{
#define ARM_SCB_VTOR 0xE000ED08
    write32(ARM_SCB_VTOR, &intr_vector);
}

__attribute__((weak)) void machine_init(void)
{
}

static void _start(void)
{
    clear_bss_section();
    copy_data_section();
    move_isr();

#ifdef CONFIG_ARM_SEMIHOSTING
    semihosting_init();
#endif

    irq_initialize();

    machine_init();

    main();

    while(1)
        asm volatile("nop");
}

__boot__ void reset_handler(void)
{
#ifdef CONFIG_BOOT_COPYTORAM
    extern void bootstrap(void);
    bootstrap();
#endif
    asm volatile("mov r13, %0" ::"r"(_eor));
    _start();
}

void default_handler(void)
{
    int psr;
    asm volatile("mrs %0, xpsr" : "=r"(psr));
    printf("unhandled interrupt: %d\n", psr & 0xFF);
    while (1)
        asm volatile("nop");
}
