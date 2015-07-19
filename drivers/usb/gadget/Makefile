obj-$(CONFIG_USB_DWC2) += gadget-dwc2.o
obj-y += gb-ap-bridge.o

ifeq ($(CONFIG_USB_DWC2),y)
CFLAGS += -DDWC_PHABOS
endif
