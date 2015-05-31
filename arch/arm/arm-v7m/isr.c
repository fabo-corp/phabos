/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/machine.h>
#include <asm/hwio.h>
#include <asm/irq.h>
#include <asm/scheduler.h>
#include <asm/mpu.h>

#include <phabos/kprintf.h>
#include <phabos/panic.h>
#include <phabos/scheduler.h>

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

    SVCALL_HANDLER = 11,
    PENDSV_HANDLER = 14,
    SYSTICK_HANDLER = 15,
    IRQ0_HANDLER = 16,
    LAST_HANDLER = CPU_NUM_IRQ + ARM_CM_NUM_EXCEPTION - 1,
};

typedef void (*intr_handler_t)(void);

extern void _eor(void);
void reset_handler(void) __boot__;
void hardfault_handler(uint32_t *data);
static void irq_common_isr(void);
void main(void);
void _pendsv_handler(void);
void _systick_handler(void);
void _hardfault_handler(void);
void _memfault_handler(void);
void _svcall_handler(void);
void default_irq_handler(int irq, void *data);
void analyze_status_registers(void);

#define __vector__ __attribute__((section(".isr_vector")))
__vector__ intr_handler_t boot_vector[] = {
    [STACK] = _eor,
    [RESET_HANDLER] = reset_handler,
};

#define __vector_align__ __attribute__((aligned(VTOR_ALIGNMENT)))
__vector_align__ intr_handler_t intr_vector[LAST_HANDLER + 1] = {
    [STACK] = _eor,
    [RESET_HANDLER] = reset_handler,
    [NMI_HANDLER] = irq_common_isr,
    [HARD_FAULT_HANDLER] = _hardfault_handler,
#ifdef CONFIG_MPU
    [MEM_FAULT_HANDLER] = _memfault_handler,
#else
    [MEM_FAULT_HANDLER] = irq_common_isr,
#endif
    [BUS_FAULT_HANDLER ... SVCALL_HANDLER - 1] = irq_common_isr,
    [SVCALL_HANDLER] = _svcall_handler,
    [SVCALL_HANDLER + 1 ... PENDSV_HANDLER -1] = irq_common_isr,
    [PENDSV_HANDLER] = _pendsv_handler,
    [SYSTICK_HANDLER] = _systick_handler,
    [IRQ0_HANDLER... LAST_HANDLER] = irq_common_isr,
};

struct irq_handler irq_vector[LAST_HANDLER + 1] = {
    [RESET_HANDLER... LAST_HANDLER] = {default_irq_handler, NULL},
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

#ifdef CONFIG_MPU
    mpu_init();
#endif

    irq_initialize();

    machine_init();

#ifdef CONFIG_MPU
    mpu_enable();
#endif

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

static void dump_context(uint32_t *context)
{
    kprintf("R0  = %#.8X\tR1  = %#.8X\tR2  = %#.8X\tR3  = %#.8X\n"
            "R4  = %#.8X\tR5  = %#.8X\tR6  = %#.8X\tR7  = %#.8X\n"
            "R8  = %#.8X\tR9  = %#.8X\tR10 = %#.8X\tR11 = %#.8X\n"
            "R12 = %#.8X\tSP  = %#.8X\tLR  = %#.8X\tPC  = %#.8X\n"
            "PSR = %#.8X\tBASEPRI = %#.8X\n",
            context[R0_REG], context[R1_REG], context[R2_REG], context[R3_REG],
            context[R4_REG], context[R5_REG], context[R6_REG], context[R7_REG],
            context[R8_REG], context[R9_REG], context[R10_REG],
            context[R11_REG], context[R12_REG], context[SP_REG],
            context[LR_REG], context[PC_REG], context[PSR_REG],
            context[BASEPRI_REG]);

    kprintf("\n");

    analyze_status_registers();
}

void hardfault_handler(uint32_t *context)
{
    kprintf("=== Hard Fault Handler ===\n");
    dump_context(context);

    while (1)
        asm volatile("nop");
}

uint32_t memfault_handler(uint32_t *context)
{
#define PSR_ISR_NUM_MASK        0xff
#define EXCEPTION_THREAD_MODE   0

#define MMFSR 0xe000ed28

    uint32_t exception = context[PSR_REG] & PSR_ISR_NUM_MASK;
    int user_mode = context[CONTROL_REG] & 0x1;

    kprintf(user_mode ? "segfault\n" : "Oops\n");
    dump_context(context);

    if (exception == EXCEPTION_THREAD_MODE) {
        write32(MMFSR, ~0); // clear register
        task_exit();
    } else {
        panic(NULL);
    }

    return context[SP_REG];
}

static void irq_common_isr(void)
{
    int psr;
    int irq;

    asm volatile("mrs %0, xpsr" : "=r"(psr));
    irq = psr & 0xFF;

    irq_vector[irq].handler(irq - ARM_CM_NUM_EXCEPTION, irq_vector[irq].data);
}
