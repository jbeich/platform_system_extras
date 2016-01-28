# Copyright 2010 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

libext4_utils_src_files := \
    make_ext4fs.cpp \
    ext4fixup.cpp \
    ext4_utils.cpp \
    allocate.cpp \
    contents.cpp \
    extent.cpp \
    indirect.cpp \
    sha1.cpp \
    wipe.cpp \
    crc16.cpp \
    ext4_sb.cpp

#
# -- All host/targets including windows
#

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils_host
# Various instances of dereferencing a type-punned pointer in extent.cpp
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_STATIC_LIBRARIES := \
    libsparse_host \
    libz
LOCAL_STATIC_LIBRARIES_darwin += libselinux
LOCAL_STATIC_LIBRARIES_linux += libselinux
LOCAL_MODULE_HOST_OS := darwin linux windows
include $(BUILD_HOST_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_ext4fs_main.cpp canned_fs_config.cpp
LOCAL_MODULE := make_ext4fs
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

libext4_utils_src_files += \
    key_control.cpp \
    ext4_crypt.cpp \
    unencrypted_properties.cpp

ifneq ($(HOST_OS),windows)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils
LOCAL_C_INCLUDES += system/core/logwrapper/include
# Various instances of dereferencing a type-punned pointer in extent.cpp
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libext2_uuid \
    libselinux \
    libsparse \
    libz
LOCAL_CFLAGS := -DREAL_UUID
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files) \
    ext4_crypt_init_extensions.cpp
LOCAL_MODULE := libext4_utils_static
# Various instances of dereferencing a type-punned pointer in extent.cpp
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_STATIC_LIBRARIES := \
    libsparse_static \
    libselinux
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_ext4fs_main.cpp canned_fs_config.cpp
LOCAL_MODULE := make_ext4fs
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libext2_uuid \
    libext4_utils \
    libselinux \
    libz
LOCAL_CFLAGS := -DREAL_UUID
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext2simg.cpp
LOCAL_MODULE := ext2simg
LOCAL_SHARED_LIBRARIES += \
    libext4_utils \
    libselinux \
    libsparse \
    libz
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext2simg.cpp
LOCAL_MODULE := ext2simg
LOCAL_SHARED_LIBRARIES += \
    libselinux
LOCAL_STATIC_LIBRARIES += \
    libext4_utils_host \
    libsparse_host \
    libz
include $(BUILD_HOST_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := setup_fs.cpp
LOCAL_MODULE := setup_fs
LOCAL_SHARED_LIBRARIES += libcutils
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext4fixup_main.cpp
LOCAL_MODULE := ext4fixup
LOCAL_SHARED_LIBRARIES += \
    libext4_utils \
    libsparse \
    libz
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext4fixup_main.cpp
LOCAL_MODULE := ext4fixup
LOCAL_STATIC_LIBRARIES += \
    libext4_utils_host \
    libsparse_host \
    libz
include $(BUILD_HOST_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := mkuserimg.sh
LOCAL_SRC_FILES := mkuserimg.sh
LOCAL_MODULE_CLASS := EXECUTABLES
# We don't need any additional suffix.
LOCAL_MODULE_SUFFIX :=
LOCAL_BUILT_MODULE_STEM := $(notdir $(LOCAL_SRC_FILES))
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PREBUILT)

endif
