#
# Copyright (C) 2012 The Android Open Source Project
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

# Only include the x86 encoder/decoder for x86 architecture
ifeq ($(TARGET_ARCH),x86)

LOCAL_PATH:= $(call my-dir)

ifneq ($(LIBENC_INCLUDED),true)

LIBENC_INCLUDED := true

enc_src_files := \
        enc_base.cpp \
        dec_base.cpp \
        enc_wrapper.cpp \
        enc_tabl.cpp

enc_include_files :=

##
##
## Build the device version of libenc
##
##
ifneq ($(SDK_ONLY),true)  # SDK_only doesn't need device version

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(enc_src_files)
LOCAL_C_INCLUDES += $(enc_include_files)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libenc
include $(BUILD_STATIC_LIBRARY)

endif # !SDK_ONLY


##
##
## Build the host version of libenc
##
##
include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(enc_src_files)
LOCAL_C_INCLUDES += $(enc_include_files)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libenc
include $(BUILD_HOST_STATIC_LIBRARY)

endif   # ifneq ($(LIBENC_INCLUDED),true)

endif   # ifeq ($(TARGET_ARCH),x86)
