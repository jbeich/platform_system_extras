# Copyright 2014 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_vfat.c
LOCAL_STATIC_LIBRARIES := \
    libsparse_host \
    libz
LOCAL_MODULE := libvfat_utils_host
include $(BUILD_HOST_STATIC_LIBRARY)
