include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gpio-avm-74hc595
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/gpio-avm-74hc595

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gpio-avm-74hc595
  SUBMENU:=GPIO Support
  TITLE:=AVM 74HC595 shift register GPIO expander
  DEPENDS:=@GPIO_SUPPORT
  FILES:=$(PKG_BUILD_DIR)/gpio-avm-74hc595.ko
  AUTOLOAD:=$(call AutoProbe,gpio-avm-74hc595)
endef

define KernelPackage/gpio-avm-74hc595/description
  Out-of-tree kernel driver for the AVM-specific 74HC595
  serial-in/parallel-out shift register GPIO expander, as used
  for LED control on several AVM FritzBox devices.
endef


define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,gpio-avm-74hc595))
