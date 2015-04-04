include $(CLEAR_VARS)
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_additional_dependencies)

LOCAL_MODULE := $(module)
LOCAL_MODULE_TAGS := $(module_tag)

LOCAL_CLANG := true

LOCAL_CFLAGS := $($(module)_cflags)

LOCAL_CPPFLAGS := $($(module)_cppflags)

LOCAL_C_INCLUDES := $($(module)_c_includes)

LOCAL_EXPORT_C_INCLUDE_DIRS := $($(module)_export_c_include_dirs)

LOCAL_SRC_FILES := $($(module)_src_files)

LOCAL_STATIC_LIBRARIES := $($(module)_static_libraries)

LOCAL_SHARED_LIBRARIES := $($(module)_shared_libraries)

LOCAL_LDLIBS := \
  $($(module)_ldlibs) \
  $($(module)_ldlibs_$(build_type)) \

ifeq ($(build_type),target)
  include $(BUILD_$(build_target))
endif

ifeq ($(build_type),host)
  ifeq ($(build_host),true)
    include $(BUILD_HOST_$(build_target))
  endif
endif
