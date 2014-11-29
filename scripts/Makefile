# Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
# Author: Fabien Parent <parent.f@gmail.com>
#
# Provided under the three clause BSD license found in the LICENSE file.

KCONFIG_FRONTENDS_NAME     := kconfig-frontends
KCONFIG_FRONTENDS_VERSION  := 3.12.0.0
KCONFIG_FRONTENDS_SRC      := $(KCONFIG_FRONTENDS_NAME)-$(KCONFIG_FRONTENDS_VERSION)
KCONFIG_FRONTENDS_FILENAME := $(KCONFIG_FRONTENDS_SRC).tar.xz
KCONFIG_FRONTENDS_URL      := http://ymorin.is-a-geek.org/download/$(KCONFIG_FRONTENDS_NAME)/$(KCONFIG_FRONTENDS_FILENAME)

distclean:
	rm -f $(KCONFIG_FRONTENDS_FILENAME)
	rm -rf $(KCONFIG_FRONTENDS_SRC) $(KCONFIG_FRONTENDS_NAME)

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
