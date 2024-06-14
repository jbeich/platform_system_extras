
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := bootloader_unit_test
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../NOTICE
LOCAL_MODULE_TAGS := tests

#include $(BUILD_PHONY_PACKAGE)
$(call record-module-type,PHONY_PACKAGE)
#ifneq ($(strip $(LOCAL_SRC_FILES)),)
#$(error LOCAL_SRC_FILES are not allowed for phony packages)
#endif

LOCAL_MODULE_CLASS := FAKE
LOCAL_MODULE_SUFFIX := -timestamp

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): $(LOCAL_ADDITIONAL_DEPENDENCIES)
	$(hide) echo "Fake: $@"
	$(hide) mkdir -p $(dir $@)
	$(hide) touch $@
