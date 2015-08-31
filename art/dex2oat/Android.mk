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

include art/build/Android.executable.mk

DEX2OAT_SRC_FILES := \
	dex2oat.cc

ifeq ($(ART_BUILD_TARGET_NDEBUG),true)
  $(eval $(call build-art-executable,dex2oat,$(DEX2OAT_SRC_FILES),libcutils libart-compiler,art/compiler,target,ndebug))
endif
ifeq ($(ART_BUILD_TARGET_DEBUG),true)
  $(eval $(call build-art-executable,dex2oat,$(DEX2OAT_SRC_FILES),libcutils libartd-compiler,art/compiler,target,debug))
endif

ifeq ($(WITH_HOST_DALVIK),true)
  # We always build dex2oat and dependencies, even if the host build is otherwise disabled, since they are used to cross compile for the target.
  ifeq ($(ART_BUILD_NDEBUG),true)
    $(eval $(call build-art-executable,dex2oat,$(DEX2OAT_SRC_FILES),libart-compiler,art/compiler,host,ndebug))
  endif
  ifeq ($(ART_BUILD_NDEBUG),true)
    $(eval $(call build-art-executable,dex2oat,$(DEX2OAT_SRC_FILES),libartd-compiler,art/compiler,host,debug))
  endif
endif
