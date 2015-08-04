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
 */

#include <asm/irq.h>

#include <phabos/greybus/debug.h>
#include <phabos/kprintf.h>

#include <stdarg.h>

#if defined(CONFIG_GB_LOG_ERROR)
#define GB_LOG_LEVEL (GB_LOG_ERROR)
#elif defined(CONFIG_GB_LOG_WARNING)
#define GB_LOG_LEVEL (GB_LOG_ERROR | GB_LOG_WARNING)
#elif defined(CONFIG_GB_LOG_DEBUG)
#define GB_LOG_LEVEL (GB_LOG_ERROR | GB_LOG_WARNING | GB_LOG_DEBUG)
#elif defined(CONFIG_GB_LOG_DUMP)
#define GB_LOG_LEVEL (GB_LOG_ERROR | GB_LOG_WARNING | GB_LOG_DEBUG | \
                      GB_LOG_DUMP)
#else
#define GB_LOG_LEVEL (GB_LOG_INFO)
#endif

int gb_log_level = GB_LOG_LEVEL;

void _gb_log(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    irq_disable();
    kvprintf(fmt, ap);
    irq_enable();

    va_end(ap);
}

void _gb_dump(const char *func, __u8 *buf, size_t size)
{
    int i;

    irq_disable();
    kprintf("%s:\n", func);
    for (i = 0; i < size; i++) {
        kprintf( "%02x ", buf[i]);
    }
    kprintf("\n");
    irq_enable();
}
