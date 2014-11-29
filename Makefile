# Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
# Author: Fabien Parent <parent.f@gmail.com>
#
# Provided under the three clause BSD license found in the LICENSE file.

export

KERNEL_NAME ?= phabos
KERNEL_ROOT := $(shell pwd)
SHELL       := /bin/bash
MAKEFILES   := scripts/Makefile.build

ifeq (${MAKELEVEL}, 0)
all: $(KERNEL_NAME).elf

clean:
	$(MAKE) -f scripts/Makefile.common dir=. $@
endif

CONFIG := $(wildcard .config)
ifneq ($(CONFIG),)
include $(CONFIG)
endif

include scripts/Makefile.build
include arch/$(ARCH)/Makefile.build

subdirs-y := arch
clean-y := config.h $(KERNEL_NAME).elf objects.lst

.config:
	echo "ERROR: No config file loaded."
	exit 1

%_defconfig:
	cp arch/${ARCH}/configs/$@ .config
	echo "Loading $@..."

config.h: .config
	$(call generate-config)

distclean: clean
	$(MAKE) -C ./scripts/ distclean

$(KERNEL_NAME).elf: config.h
	rm -f objects.lst
	$(MAKE) -f scripts/Makefile.common dir=. all
	$(call build,LD)
	$(LD) $(LDFLAGS) $(linker_files) -o $@ \
		`cat objects.lst | tr '\n' ' '`

menuconfig: scripts/kconfig-frontends/bin/kconfig-mconf
	scripts/kconfig-frontends/bin/kconfig-mconf Kconfig

nconfig: scripts/kconfig-frontends/bin/kconfig-mconf
	scripts/kconfig-frontends/bin/kconfig-nconf Kconfig

config: scripts/kconfig-frontends/bin/kconfig-mconf
	scripts/kconfig-frontends/bin/kconfig-conf Kconfig

scripts/kconfig-frontends/bin/kconfig-%:
	$(MAKE) -C ./scripts/ $(subst scripts/kconfig-frontends/bin/,,$@)

.PHONY: $(KERNEL_NAME).elf
ifndef VERBOSE
.SILENT:
endif
