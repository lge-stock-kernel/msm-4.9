#
# Makefile for the fingerprint drivers.
#

# Each configuration option enables a list of files.
ccflags-y += -Idrivers/misc/

obj-$(CONFIG_INPUT_FPC1511)     += fpc1511_tee.o
obj-$(CONFIG_INPUT_FPC1035_DUAL)     += fpc1035_tee_dual.o
obj-$(CONFIG_FINGERPRINT_FPS998E_ZQL1606) += cdfingerfp/

ccflags-$(CONFIG_FINGERPRINT_FPC1035_ID_HI) += -DSUPPORT_FP_ID_DUAL -DFP_ID_VALUE=1
ccflags-$(CONFIG_FINGERPRINT_FPC1035_ID_LO) += -DSUPPORT_FP_ID_DUAL -DFP_ID_VALUE=0
