LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    memory_replay.cpp \
	GetPss.cpp \

LOCAL_MODULE_TAGS := debug
LOCAL_MODULE := memory_replay

LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64
include $(BUILD_EXECUTABLE)
