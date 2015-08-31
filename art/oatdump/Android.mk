#
# Copyright (C) 2011 The Android Open Source Project
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

OATDUMP_SRC_FILES := \
	oatdump.cc

include art/build/Android.executable.mk

ifeq ($(ART_BUILD_TARGET_NDEBUG),true)
  $(eval $(call build-art-executable,oatdump,$(OATDUMP_SRC_FILES),libcutils,,target,ndebug))
endif
ifeq ($(ART_BUILD_TARGET_DEBUG),true)
  $(eval $(call build-art-executable,oatdump,$(OATDUMP_SRC_FILES),libcutils,,target,debug))
endif

ifeq ($(WITH_HOST_DALVIK),true)
  ifeq ($(ART_BUILD_HOST_NDEBUG),true)
    $(eval $(call build-art-executable,oatdump,$(OATDUMP_SRC_FILES),,,host,ndebug))
  endif
  ifeq ($(ART_BUILD_HOST_DEBUG),true)
    $(eval $(call build-art-executable,oatdump,$(OATDUMP_SRC_FILES),,,host,debug))
  endif
endif
