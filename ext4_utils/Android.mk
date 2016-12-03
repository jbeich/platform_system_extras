# Copyright 2010 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)


libext2fs_lib := libext2fs \
    libext2_com_err

libext2fs_lib_static := libext2fs \
    libext2_com_err

libext2fs_lib_host = $(addsuffix -host, $(libext2fs_lib))


libext4_crypt_src_files += \
    ext4_crypt_init_extensions.cpp \
    key_control.cpp \
    ext4_crypt.cpp


include $(CLEAR_VARS)
LOCAL_MODULE := libext4_crypt
LOCAL_SRC_FILES := $(libext4_crypt_src_files)
LOCAL_SHARED_LIBRARIES := libbase libcutils liblogwrap
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ext4_crypt
LOCAL_EXPORT_C_INCLUDE_DIRS = $(LOCAL_PATH)/include/ext4_crypt
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libext4_crypt
LOCAL_SRC_FILES := $(libext4_crypt_src_files)
LOCAL_STATIC_LIBRARIES := libbase libcutils liblogwrap
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ext4_crypt
LOCAL_EXPORT_C_INCLUDE_DIRS = $(LOCAL_PATH)/include/ext4_crypt
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := blk_alloc_to_base_fs.c
LOCAL_MODULE := blk_alloc_to_base_fs
LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_CFLAGS_darwin := -DHOST
LOCAL_CFLAGS_linux := -DHOST
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


include $(CLEAR_VARS)
LOCAL_MODULE := mkuserimg_mke2fs.sh
LOCAL_SRC_FILES := mkuserimg_mke2fs.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_REQUIRED_MODULES := mke2fs e2fsdroid
# We don't need any additional suffix.
LOCAL_MODULE_SUFFIX :=
LOCAL_BUILT_MODULE_STEM := $(notdir $(LOCAL_SRC_FILES))
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PREBUILT)


ifneq ($(TARGET_USES_MKE2FS),true)
    include $(LOCAL_PATH)/old_ext4_utils/Android.mk
else

libext4_utils_src_files := \
    ext4_utils.c \
    ext4_mkfs.c \
    wipe.c

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils_host
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ext4_utils external/e2fsprogs/misc
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include/ext4_utils
LOCAL_STATIC_LIBRARIES := \
    $(libext2fs_lib_host) \
    libsparse_host \
    libz
LOCAL_WHOLE_STATIC_LIBRARIES := libext2fs_inode
LOCAL_MODULE_HOST_OS := darwin linux windows
# Linux: uuid_generate is part of libext2_uuid
LOCAL_STATIC_LIBRARIES_linux += libselinux libext2_uuid-host
# Darwin: uuid_geneate is part of the libc
LOCAL_STATIC_LIBRARIES_darwin += libselinux
# Windows: uuid_generate is a custom function
LOCAL_STATIC_LIBRARIES_windows += libcrypto
LOCAL_SRC_FILES_windows += win32_uuid.c
LOCAL_LDLIBS_windows := -lws2_32
include $(BUILD_HOST_SHARED_LIBRARY)


####### REMOVE: we should not build this.
#######         Fastboot is not allowed to use it.
include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils_host
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ext4_utils external/e2fsprogs/misc
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include/ext4_utils
LOCAL_STATIC_LIBRARIES := \
    $(libext2fs_lib_host) \
    libsparse_host \
    libz
LOCAL_WHOLE_STATIC_LIBRARIES := libext2fs_inode
LOCAL_MODULE_HOST_OS := darwin linux windows
# Linux: uuid_generate is part of libext2_uuid
LOCAL_STATIC_LIBRARIES_linux := libselinux libext2_uuid-host
# Darwin: uuid_generate is part of the libc
LOCAL_STATIC_LIBRARIES_darwin := libselinux
# Windows: uuid_generate is a custom function
LOCAL_STATIC_LIBRARIES_windows := libcrypto
LOCAL_SRC_FILES_windows += win32_uuid.c
LOCAL_LDLIBS_windows := -lws2_32
include $(BUILD_HOST_STATIC_LIBRARY)


libext2fs_lib += libext2_uuid
libext2fs_lib_static += libext2_uuid_static


include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ext4_utils external/e2fsprogs/misc
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include/ext4_utils
LOCAL_WHOLE_STATIC_LIBRARIES := libext2fs_inode
LOCAL_SHARED_LIBRARIES := $(libext2fs_lib) \
    libbase \
    libcutils \
    liblogwrap \
    libselinux \
    libsparse
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils_static
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ext4_utils external/e2fsprogs/misc
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include/ext4_utils
LOCAL_WHOLE_STATIC_LIBRARIES := libext2fs_inode
LOCAL_STATIC_LIBRARIES := $(libext2fs_lib_static) \
    liblogwrap \
    libsparse_static \
    libz \
    libselinux \
    libbase
include $(BUILD_STATIC_LIBRARY)


endif
