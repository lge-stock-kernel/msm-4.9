ifeq ($(CONFIG_MACH_MSM8917_MH5LM),y)
DTS_PROJECT := msm8917-mh5lm
DTS_TARGET := msm8917-mh5lm
endif

ifeq ($(CONFIG_MACH_MSM8917_MH5LM_NONAB),y)
DTS_PROJECT := msm8917-mh5lm
DTS_TARGET := msm8917-mh5lm_nonAB
endif

ifeq ($(CONFIG_MACH_LGE), y)
dtb-$(CONFIG_ARCH_MSM8917) += msm8917-pmi8937.dtb \
   msm8917-pmi8940.dtb \
   msm8917-pmi8950.dtb
endif

ifeq ($(CONFIG_MACH_SDM450_MH4X),y)
DTS_PROJECT := sdm450-mh4x
DTS_TARGET := sdm450-mh4x
endif

ifeq ($(CONFIG_MACH_LGE), y)
dtb-$(CONFIG_ARCH_SDM450) += sdm450-pmi632.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
ALL_DTS := $(notdir $(wildcard echo $(srctree)/$(src)/../../../../arm64/boot/dts/lge/$(DTS_PROJECT)/$(DTS_TARGET)/*.dts))
dtbo-y += $(patsubst %.dts,%.dtbo,$(ALL_DTS))

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTBO_LIST := $(addsuffix .dtbo,$(DTB_NAMES))
else
DTBO_LIST := $(dtbo-y)
endif
endif

targets += dtbs
targets += $(addprefix ../, $(DTBO_LIST))

$(obj)/%.dtb: $(src)/../../../../arm64/boot/dts/qcom/%.dts FORCE
	$(call if_changed_dep,dtc)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
$(obj)/%.dtbo: $(src)/../../../../arm64/boot/dts/lge/$(DTS_PROJECT)/$(DTS_TARGET)/%.dts FORCE
	$(call if_changed_dep,dtc)
	$(call if_changed,dtbo_verify)

dtbs: $(addprefix $(obj)/,$(dtb-y)) $(addprefix $(obj)/,$(dtbo-y))
endif

clean-files := *.dtb
