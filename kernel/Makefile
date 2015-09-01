# Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
# Author: Fabien Parent <parent.f@gmail.com>
#
# Provided under the three clause BSD license found in the LICENSE file.

obj-y := main.o
obj-y += mm.o
obj-y += signal.o
obj-$(CONFIG_MCACHE) += mcache.o
obj-y += spawn.o
obj-y += libc-support.o
obj-y += scheduler.o
obj-$(CONFIG_SCHED_RR) += scheduler-rr.o
obj-$(CONFIG_SCHED_FIFO) += scheduler-fifo.o
obj-y += task.o
obj-y += panic.o
obj-y += syscall.o
obj-$(CONFIG_ELF) += elf-loader.o
obj-y += dev.o

ld-script-y += kernel.ld
