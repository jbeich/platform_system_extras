# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

common_src_files := \
    fdt_to_ufdt.c \
    ufdt_node.c \
    ufdt_node_dict.c

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libufdt
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := \
    libfdt \
    libdtoverlay_sysdeps

include $(BUILD_STATIC_LIBRARY)

####################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libufdt_host
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := \
    libfdt_host \
    libdtoverlay_sysdeps_host

include $(BUILD_HOST_STATIC_LIBRARY)

###################################################
