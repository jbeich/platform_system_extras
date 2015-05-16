LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_MODULE := fec
LOCAL_SRC_FILES := main.cpp image.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libsparse_host libz libmincrypt libfec_host libfec_rs_host
LOCAL_SHARED_LIBRARIES := libbase
LOCAL_CFLAGS += -Wall -Werror -O3
LOCAL_C_INCLUDES += external/fec
include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_MODULE := fec
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_SRC_FILES := main.cpp image.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libmincrypt libfec libfec_rs libbase
LOCAL_CFLAGS += -Wall -Werror -O3 -DIMAGE_NO_SPARSE=1
LOCAL_C_INCLUDES += external/fec
include $(BUILD_EXECUTABLE)
