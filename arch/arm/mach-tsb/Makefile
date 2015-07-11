# Copyright (c) 2014-2015 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

obj-y += lowio.o
obj-y += scm.o
obj-$(CONFIG_TSB_BOARD_BDB) += board-bdb.o
obj-$(CONFIG_TSB_GPIO) += gpio.o
obj-$(CONFIG_TSB_CDSI) += cdsi.o
obj-$(CONFIG_ARA_BACKPORT) += driver.o
obj-$(CONFIG_ARA_BACKPORT) += device-table.o
obj-$(CONFIG_TSB_PLL) += pll.o
obj-$(CONFIG_TSB_I2S) += i2s.o

ifeq ($(CONFIG_TSB_ES2),y)
obj-$(CONFIG_TSB_UNIPRO) += es2-mphy-fixups.o
obj-$(CONFIG_TSB_UNIPRO) += unipro-es2.o
endif

ifeq ($(CONFIG_TSB_ES1),y)
obj-$(CONFIG_TSB_UNIPRO) += unipro-es1.o
endif
