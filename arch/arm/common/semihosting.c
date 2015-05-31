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
#include <errno.h>

#include <asm/semihosting.h>
#include <phabos/driver.h>
#include <phabos/serial/tty.h>
#include <phabos/utils.h>

#if defined(CONFIG_CPU_ARMV7M)
#define SEMIHOSTING_SVC "bkpt 0xAB;"
#else
#define SEMIHOSTING_SVC "svc 0xAB;"
#endif

__driver__ struct driver semihosting_driver;

struct semihosting_device {
    struct device device;
    struct tty_device tty;

    int fd[2];
};

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

static struct semihosting_device semihosting_device = {
        .device = {
        .name = "arm_semihosting",
        .description = "ARM semihosting",
        .driver = "arm_semihosting",
    },
};

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

ssize_t semihosting_read(struct file *file, void *buffer, size_t buflen)
{
    ssize_t nread;
    uint32_t params[3];

    params[0] = (uint32_t) semihosting_device.fd[SEMIHOSTING_READ_STREAM];
    params[1] = (uint32_t) buffer;
    params[2] = (uint32_t) buflen;

    nread = semihosting_syscall(SYSCALL_READ, &params[0]);

    for (int i = 0; i < buflen - nread; i++) {
        char *buf = (char*) buffer;
        if (buf[i] == '\r')
            buf[i] = '\n';
    }

    return buflen - nread;
}

ssize_t semihosting_write(struct file *file, const void *buffer, size_t buflen)
{
    size_t not_written = 0;
    uint32_t params[3];

    params[0] = (uint32_t) semihosting_device.fd[SEMIHOSTING_WRITE_STREAM];
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

static struct file_operations semihosting_ops = {
    .read = semihosting_read,
    .write = semihosting_write,
};

static int semihosting_init(struct driver *drv)
{
    semihosting_device.fd[SEMIHOSTING_READ_STREAM] =
        semihosting_open(":tt", READ_MODE);
    if (semihosting_device.fd[SEMIHOSTING_READ_STREAM] == -1)
        return -ENODEV;

    semihosting_device.fd[SEMIHOSTING_WRITE_STREAM] =
        semihosting_open(":tt", WRITE_MODE);
    if (semihosting_device.fd[SEMIHOSTING_WRITE_STREAM] == -1)
        return -ENODEV;

    device_register(&semihosting_device.device);

    return 0;
}

static int semihosting_probe(struct device *device)
{
    struct semihosting_device *dev =
        containerof(device, struct semihosting_device, device);
    dev_t devnum;
    int retval;

    RET_IF_FAIL(device, -EINVAL);

    device->ops = semihosting_ops;

    retval = devnum_alloc(&semihosting_driver, device, &devnum);
    RET_IF_FAIL(!retval, retval);

    return tty_register(&dev->tty, devnum);
}

static int semihosting_remove(struct device *device)
{
    struct semihosting_device *dev =
        containerof(device, struct semihosting_device, device);

    RET_IF_FAIL(device, -EINVAL);

    return tty_unregister(&dev->tty);
}

__driver__ struct driver semihosting_driver = {
    .name = "arm_semihosting",

    .init = semihosting_init,
    .probe = semihosting_probe,
    .remove = semihosting_remove,
};
