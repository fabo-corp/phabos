# Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
# Author: Fabien Parent <parent.f@gmail.com>
#
# Provided under the three clause BSD license found in the LICENSE file.

obj-y := isr.o
obj-y += aeabi.o
obj-y += boot.o
obj-y += irq.o
obj-y += spinlock.o
obj-y += atomic.o
obj-y += scheduler.o
obj-y += irq-handler.o
obj-$(CONFIG_SCHEDULER_WATCHDOG) += watchdog.o

ld-script-y += arm-v7m.ld
