#
# Makefile for the touchscreen drivers.
#

# Each configuration option enables a list of files.

# Touch Core Driver Common Header Code..!!
ccflags-y += -I$(srctree)/drivers/input/touchscreen/lge/

obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4310)		+= touch_td4310.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4310)		+= touch_td4310_fw_upgrade.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4310)		+= touch_td4310_rmi_dev.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4310)		+= touch_td4310_prd.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4310)		+= touch_td4310_lpwg.o
#obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4310)		+= touch_td4310_f54_test.o # [bringup] need to check. file not exist.
