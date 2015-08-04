obj-y += core.o
obj-y += hub.o
obj-y += generic.o
obj-$(CONFIG_USB_DWC2) += hcd-dwc2.o

ifeq ($(CONFIG_USB_DWC2),y)
CFLAGS += -DDWC_PHABOS
endif
