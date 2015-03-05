
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# 
# Static library containing guts of AWP daemon. 
#
LOCAL_C_INCLUDES := system/core/include
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_MODULE := libperfprofdcore
LOCAL_SRC_FILES :=  \
	perf_profile.proto \
	perf_utils.cc \
	base/logging.cc \
	address_mapper.cc \
	perf_reader.cc \
	perf_parser.cc \
	perf_data_converter.cc \
	perfprofdcore.cc
LOCAL_CPPFLAGS += -Wall -Wno-sign-compare -Wno-unused-parameter -Werror -std=gnu++11 
include $(BUILD_STATIC_LIBRARY)
include $(CLEAR_VARS)

#
# Static library with primary utilities layer (called by perfprofd core)
#
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_MODULE := libperfprofdutils
LOCAL_CFLAGS := -Wall -Werror -std=gnu++11
LOCAL_SRC_FILES := perfprofdutils.cc
include $(BUILD_STATIC_LIBRARY)
include $(CLEAR_VARS)

#
# Main daemon 
#
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_SRC_FILES := perfprofdmain.cc
LOCAL_STATIC_LIBRARIES := libperfprofdcore libperfprofdutils 
LOCAL_SHARED_LIBRARIES := liblog libprotobuf-cpp-full
LOCAL_SYSTEM_SHARED_LIBRARIES := libc libstdc++
LOCAL_CFLAGS := -Wall -Werror -std=gnu++11
LOCAL_MODULE := perfprofd
LOCAL_SHARED_LIBRARIES += libcutils
include $(BUILD_EXECUTABLE)
include $(CLEAR_VARS)

#
# Config file (perfprofd.conf)
#
LOCAL_MODULE := perfprofd.conf
LOCAL_SRC_FILES := perfprofd.conf
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)/system/etc
include $(BUILD_PREBUILT)

