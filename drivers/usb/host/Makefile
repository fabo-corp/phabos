obj-$(CONFIG_USB_CORE) += core.o
obj-$(CONFIG_USB_CORE) += hub.o
#obj-$(CONFIG_USB_CORE) += generic.o
obj-$(CONFIG_USB_DWC2) += hcd-dwc2.o

ifeq ($(CONFIG_USB_DWC2),y)
CFLAGS += -DDWC_PHABOS
endif
