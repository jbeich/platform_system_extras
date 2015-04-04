LOCAL_PATH := $(call my-dir)

ifeq ($(HOST_OS)-$(HOST_ARCH),$(filter $(HOST_OS)-$(HOST_ARCH),linux-x86 linux-x86_64))
  build_host := true
else
  build_host := false
endif

common_additional_dependencies := $(LOCAL_PATH)/Android.mk $(LOCAL_PATH)/Android.build.mk

libsimpleperf_cppflags := \
  -std=c++11 -Wall -Wextra -Werror #-Wunused \

libsimpleperf_c_includes := \
  $(LOCAL_PATH) \
  $(LOCAL_PATH)/include \

libsimpleperf_export_c_include_dirs := \
  $(LOCAL_PATH)/include \

libsimpleperf_src_files := \
  command.cpp \
  environment.cpp \
  event_attr.cpp \
  event.cpp \
  event_fd.cpp \
  lib_interface.cpp \
  record.cpp \
  record_file.cpp \
  workload.cpp \

libsimpleperf_ldlibs_host := \
  -lrt \

module := libsimpleperf
module_tag := optional
build_type := target
build_target := STATIC_LIBRARY
include $(LOCAL_PATH)/Android.build.mk
build_type := host
include $(LOCAL_PATH)/Android.build.mk


simpleperf_cppflags := \
  -std=c++11 -Wall -Wextra -Werror #-Wunused \

simpleperf_c_includes := \
  $(LOCAL_PATH) \

simpleperf_src_files := \
  perf_main.cpp \

simpleperf_static_libraries := \
  libsimpleperf \

simpleperf_ldlibs_host := \
  -lrt \

module := simpleperf
module_tag := optional
build_type := target
build_target := EXECUTABLE
include $(LOCAL_PATH)/Android.build.mk
build_type := host
include $(LOCAL_PATH)/Android.build.mk
