LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES :=  tcp_nuke_addr_test.c

LOCAL_MODULE := tcp_nuke_addr_test

LOCAL_CFLAGS := -O2 -g -W -Wall -D__ANDROID__ -DNO_SCRIPT
LOCAL_SYSTEM_SHARED_LIBRARIES := libc
LOCAL_SHARED_LIBRARIES := libnetutils

include $(BUILD_EXECUTABLE)
