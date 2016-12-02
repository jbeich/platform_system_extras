# Copyright 2010 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

libext4_utils_src_files := \
    make_ext4fs.c \
    ext4fixup.c \
    ext4_utils.c \
    allocate.c \
    contents.c \
    extent.c \
    indirect.c \
    sha1.c \
    wipe.c \
    crc16.c \
    ext4_sb.c

#
# -- All host/targets including windows
#

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils_host
# Various instances of dereferencing a type-punned pointer in extent.c
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := \
    libsparse_host
LOCAL_STATIC_LIBRARIES_darwin += libselinux
LOCAL_STATIC_LIBRARIES_linux += libselinux
LOCAL_MODULE_HOST_OS := darwin linux windows
include $(BUILD_HOST_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_ext4fs_main.c
LOCAL_MODULE := make_ext4fs
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_STATIC_LIBRARIES += \
    libext4_utils_host \
    libsparse_host \
    libz
LOCAL_LDLIBS_windows += -lws2_32
LOCAL_SHARED_LIBRARIES_darwin += libselinux
LOCAL_SHARED_LIBRARIES_linux += libselinux
LOCAL_CFLAGS_darwin := -DHOST
LOCAL_CFLAGS_linux := -DHOST
include $(BUILD_HOST_EXECUTABLE)


#
# -- All host/targets excluding windows
#

ifneq ($(HOST_OS),windows)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    system/core/logwrapper/include
# Various instances of dereferencing a type-punned pointer in extent.c
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_CFLAGS += -DREAL_UUID
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := \
    libbase \
    libcutils \
    libext2_uuid \
    libselinux \
    libsparse
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils_static
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include
# Various instances of dereferencing a type-punned pointer in extent.c
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := \
    liblogwrap \
    libsparse_static \
    libselinux \
    libbase
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_ext4fs_main.c
LOCAL_MODULE := make_ext4fs
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libext2_uuid \
    libext4_utils \
    libselinux \
    libz
LOCAL_CFLAGS := -DREAL_UUID
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := setup_fs.c
LOCAL_MODULE := setup_fs
LOCAL_SHARED_LIBRARIES += libcutils
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext4fixup_main.c
LOCAL_MODULE := ext4fixup
LOCAL_SHARED_LIBRARIES += \
    libext4_utils \
    libsparse \
    libz
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext4fixup_main.c
LOCAL_MODULE := ext4fixup
LOCAL_STATIC_LIBRARIES += \
    libext4_utils_host \
    libsparse_host \
    libz
include $(BUILD_HOST_EXECUTABLE)


endif
