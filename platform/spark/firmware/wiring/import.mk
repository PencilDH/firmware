WIRING_MODULE_PATH ?= $(PROJECT_ROOT)/wiring
include $(call rwildcard,$(WIRING_MODULE_PATH)/,include.mk)

WIRING_LIB_DIR = $(BUILD_PATH_BASE)/wiring/$(BUILD_TARGET_PLATFORM)
WIRING_LIB_DEP = $(WIRING_LIB_DIR)/libwiring.a