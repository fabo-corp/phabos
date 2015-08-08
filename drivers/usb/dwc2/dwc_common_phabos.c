/*
 * Copyright (c) 2014-2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 *
 * Based on dwc_common_linux.c
 */

#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>

#include <asm/byteordering.h>
#include <asm/machine.h>
#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/hwio.h>
#include <phabos/time.h>
#include <phabos/scheduler.h>
#include <phabos/workqueue.h>
#include <phabos/utils.h>
#include <phabos/workqueue.h>
#include <phabos/sleep.h>
#include <phabos/assert.h>
#include <phabos/watchdog.h>

/* OS-Level Implementations */

#include "dwc_os.h"
#include "dwc_list.h"

#ifdef CONFIG_USB_DWC2_QUIET
#undef __DWC_WARN
#undef __DWC_ERROR
#undef DWC_PRINTF
#endif

/* MISC */

void *DWC_MEMSET(void *dest, uint8_t byte, uint32_t size)
{
    return memset(dest, byte, size);
}

void *DWC_MEMCPY(void *dest, void const *src, uint32_t size)
{
    return memcpy(dest, src, size);
}

void *DWC_MEMMOVE(void *dest, void *src, uint32_t size)
{
    return memmove(dest, src, size);
}

int DWC_MEMCMP(void *m1, void *m2, uint32_t size)
{
    return memcmp(m1, m2, size);
}

int DWC_STRNCMP(void *s1, void *s2, uint32_t size)
{
    return strncmp(s1, s2, size);
}

int DWC_STRCMP(void *s1, void *s2)
{
    return strcmp(s1, s2);
}

int DWC_STRLEN(char const *str)
{
    return strlen(str);
}

char *DWC_STRCPY(char *to, char const *from)
{
    return strcpy(to, from);
}

char *DWC_STRDUP(char const *str)
{
    return strdup(str);
}

int DWC_ATOI(const char *str, int32_t *value)
{
    char *end = NULL;

    *value = strtol(str, &end, 0);
    if (*end == '\0') {
        return 0;
    }

    return -1;
}

int DWC_ATOUI(const char *str, uint32_t *value)
{
    char *end = NULL;

    *value = strtoul(str, &end, 0);
    if (*end == '\0') {
        return 0;
    }

    return -1;
}

/* dwc_debug.h */

dwc_bool_t DWC_IN_IRQ(void)
{
    uint32_t xpsr;
    asm volatile("mrs %0, xpsr" : "=r"(xpsr));
    return (xpsr & 0xff) != 0;
}

dwc_bool_t DWC_IN_BH(void)
{
    return false;
}

void DWC_VPRINTF(char *format, va_list args)
{
    kvprintf(format, args);
}

int DWC_VSNPRINTF(char *str, int size, char *format, va_list args)
{
    return vsnprintf(str, size, format, args);
}

void DWC_PRINTF(char *format, ...)
{
    va_list args;

    va_start(args, format);
    DWC_VPRINTF(format, args);
    va_end(args);
}

int DWC_SPRINTF(char *buffer, char *format, ...)
{
    int retval;
    va_list args;

    va_start(args, format);
    retval = vsprintf(buffer, format, args);
    va_end(args);
    return retval;
}

int DWC_SNPRINTF(char *buffer, int size, char *format, ...)
{
    int retval;
    va_list args;

    va_start(args, format);
    retval = vsnprintf(buffer, size, format, args);
    va_end(args);
    return retval;
}

void __DWC_WARN(char *format, ...)
{
    va_list args;

    va_start(args, format);
    DWC_PRINTF("Warning: ");
    DWC_VPRINTF(format, args);
    va_end(args);
}

void __DWC_ERROR(char *format, ...)
{
    va_list args;

    va_start(args, format);
    DWC_PRINTF("Error: ");
    DWC_VPRINTF(format, args);
    va_end(args);
}

void DWC_EXCEPTION(char *format, ...)
{
    va_list args;

    va_start(args, format);
    DWC_PRINTF("Error: ");
    DWC_VPRINTF(format, args);
    va_end(args);
    asm volatile("b .");
}

#ifdef DEBUG
void __DWC_DEBUG(char *format, ...)
{
    va_list args;

    va_start(args, format);
    DWC_VPRINTF(format, args);
    va_end(args);
}
#endif

/* dwc_mem.h */

void *__DWC_DMA_ALLOC(void *dma_ctx, uint32_t size, dwc_dma_t *dma_addr)
{
    void *buf = __DWC_ALLOC(dma_ctx, size);
    *dma_addr = (dwc_dma_t) buf;
    if (!buf) {
        return NULL;
    }

    memset(buf, 0, (size_t)size);
    return buf;
}

void *__DWC_DMA_ALLOC_ATOMIC(void *dma_ctx, uint32_t size, dwc_dma_t *dma_addr)
{
    void *buf = __DWC_ALLOC_ATOMIC(dma_ctx, size);
    *dma_addr = (dwc_dma_t) buf;
    if (!buf) {
        return NULL;
    }

    memset(buf, 0, (size_t)size);
    return buf;
}

void __DWC_DMA_FREE(void *dma_ctx, uint32_t size, void *virt_addr,
                    dwc_dma_t dma_addr)
{
    kfree(virt_addr);
}

void *__DWC_ALLOC(void *mem_ctx, uint32_t size)
{
    void *ptr = kmalloc(size, MM_DMA);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

void *__DWC_ALLOC_ATOMIC(void *mem_ctx, uint32_t size)
{
    void *ptr;

    irq_disable();
    ptr = __DWC_ALLOC(mem_ctx, size);
    irq_enable();
    return ptr;
}

void __DWC_FREE(void *mem_ctx, void *addr)
{
    kfree(addr);
}

/* Byte Ordering Conversions */

uint32_t DWC_CPU_TO_LE32(uint32_t *p)
{
    return cpu_to_le32(*p);
}

uint32_t DWC_CPU_TO_BE32(uint32_t *p)
{
    return cpu_to_be32(*p);
}

uint32_t DWC_LE32_TO_CPU(uint32_t *p)
{
    return le32_to_cpu(*p);
}

uint32_t DWC_BE32_TO_CPU(uint32_t *p)
{
    return be32_to_cpu(*p);
}

uint16_t DWC_CPU_TO_LE16(uint16_t *p)
{
    return cpu_to_le16(*p);
}

uint16_t DWC_CPU_TO_BE16(uint16_t *p)
{
    return cpu_to_be16(*p);
}

uint16_t DWC_LE16_TO_CPU(uint16_t *p)
{
    return le16_to_cpu(*p);
}

uint16_t DWC_BE16_TO_CPU(uint16_t *p)
{
    return be16_to_cpu(*p);
}

/* Registers */

uint32_t DWC_READ_REG32(uint32_t volatile *reg)
{
    return read32((uint32_t) reg);
}

void DWC_WRITE_REG32(uint32_t volatile *reg, uint32_t value)
{
    write32((uint32_t) reg, value);
}

void DWC_MODIFY_REG32(uint32_t volatile *reg, uint32_t clear_mask,
                      uint32_t set_mask)
{
    DWC_WRITE_REG32(reg, (DWC_READ_REG32(reg) & ~clear_mask) | set_mask);
}

/* Locking */

dwc_spinlock_t *DWC_SPINLOCK_ALLOC(void)
{
    return (dwc_spinlock_t *)~0u;
}

void DWC_SPINLOCK_FREE(dwc_spinlock_t *lock)
{
}

void DWC_SPINLOCK(dwc_spinlock_t *lock)
{
    sched_lock();
}

void DWC_SPINUNLOCK(dwc_spinlock_t *lock)
{
    sched_unlock();
}

void DWC_SPINLOCK_IRQSAVE(dwc_spinlock_t *lock, dwc_irqflags_t *flags)
{
    irq_disable();
}

void DWC_SPINUNLOCK_IRQRESTORE(dwc_spinlock_t *lock, dwc_irqflags_t flags)
{
    irq_enable();
}

/* Timing */

void DWC_UDELAY(uint32_t usecs)
{
    udelay(usecs);
}

void DWC_MDELAY(uint32_t msecs)
{
    mdelay(msecs);
}

void DWC_MSLEEP(uint32_t msecs)
{
    usleep(msecs * 1000);
}

uint32_t DWC_TIME(void)
{
    uint32_t r;
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    r = t.tv_sec * 1000 + t.tv_nsec / 1000000;
    return r;
}

/* Timers */

struct dwc_timer {
    struct watchdog watchdog;
    dwc_timer_callback_t cb;
    void *data;
    uint32_t period;
    dwc_spinlock_t *lock;
};

static void timer_callback(struct watchdog *watchdog)
{
    struct dwc_timer *timer =
        containerof(watchdog, struct dwc_timer, watchdog);

    RET_IF_FAIL(timer->cb,);
    timer->cb(timer->data);

    if (timer->period)
        watchdog_start(&timer->watchdog, timer->period);
}

dwc_timer_t *DWC_TIMER_ALLOC(char *name, dwc_timer_callback_t cb, void *data)
{
    dwc_timer_t *t = malloc(sizeof(*t));
    if (!t)
        return NULL;

    memset(t, 0, sizeof(*t));
    watchdog_init(&t->watchdog);
    t->watchdog.timeout = timer_callback;
    t->cb = cb;
    t->data = data;
    return t;
}

void DWC_TIMER_FREE(dwc_timer_t *timer)
{
    watchdog_delete(&timer->watchdog);
    free(timer);
}

void _DWC_TIMER_SCHEDULE(dwc_timer_t *timer, uint32_t time)
{
    watchdog_start(&timer->watchdog, time * 1000);
}

void DWC_TIMER_SCHEDULE(dwc_timer_t *timer, uint32_t time)
{
    _DWC_TIMER_SCHEDULE(timer, time);
}

void DWC_TIMER_SCHEDULE_PERIODIC(dwc_timer_t *timer, uint32_t time)
{
    timer->period = time;
    _DWC_TIMER_SCHEDULE(timer, time);
}

void DWC_TIMER_CANCEL(dwc_timer_t *timer)
{
    watchdog_cancel(&timer->watchdog);
}

/* tasklets
 - run in interrupt context (cannot sleep)
 - each tasklet runs on a single CPU
 - different tasklets can be running simultaneously on different CPUs
 */
struct dwc_tasklet {
    struct task *task;
    dwc_tasklet_callback_t cb;
    void *data;
};

dwc_tasklet_t *DWC_TASK_ALLOC(char *name, dwc_tasklet_callback_t cb, void *data)
{
    dwc_tasklet_t *t = malloc(sizeof(*t));
    if (!t)
        return NULL;

    t->cb = cb;
    t->data = data;

    return t;
}

void DWC_TASK_FREE(dwc_tasklet_t *task)
{
    // FIXME free task memory
    // FIXME remove task from runqueue
    free(task);
}

void DWC_TASK_SCHEDULE(dwc_tasklet_t *task)
{
    task->task = task_run(task->cb, task->data, 0);
}

/* workqueues
 - run in process context (can sleep)
 */
struct dwc_workq {
};

int DWC_WORKQ_WAIT_WORK_DONE(dwc_workq_t *wq, int timeout)
{
    return workqueue_wait_empty((struct workqueue*) wq, timeout);
}

dwc_workq_t *DWC_WORKQ_ALLOC(char *name)
{
    return (dwc_workq_t*) workqueue_create(name);
}

void DWC_WORKQ_FREE(dwc_workq_t *wq)
{
    workqueue_destroy((struct workqueue*) wq);
}

void DWC_WORKQ_SCHEDULE(dwc_workq_t *wq, dwc_work_callback_t cb, void *data,
                        char *format, ...)
{
    workqueue_queue((struct workqueue*) wq, cb, data);
}

void DWC_WORKQ_SCHEDULE_DELAYED(dwc_workq_t *wq, dwc_work_callback_t cb,
                void *data, uint32_t time, char *format, ...)
{
    workqueue_schedule((struct workqueue*) wq, cb, data, time);
}

int DWC_WORKQ_PENDING(dwc_workq_t *wq)
{
    return workqueue_has_pending_work((struct workqueue*) wq);
}

void *phys_to_virt(unsigned long address)
{
    return (void *)address;
}
