ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
# Per Platform
ifeq ($(CONFIG_MACH_SDM450_CV7AS),y)
DTS_PROJECT := sdm450-cv7as
endif

# Per Device
ifeq ($(CONFIG_MACH_SDM450_CV7AS),y)
DTS_TARGET := sdm450-cv7as
endif

ifeq ($(CONFIG_MACH_SDM450_MH3J),y)
DTS_PROJECT := sdm450-mh3j
endif

# Per Device
ifeq ($(CONFIG_MACH_SDM450_MH3J),y)
DTS_TARGET := sdm450-mh3j
endif


ALL_DTS := $(notdir $(wildcard echo $(srctree)/$(src)/$(DTS_PROJECT)/$(DTS_TARGET)/*.dts))
dtbo-y += $(patsubst %.dts,%.dtbo,$(ALL_DTS))

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtbo,$(DTB_NAMES))
else
DTB_LIST := $(dtbo-y)
endif

always  := $(dtbo-y)

$(obj)/%.dtbo: $(src)/$(DTS_PROJECT)/$(DTS_TARGET)/%.dts FORCE
	$(call if_changed_dep,dtc)
	$(call if_changed,dtbo_verify)

clean-files := *.dtb
endif
