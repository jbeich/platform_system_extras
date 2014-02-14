#
# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES += \
    libbinder \
    libutils \

LOCAL_2ND_ARCH_VAR_PREFIX := $(binderLibTest_2nd_arch_var_prefix)

ifeq ($(TARGET_IS_64_BIT)|$(binderLibTest_2nd_arch_var_prefix),true|)
LOCAL_MODULE := binderLibTest64
else
LOCAL_MODULE := binderLibTest
endif
LOCAL_SRC_FILES := binderLibTest.cpp
include $(BUILD_NATIVE_TEST)
