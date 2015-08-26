# Copyright (C) 2015 Fabien Parent. All rights reserved.
# Author: Fabien Parent <parent.f@gmail.com>
#
# Provided under the three clause BSD license found in the LICENSE file.

CFLAGS += -Idrivers/greybus/

obj-y += ap.o
obj-y += svc.o
obj-y += control.o
obj-y += manifest.o
obj-y += bundle.o
obj-y += cport.o
obj-y += connection.o
obj-y += interface.o

obj-$(CONFIG_GREYBUS_AP_GPIO) += gpio.o
obj-$(CONFIG_GREYBUS_AP_I2C) += i2c.o
obj-$(CONFIG_GREYBUS_AP_UART) += uart.o
