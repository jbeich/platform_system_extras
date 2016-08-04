# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

common_src_files := \
    fdt_overlay.c \
    fdt_overlay_improved.c

###################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libdtoverlay
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

# libfdt is under external/dtc/libfdt while other
# libraries are under this folder.
LOCAL_STATIC_LIBRARIES := \
    libufdt \
    libfdt \
    libdtoverlay_sysdeps

include $(BUILD_STATIC_LIBRARY)

###################################################

include $(CLEAR_VARS)

# Host static libs are useful and simpler for testing and profiling.
# e.g. memory usage measurements and call graph generation.
LOCAL_MODULE := libdtoverlay_host
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

# libfdt_host is under external/dtc/libfdt while other
# libraries are under this folder.
LOCAL_STATIC_LIBRARIES := \
    libufdt_host \
    libfdt_host \
    libdtoverlay_sysdeps_host

include $(BUILD_HOST_STATIC_LIBRARY)

####################################################

include $(call first-makefiles-under, $(LOCAL_PATH))
