# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := dtoverlay_test_app
LOCAL_SRC_FILES := dtoverlay_test_app.c
LOCAL_STATIC_LIBRARIES := \
    libdtoverlay \
    libufdt libfdt \
    libdtoverlay_sysdeps
LOCAL_REQUIRED_MODULES := dtc

include $(BUILD_HOST_NATIVE_TEST)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libufdt_hash_test_app
LOCAL_SRC_FILES := libufdt_hash_test_app.c
LOCAL_STATIC_LIBRARIES := \
    libufdt \
    libfdt \
    libdtoverlay_sysdeps

include $(BUILD_HOST_NATIVE_TEST)

#####################################################

