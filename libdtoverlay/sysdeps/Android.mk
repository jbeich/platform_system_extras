# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libdtoverlay_sysdeps
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := libdtoverlay_sysdeps_posix.c
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := libfdt

include $(BUILD_STATIC_LIBRARY)

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libdtoverlay_sysdeps
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := libdtoverlay_sysdeps_posix.c
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := libfdt

include $(BUILD_HOST_STATIC_LIBRARY)

