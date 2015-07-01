/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <config.h>
#include <phabos/scheduler.h>
#include <phabos/watchdog.h>
#include <asm/scheduler.h>
#include <asm/hwio.h>
#include <asm/machine.h>

#define ICSR                            0xE000ED04
#define ICSR_PENDSVSET                  (1 << 28)

#define SHPR3                           0xE000ED20
#define SHPR3_PENDSV_PRIO_OFFSET        2

#define THUMB_MASK                      (1 << 24)
#define NEW_TASK_PSR                    THUMB_MASK
#define RETURN_TO_SUPERVISOR_THREAD     0xFFFFFFF9

#define PSR_ISR_NUM_MASK                0xFF
#define EXCEPTION_THREAD_MODE           0

#define SYST_RVR 0xE000E014
#define SYST_CVR 0xE000E018
#define SYST_RVR_MAX 0xffffff

#define STCSR                0xE000E010
#define STCSR_SYSTICK_ENABLE (1 << 0)
#define STCSR_TICKINT        (1 << 1)
#define STCSR_CLKSOURCE      (1 << 2)

#if DEBUG_SCHEDULER
static const char* const reg_names[] = {
    [R0_REG] = "R0",
    [R1_REG] = "R1",
    [R2_REG] = "R2",
    [R3_REG] = "R3",
    [R4_REG] = "R4",
    [R5_REG] = "R5",
    [R6_REG] = "R6",
    [R7_REG] = "R7",
    [R8_REG] = "R8",
    [R9_REG] = "R9",
    [R10_REG] = "R10",
    [R11_REG] = "R11",
    [R12_REG] = "R12",
    [SP_REG] = "SP",
    [LR_REG] = "LR",
    [PC_REG] = "PC",
    [PSR_REG] = "PSR",
    [EXC_RETURN_REG] = "EXC_RETURN",
    [BASEPRI_REG] = "BASEPRI",
    [REENT_REG] = "REENT",
};
#endif

extern struct task *current;
extern bool need_resched;

static uint32_t ticks_increment;
uint64_t scheduler_ticks;
void watchdog_check_expired(void);

static uint32_t sched_max_multiplier(void)
{
    return SYST_RVR_MAX / (CPU_FREQ / HZ);
}

void sched_set_tick_multiplier(uint64_t tick_multiplier)
{
    RET_IF_FAIL(tick_multiplier > 0,);

    if (tick_multiplier == ~0ull) {
        write32(STCSR, 0);
        return;
    }

    if (tick_multiplier > sched_max_multiplier())
        tick_multiplier = sched_max_multiplier();

    if (ticks_increment == tick_multiplier)
        return;

    ticks_increment = tick_multiplier;

    write32(STCSR, 0);
    write32(SYST_RVR, (CPU_FREQ / HZ) * tick_multiplier);
    write32(SYST_CVR, 0);
    write32(STCSR, STCSR_SYSTICK_ENABLE | STCSR_TICKINT | STCSR_CLKSOURCE);
}

uint32_t sched_get_tick_multiplier(void)
{
    return ticks_increment;
}

void sched_configure_next_tick(void)
{
    sched_set_tick_multiplier(watchdog_get_ticks_until_next_expiration());
}

void scheduler_arch_init(void)
{
    scheduler_ticks = 0;

    /* lower the priority of PendSV */
    write8(SHPR3 + SHPR3_PENDSV_PRIO_OFFSET, 255);

    sched_set_tick_multiplier(1);
}

void task_init_registers(struct task *task, void *task_entry, void *data,
                         uint32_t stack_addr)
{
    memset(&task->registers, 0, sizeof(task->registers));
    task->registers[SP_REG] = stack_addr - sizeof(struct _reent);
    task->registers[PC_REG] = ((uint32_t) task_entry) & ~1; /* Store PC as ARM addr */
    task->registers[LR_REG] = (uint32_t) task_exit;
    task->registers[PSR_REG] = NEW_TASK_PSR;
    task->registers[EXC_RETURN_REG] = RETURN_TO_SUPERVISOR_THREAD;
    task->registers[R0_REG] = (uint32_t) data;

    /* init task's libc */
    task->registers[REENT_REG] = task->registers[SP_REG];
    _REENT_INIT_PTR((struct _reent*) task->registers[REENT_REG]);

    task->registers[SP_REG] -= sizeof(task->registers);
}

void sched_yield(void)
{
    need_resched = true;
    write32(ICSR, read32(ICSR) | ICSR_PENDSVSET);
}

uint32_t systick_handler(uint32_t *stack_top)
{
    scheduler_ticks += ticks_increment;

#ifdef CONFIG_SCHEDULER_WATCHDOG
    watchdog_check_expired();
#endif

    uint32_t exception = stack_top[PSR_REG] & PSR_ISR_NUM_MASK;
    if (exception == EXCEPTION_THREAD_MODE) {
        schedule(stack_top);
    } else {
        write32(ICSR, read32(ICSR) | ICSR_PENDSVSET);
        need_resched = true;
        return (uint32_t) (stack_top + 1);
    }

    return current->registers[SP_REG];
}

uint32_t pendsv_handler(uint32_t *stack_top)
{
    uint32_t sp;

    irq_disable();

    if (need_resched)
        schedule(stack_top);

    sp = current->registers[SP_REG];

    irq_enable();

    return sp;
}
