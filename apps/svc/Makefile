subdirs-y += commands

obj-y += ina230.o
obj-y += tsb_es2_mphy_fixups.o
obj-y += vreg.o
obj-y += interface.o
obj-y += up_debug.o
obj-y += tsb_switch.o
obj-y += svc.o
obj-y += tsb_switch_es1.o
obj-y += tsb_switch_es2.o
obj-y += up_bdb_pm.o

obj-$(CONFIG_BOARD_ARA_SVC_BDB1B) += board-bdb1b.o
obj-$(CONFIG_BOARD_ARA_SVC_BDB2) += board-bdb2a.o

CFLAGS += -Iapps/greybus-utils
