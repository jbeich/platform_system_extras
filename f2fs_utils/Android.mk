# Copyright 2017 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

ifneq ($(HOST_OS),windows)

include $(CLEAR_VARS)
LOCAL_MODULE := mkf2fsuserimg.sh
LOCAL_SRC_FILES := mkf2fsuserimg.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_REQUIRED_MODULES := make_f2fs sload_f2fs
# We don't need any additional suffix.
LOCAL_MODULE_SUFFIX :=
LOCAL_BUILT_MODULE_STEM := $(notdir $(LOCAL_SRC_FILES))
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PREBUILT)

endif
