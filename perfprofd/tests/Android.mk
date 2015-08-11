# Build the unit tests.
LOCAL_PATH := $(call my-dir)

perfprofd_test_cppflags := -Wall -Wno-sign-compare -Wno-unused-parameter -Werror -std=gnu++11

#
# Static library with mockup utilities layer (called by unit test).
#
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_C_INCLUDES += system/extras/perfprofd
LOCAL_MODULE := libperfprofdmockutils
LOCAL_CPPFLAGS += $(perfprofd_test_cppflags)
LOCAL_SRC_FILES := perfprofdmockutils.cc
include $(BUILD_STATIC_LIBRARY)

# $(1) name of test file to copy
define copy-test-file
include $(CLEAR_VARS)
LOCAL_MODULE := $(1)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := DATA
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest/perfprofd_test
LOCAL_SRC_FILES := $(1)
include $(BUILD_PREBUILT)
endef

# Input files needed by unit test
$(eval $(call copy-test-file,canned.perf.data))
$(eval $(call copy-test-file,perfprofd.test.smallish.zip))

#
# Unit test for perfprofd
#
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_STATIC_LIBRARIES := libperfprofdcore libperfprofdmockutils libziparchive
LOCAL_SHARED_LIBRARIES := libprotobuf-cpp-lite libz libbase libutils
LOCAL_C_INCLUDES += system/extras/perfprofd external/protobuf/src
LOCAL_SRC_FILES := perfprofd_test.cc
LOCAL_CPPFLAGS += $(perfprofd_test_cppflags)
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_MODULE := perfprofd_test
include $(BUILD_NATIVE_TEST)

# Clean temp vars
perfprofd_test_cppflags :=
