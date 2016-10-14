# Copyright 2015 The Android Open Source Project

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := bootctl.cpp
LOCAL_MODULE := bootctl
LOCAL_SHARED_LIBRARIES := \
    libhidl \
    libhwbinder \
    libutils \
    android.hardware.boot@1.0 \

include $(BUILD_EXECUTABLE)
