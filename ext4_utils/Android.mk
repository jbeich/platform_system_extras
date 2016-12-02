# Copyright 2010 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)


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
# Add the new tools here
endif
