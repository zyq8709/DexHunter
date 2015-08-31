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

ifndef ANDROID_COMMON_MK
ANDROID_COMMON_MK = true

# These can be overridden via the environment or by editing to
# enable/disable certain build configuration.
#
# For example, to disable everything but the host debug build you use:
#
# (export ART_BUILD_TARGET_NDEBUG=false && export ART_BUILD_TARGET_DEBUG=false && export ART_BUILD_HOST_NDEBUG=false && ...)
#
# Beware that tests may use the non-debug build for performance, notable 055-enum-performance
#
ART_BUILD_TARGET_NDEBUG ?= true
ART_BUILD_TARGET_DEBUG ?= true
ART_BUILD_HOST_NDEBUG ?= $(WITH_HOST_DALVIK)
ART_BUILD_HOST_DEBUG ?= $(WITH_HOST_DALVIK)

ifeq ($(ART_BUILD_TARGET_NDEBUG),false)
$(info Disabling ART_BUILD_TARGET_NDEBUG)
endif
ifeq ($(ART_BUILD_TARGET_DEBUG),false)
$(info Disabling ART_BUILD_TARGET_DEBUG)
endif
ifeq ($(ART_BUILD_HOST_NDEBUG),false)
$(info Disabling ART_BUILD_HOST_NDEBUG)
endif
ifeq ($(ART_BUILD_HOST_DEBUG),false)
$(info Disabling ART_BUILD_HOST_DEBUG)
endif

#
# Used to enable smart mode
#
ART_SMALL_MODE := false
ifneq ($(wildcard art/SMALL_ART),)
$(info Enabling ART_SMALL_MODE because of existence of art/SMALL_ART)
ART_SMALL_MODE := true
endif
ifeq ($(WITH_ART_SMALL_MODE), true)
ART_SMALL_MODE := true
endif

#
# Used to enable SEA mode
#
ART_SEA_IR_MODE := false
ifneq ($(wildcard art/SEA_IR_ART),)
$(info Enabling ART_SEA_IR_MODE because of existence of art/SEA_IR_ART)
ART_SEA_IR_MODE := true
endif
ifeq ($(WITH_ART_SEA_IR_MODE), true)
ART_SEA_IR_MODE := true
endif

#
# Used to enable portable mode
#
ART_USE_PORTABLE_COMPILER := false
ifneq ($(wildcard art/USE_PORTABLE_COMPILER),)
$(info Enabling ART_USE_PORTABLE_COMPILER because of existence of art/USE_PORTABLE_COMPILER)
ART_USE_PORTABLE_COMPILER := true
endif
ifeq ($(WITH_ART_USE_PORTABLE_COMPILER),true)
$(info Enabling ART_USE_PORTABLE_COMPILER because WITH_ART_USE_PORTABLE_COMPILER=true)
ART_USE_PORTABLE_COMPILER := true
endif

LLVM_ROOT_PATH := external/llvm
include $(LLVM_ROOT_PATH)/llvm.mk

# Clang build.
# ART_TARGET_CLANG := true
# ART_HOST_CLANG := true

# directory used for gtests on device
ART_NATIVETEST_DIR := /data/nativetest/art
ART_NATIVETEST_OUT := $(TARGET_OUT_DATA_NATIVE_TESTS)/art

# directory used for tests on device
ART_TEST_DIR := /data/art-test
ART_TEST_OUT := $(TARGET_OUT_DATA)/art-test

ART_CPP_EXTENSION := .cc

ART_HOST_SHLIB_EXTENSION := $(HOST_SHLIB_SUFFIX)
ART_HOST_SHLIB_EXTENSION ?= .so

ART_C_INCLUDES := \
	external/gtest/include \
	external/valgrind/main/include \
	external/zlib \
	frameworks/compile/mclinker/include

art_cflags := \
	-fno-rtti \
	-std=gnu++11 \
	-ggdb3 \
	-Wall \
	-Werror \
	-Wextra \
	-Wstrict-aliasing=3 \
	-fstrict-aliasing

ifeq ($(ART_SMALL_MODE),true)
  art_cflags += -DART_SMALL_MODE=1
endif

ifeq ($(ART_SEA_IR_MODE),true)
  art_cflags += -DART_SEA_IR_MODE=1
endif

ifeq ($(HOST_OS),linux)
  art_non_debug_cflags := \
	-Wframe-larger-than=1728
endif

art_non_debug_cflags := \
        -O3

art_debug_cflags := \
	-O1 \
	-DDYNAMIC_ANNOTATIONS_ENABLED=1 \
	-UNDEBUG

# start of image reserved address space
IMG_HOST_BASE_ADDRESS   := 0x60000000

ifeq ($(TARGET_ARCH),mips)
IMG_TARGET_BASE_ADDRESS := 0x30000000
else
IMG_TARGET_BASE_ADDRESS := 0x60000000
endif

ART_HOST_CFLAGS := $(art_cflags) -DANDROID_SMP=1 -DART_BASE_ADDRESS=$(IMG_HOST_BASE_ADDRESS)

ifeq ($(TARGET_ARCH),x86)
ART_TARGET_CFLAGS += -msse2
endif

ART_TARGET_CFLAGS := $(art_cflags) -DART_TARGET -DART_BASE_ADDRESS=$(IMG_TARGET_BASE_ADDRESS)
ifeq ($(TARGET_CPU_SMP),true)
  ART_TARGET_CFLAGS += -DANDROID_SMP=1
else
  ART_TARGET_CFLAGS += -DANDROID_SMP=0
endif

# Enable thread-safety for GCC 4.6 on the target but not for GCC 4.7 where this feature was removed.
ifneq ($(filter 4.6 4.6.%, $(TARGET_GCC_VERSION)),)
  ART_TARGET_CFLAGS += -Wthread-safety
else
  # Warn if not using GCC 4.6 for target builds when not doing a top-level or 'mma' build.
  ifneq ($(ONE_SHOT_MAKEFILE),)
    # Enable target GCC 4.6 with: export TARGET_GCC_VERSION_EXP=4.6
    $(info Using target GCC $(TARGET_GCC_VERSION) disables thread-safety checks.)
  endif
endif
# We build with GCC 4.6 on the host.
ART_HOST_CFLAGS += -Wthread-safety

# Make host builds easier to debug and profile by not omitting the frame pointer.
ART_HOST_CFLAGS += -fno-omit-frame-pointer

# To use oprofile_android --callgraph, uncomment this and recompile with "mmm art -B -j16"
# ART_TARGET_CFLAGS += -fno-omit-frame-pointer -marm -mapcs

ART_HOST_NON_DEBUG_CFLAGS := $(art_non_debug_cflags)
ART_TARGET_NON_DEBUG_CFLAGS := $(art_non_debug_cflags)

# TODO: move -fkeep-inline-functions to art_debug_cflags when target gcc > 4.4 (and -lsupc++)
ART_HOST_DEBUG_CFLAGS := $(art_debug_cflags) -fkeep-inline-functions
ART_HOST_DEBUG_LDLIBS := -lsupc++

ifneq ($(HOST_OS),linux)
  # Some Mac OS pthread header files are broken with -fkeep-inline-functions.
  ART_HOST_DEBUG_CFLAGS := $(filter-out -fkeep-inline-functions,$(ART_HOST_DEBUG_CFLAGS))
  # Mac OS doesn't have libsupc++.
  ART_HOST_DEBUG_LDLIBS := $(filter-out -lsupc++,$(ART_HOST_DEBUG_LDLIBS))
endif

ART_TARGET_DEBUG_CFLAGS := $(art_debug_cflags)

ifeq ($(ART_USE_PORTABLE_COMPILER),true)
PARALLEL_ART_COMPILE_JOBS := -j8
endif

ART_BUILD_TARGET := false
ART_BUILD_HOST := false
ART_BUILD_NDEBUG := false
ART_BUILD_DEBUG := false
ifeq ($(ART_BUILD_TARGET_NDEBUG),true)
  ART_BUILD_TARGET := true
  ART_BUILD_NDEBUG := true
endif
ifeq ($(ART_BUILD_TARGET_DEBUG),true)
  ART_BUILD_TARGET := true
  ART_BUILD_DEBUG := true
endif
ifeq ($(ART_BUILD_HOST_NDEBUG),true)
  ART_BUILD_HOST := true
  ART_BUILD_NDEBUG := true
endif
ifeq ($(ART_BUILD_HOST_DEBUG),true)
  ART_BUILD_HOST := true
  ART_BUILD_DEBUG := true
endif

endif # ANDROID_COMMON_MK
