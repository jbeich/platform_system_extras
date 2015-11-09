LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := tcp_nuke_addr_test

LOCAL_C_INCLUDES += frameworks/native/include external/libcxx/include
LOCAL_CPPFLAGS += -std=c++11 -Wall -Werror
LOCAL_SHARED_LIBRARIES := libc++
LOCAL_SRC_FILES := tcp_nuke_addr_test.cpp
LOCAL_MODULE_TAGS := eng tests


include $(CLEAR_VARS)
LOCAL_MODULE := tcp_server

LOCAL_CPPFLAGS += -std=c++11 -Wall -Werror
LOCAL_SRC_FILES := tcp_server.c
LOCAL_MODULE_TAGS := eng tests

include $(BUILD_NATIVE_TEST)


include $(CLEAR_VARS)
LOCAL_MODULE := tcp_client

LOCAL_CPPFLAGS += -std=c++11 -Wall -Werror
LOCAL_SRC_FILES := tcp_client.c
LOCAL_MODULE_TAGS := eng tests

include $(BUILD_NATIVE_TEST)


include $(CLEAR_VARS)
LOCAL_MODULE := tcp_nuke

LOCAL_SRC_FILES := tcp_nuke.c
LOCAL_MODULE_TAGS := eng tests

include $(BUILD_NATIVE_TEST)
