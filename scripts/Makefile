# Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
# Author: Fabien Parent <parent.f@gmail.com>
#
# Provided under the three clause BSD license found in the LICENSE file.

KCONFIG_FRONTENDS_NAME     := kconfig-frontends
KCONFIG_FRONTENDS_VERSION  := 3.12.0.0
KCONFIG_FRONTENDS_SRC      := $(KCONFIG_FRONTENDS_NAME)-$(KCONFIG_FRONTENDS_VERSION)
KCONFIG_FRONTENDS_FILENAME := $(KCONFIG_FRONTENDS_SRC).tar.xz
KCONFIG_FRONTENDS_URL      := http://ymorin.is-a-geek.org/download/$(KCONFIG_FRONTENDS_NAME)/$(KCONFIG_FRONTENDS_FILENAME)

NEWLIB_REPO_NAME   := newlib-nano
NEWLIB_REPO_URL    := https://github.com/fabo-corp/$(NEWLIB_REPO_NAME).git
NEWLIB_REPO_BRANCH := v2.1-arm-clang

distclean:
	rm -f $(KCONFIG_FRONTENDS_FILENAME)
	rm -rf $(KCONFIG_FRONTENDS_SRC) $(KCONFIG_FRONTENDS_NAME)
	rm -rf $(NEWLIB_REPO_NAME) $(KERNEL_ROOT)/libc

kconfig-%: $(KCONFIG_FRONTENDS_FILENAME)
	echo "    GEN $@"
	tar xJf $(KCONFIG_FRONTENDS_FILENAME); \
	cd $(KCONFIG_FRONTENDS_SRC); \
	unset CC LD AR AS CFLAGS CPPFLAGS ASFLAGS LDFLAGS; \
	./configure --prefix=`pwd`/../kconfig-frontends \
	            --enable-$(subst kconfig-,,$@) 1>./configure.log 2>&1; \
	if [ $$? -ne 0 ]; \
	then \
	    echo "ERROR: configuration of newlibc-nano failed"; \
	    echo "\tread ./scripts/$(NEWLIB_REPO_NAME)/newlib/configure.log for more details"; \
	    exit 1; \
	fi; \
	make 1>./build.log 2>&1; \
	if [ $$? -ne 0 ]; \
	then \
	    echo "ERROR: compilation of newlibc-nano failed"; \
	    echo "\tread ./scripts/$(NEWLIB_REPO_NAME)/newlib/build.log for more details"; \
	    exit 1; \
	fi; \
	make install 1>./install.log 2>&1
	if [ $$? -ne 0 ]; \
	then \
	    echo "ERROR: installation of newlibc-nano failed"; \
	    echo "\tread ./scripts/$(NEWLIB_REPO_NAME)/newlib/install.log for more details"; \
	    exit 1; \
	fi; \

$(KCONFIG_FRONTENDS_FILENAME):
	echo "    DL $@"
	wget $(KCONFIG_FRONTENDS_URL)

$(NEWLIB_REPO_NAME):
	echo "    DL $@:$(NEWLIB_REPO_BRANCH)"
	git clone $(NEWLIB_REPO_URL) -b $(NEWLIB_REPO_BRANCH) 2>/dev/null

libc: $(NEWLIB_REPO_NAME)
	echo "    GEN $@"
	cd ./$(NEWLIB_REPO_NAME)/newlib; \
	unset CFLAGS CPPLAGS LDFLAGS MAKEFILES; \
	CC=$(CC) CFLAGS="$(CFLAGS)" \
	./configure --build=$(call compiler-triple) \
	            --prefix=$(KERNEL_ROOT)/libc \
	            --disable-newlib-supplied-syscalls \
	            1>./configure.log 2>&1; \
	if [ $$? -ne 0 ]; \
	then \
	    echo "ERROR: configuration of newlibc-nano failed"; \
	    echo "\tread ./scripts/$(NEWLIB_REPO_NAME)/newlib/configure.log for more details"; \
	    exit 1; \
	fi; \
	make 1>./build.log 2>&1; \
	if [ $$? -ne 0 ]; \
	then \
	    echo "ERROR: compilation of newlibc-nano failed"; \
	    echo "\tread ./scripts/$(NEWLIB_REPO_NAME)/newlib/build.log for more details"; \
	    exit 1; \
	fi; \
	make install 1>./install.log 2>&1
	if [ $$? -ne 0 ]; \
	then \
	    echo "ERROR: installation of newlibc-nano failed"; \
	    echo "\tread ./scripts/$(NEWLIB_REPO_NAME)/newlib/install.log for more details"; \
	    exit 1; \
	fi; \

compiler-triple = $(subst --,-,$(shell $(CC) $(CFLAGS) -dumpmachine))
