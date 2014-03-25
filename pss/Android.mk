LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := pss
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)

LOCAL_C_INCLUDES := $(call include-path-for, libpagemap)

LOCAL_SRC_FILES := pss.cpp

LOCAL_SHARED_LIBRARIES := libpagemap

include $(BUILD_EXECUTABLE)
