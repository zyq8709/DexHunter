#
# Copyright (C) 2013 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)

dalvikvm_cflags := -Wall -Werror -Wextra

include $(CLEAR_VARS)
LOCAL_MODULE := dalvikvm
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := cc
LOCAL_SRC_FILES := dalvikvm.cc
LOCAL_CFLAGS := $(dalvikvm_cflags)
LOCAL_SHARED_LIBRARIES := libdl libnativehelper
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include external/stlport/libstlport.mk
include $(BUILD_EXECUTABLE)
ART_TARGET_EXECUTABLES += $(TARGET_OUT_EXECUTABLES)/$(LOCAL_MODULE)

ifeq ($(WITH_HOST_DALVIK),true)
include $(CLEAR_VARS)
LOCAL_MODULE := dalvikvm
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := cc
LOCAL_SRC_FILES := dalvikvm.cc
LOCAL_CFLAGS := $(dalvikvm_cflags)
LOCAL_SHARED_LIBRARIES := libnativehelper
LOCAL_LDFLAGS := -ldl
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(BUILD_HOST_EXECUTABLE)
ART_HOST_EXECUTABLES += $(HOST_OUT_EXECUTABLES)/$(LOCAL_MODULE)
endif
