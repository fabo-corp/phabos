/*
 * Copyright (c) 2015 Google Inc.
 * All rights reserved.
 *
 * Author: Fabien Parent <parent.f@gmail.com>
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

#include <config.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <asm/semihosting.h>

#if defined(CONFIG_CPU_ARMV7M)
#define SEMIHOSTING_SVC "bkpt 0xAB;"
#else
#define SEMIHOSTING_SVC "svc 0xAB;"
#endif

enum open_mode {
    READ_MODE = 0,
    WRITE_MODE = 4,
};

enum semihosting_stream {
    SEMIHOSTING_READ_STREAM = 0,
    SEMIHOSTING_WRITE_STREAM = 1,
};

enum semihosting_syscall {
    SYSCALL_OPEN = 0x1,
    SYSCALL_WRITEC = 0x3,
    SYSCALL_WRITE = 0x5,
    SYSCALL_READ = 0x6,
};

static int fd[2];

static uint32_t semihosting_syscall(int syscall, uint32_t *params)
{
    uint32_t result;

    asm volatile(
        "mov r0, %2;"
        "mov r1, %1;"
        SEMIHOSTING_SVC
        "mov %0, r0;"
        :"=r"(result)
        :"r"(params), "r"(syscall)
        :"r0", "r1", "memory"
    );

    return result;
}

static int semihosting_open(const char *const filename, int mode)
{
    uint32_t params[] = {
        (uint32_t) filename,
        mode,
        (uint32_t) strlen(filename)
    };

    return semihosting_syscall(SYSCALL_OPEN, &params[0]);
}

ssize_t semihosting_read(char *buffer, size_t buflen)
{
    ssize_t nread;
    uint32_t params[3];

    params[0] = (uint32_t) fd[SEMIHOSTING_READ_STREAM];
    params[1] = (uint32_t) buffer;
    params[2] = (uint32_t) buflen;

    nread = semihosting_syscall(SYSCALL_READ, &params[0]);

    return !nread ? buflen : nread;
}

ssize_t semihosting_write(const char *buffer, size_t buflen)
{
    size_t not_written = 0;
    uint32_t params[3];

    params[0] = (uint32_t) fd[SEMIHOSTING_WRITE_STREAM];
    params[1] = (uint32_t) buffer;
    params[2] = (uint32_t) buflen;

    not_written = semihosting_syscall(SYSCALL_WRITE, &params[0]);

    return buflen - not_written;
}

void semihosting_putc(char c)
{
    uint32_t c32 = c;
    semihosting_syscall(SYSCALL_WRITEC, (uint32_t*) &c32);
}

void semihosting_init(void)
{
    fd[SEMIHOSTING_READ_STREAM] = semihosting_open(":tt", READ_MODE);
    if (fd[SEMIHOSTING_READ_STREAM] == -1)
        return;

    fd[SEMIHOSTING_WRITE_STREAM] = semihosting_open(":tt", WRITE_MODE);
    if (fd[SEMIHOSTING_WRITE_STREAM] == -1)
        return;
}
