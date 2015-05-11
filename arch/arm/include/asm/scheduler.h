/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ARM_SCHEDULER_H__
#define __ARM_SCHEDULER_H__

#include <stdint.h>
#include <asm/irq.h>

typedef uint32_t register_t;
struct task;

enum register_offset
{
    SP_REG = 0,
    R4_REG,
    R5_REG,
    R6_REG,
    R7_REG,
    R8_REG,
    R9_REG,
    R10_REG,
    R11_REG,
    BASEPRI_REG,
    EXC_RETURN_REG,
    R0_REG,
    R1_REG,
    R2_REG,
    R3_REG,
    R12_REG,
    LR_REG,
    PC_REG,
    PSR_REG,

    MAX_REG,
};

static inline uint64_t get_ticks(void)
{
    uint64_t ticks;
    extern uint64_t scheduler_ticks;

    irq_disable();
    ticks = scheduler_ticks;
    irq_enable();

    return ticks;
}

void schedule(uint32_t *stack_top);
void scheduler_arch_init(void);
void task_init_registers(struct task *task, void *task_entry, void *data,
                         uint32_t stack_addr);

#endif /* __ARM_SCHEDULER_H__ */

