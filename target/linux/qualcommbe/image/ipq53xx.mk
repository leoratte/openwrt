DTS_DIR := $(DTS_DIR)/qcom

define Device/avm_fritzbox7690
	$(call Device/FitImage)
	$(call Device/EmmcImage)
	DEVICE_VENDOR := AVM
	DEVICE_MODEL := FritzBox-7690
	DEVICE_DTS := ipq5332-avm-fritzbox7690
	DEVICE_DTS_CONFIG := config@avm-fritzbox7690
	SOC := ipq5332
	DEVICE_PACKAGES := f2fsck mkf2fs
endef
TARGET_DEVICES += avm_fritzbox7690
