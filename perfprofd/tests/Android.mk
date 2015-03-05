
# Build the unit tests.
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


#
# Static library with mockup utilities layer (called by unit test).
#
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_C_INCLUDES += system/extras/perfprofd
LOCAL_MODULE := libperfprofdmockutils
LOCAL_CFLAGS += -Wall -Werror -std=gnu++11 
LOCAL_SRC_FILES := perfprofdmockutils.cc
include $(BUILD_STATIC_LIBRARY)
include $(CLEAR_VARS)

#
# Canned perf.data files needed by unit test. 
#
LOCAL_MODULE := canned.perf.data
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := DATA
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest/perfprofd_test
LOCAL_SRC_FILES := canned.perf.data
include $(BUILD_PREBUILT)
include $(CLEAR_VARS)

#
# Unit test for perfprofd
#
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_STATIC_LIBRARIES := \
    libperfprofdcore \
    libperfprofdmockutils 
LOCAL_SHARED_LIBRARIES := libprotobuf-cpp-full
LOCAL_C_INCLUDES += system/extras/perfprofd external/protobuf/src
LOCAL_SRC_FILES := perfprofd_test.cc
LOCAL_CPPFLAGS += -Wall -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function -Werror -std=gnu++11 
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_MODULE := perfprofd_test

include $(BUILD_NATIVE_TEST)

