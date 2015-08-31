#
# Copyright (C) 2010 The Android Open Source Project
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
# Copyright The Android Open Source Project

LOCAL_PATH := $(call my-dir)

test_module = dalvik-vm-unit-tests
test_tags = eng tests

test_src_files = \
    dvmHumanReadableDescriptor_test.cpp \
    
test_c_includes = \
    dalvik \
    dalvik/vm \

# Build for the device. Run with:
#   adb shell /data/nativetest/dalvik-vm-unit-tests/dalvik-vm-unit-tests
include $(CLEAR_VARS)
LOCAL_CFLAGS += -DANDROID_SMP=1
LOCAL_C_INCLUDES += $(test_c_includes)
LOCAL_MODULE := $(test_module)
LOCAL_MODULE_TAGS := $(test_tags)
LOCAL_SRC_FILES := $(test_src_files)
LOCAL_SHARED_LIBRARIES += libcutils libdvm
include $(BUILD_NATIVE_TEST)

# Build for the host.
# TODO: BUILD_HOST_NATIVE_TEST doesn't work yet; STL-related compile-time and
# run-time failures, presumably astl/stlport/genuine host STL confusion.
#include $(CLEAR_VARS)
#LOCAL_C_INCLUDES += $(test_c_includes)
#LOCAL_MODULE := $(test_module)
#LOCAL_MODULE_TAGS := $(test_tags)
#LOCAL_SRC_FILES := $(test_src_files)
#LOCAL_SHARED_LIBRARIES += libdvm libcrypto libssl libicuuc libicui18n
#LOCAL_WHOLE_STATIC_LIBRARIES += libcutils liblog libdvm
#include $(BUILD_HOST_NATIVE_TEST)
