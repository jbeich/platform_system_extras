# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libdtoverlay
LOCAL_SRC_FILES := fdt_overlay.c
LOCAL_STATIC_LIBRARIES := libfdt

include $(BUILD_HOST_STATIC_LIBRARY)

####################################################

include $(CLEAR_VARS)

LOCAL_MODULE := dtoverlay_test_app
LOCAL_SRC_FILES := dtoverlay_test_app.c
LOCAL_STATIC_LIBRARIES := libdtoverlay libfdt
LOCAL_REQUIRED_MODULES := dtc

include $(BUILD_HOST_NATIVE_TEST)
