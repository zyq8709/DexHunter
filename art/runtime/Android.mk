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

include art/build/Android.common.mk

LIBART_COMMON_SRC_FILES := \
	atomic.cc.arm \
	barrier.cc \
	base/logging.cc \
	base/mutex.cc \
	base/stringpiece.cc \
	base/stringprintf.cc \
	base/timing_logger.cc \
	base/unix_file/fd_file.cc \
	base/unix_file/mapped_file.cc \
	base/unix_file/null_file.cc \
	base/unix_file/random_access_file_utils.cc \
	base/unix_file/string_file.cc \
	check_jni.cc \
	class_linker.cc \
	common_throws.cc \
	debugger.cc \
	dex_file.cc \
	dex_file_verifier.cc \
	dex_instruction.cc \
	disassembler.cc \
	disassembler_arm.cc \
	disassembler_mips.cc \
	disassembler_x86.cc \
	elf_file.cc \
	gc/allocator/dlmalloc.cc \
	gc/accounting/card_table.cc \
	gc/accounting/gc_allocator.cc \
	gc/accounting/heap_bitmap.cc \
	gc/accounting/mod_union_table.cc \
	gc/accounting/space_bitmap.cc \
	gc/collector/garbage_collector.cc \
	gc/collector/mark_sweep.cc \
	gc/collector/partial_mark_sweep.cc \
	gc/collector/sticky_mark_sweep.cc \
	gc/heap.cc \
	gc/space/dlmalloc_space.cc \
	gc/space/image_space.cc \
	gc/space/large_object_space.cc \
	gc/space/space.cc \
	hprof/hprof.cc \
	image.cc \
	indirect_reference_table.cc \
	instrumentation.cc \
	intern_table.cc \
	interpreter/interpreter.cc \
	jdwp/jdwp_event.cc \
	jdwp/jdwp_expand_buf.cc \
	jdwp/jdwp_handler.cc \
	jdwp/jdwp_main.cc \
	jdwp/jdwp_request.cc \
	jdwp/jdwp_socket.cc \
	jdwp/object_registry.cc \
	jni_internal.cc \
	jobject_comparator.cc \
	locks.cc \
	mem_map.cc \
	memory_region.cc \
	mirror/art_field.cc \
	mirror/art_method.cc \
	mirror/array.cc \
	mirror/class.cc \
	mirror/dex_cache.cc \
	mirror/object.cc \
	mirror/stack_trace_element.cc \
	mirror/string.cc \
	mirror/throwable.cc \
	monitor.cc \
	native/dalvik_system_DexFile.cc \
	native/dalvik_system_VMDebug.cc \
	native/dalvik_system_VMRuntime.cc \
	native/dalvik_system_VMStack.cc \
	native/dalvik_system_Zygote.cc \
	native/java_lang_Class.cc \
	native/java_lang_DexCache.cc \
	native/java_lang_Object.cc \
	native/java_lang_Runtime.cc \
	native/java_lang_String.cc \
	native/java_lang_System.cc \
	native/java_lang_Thread.cc \
	native/java_lang_Throwable.cc \
	native/java_lang_VMClassLoader.cc \
	native/java_lang_reflect_Array.cc \
	native/java_lang_reflect_Constructor.cc \
	native/java_lang_reflect_Field.cc \
	native/java_lang_reflect_Method.cc \
	native/java_lang_reflect_Proxy.cc \
	native/java_util_concurrent_atomic_AtomicLong.cc \
	native/org_apache_harmony_dalvik_ddmc_DdmServer.cc \
	native/org_apache_harmony_dalvik_ddmc_DdmVmInternal.cc \
	native/sun_misc_Unsafe.cc \
	oat.cc \
	oat_file.cc \
	offsets.cc \
	os_linux.cc \
	primitive.cc \
	reference_table.cc \
	reflection.cc \
	runtime.cc \
	signal_catcher.cc \
	stack.cc \
	thread.cc \
	thread_list.cc \
	thread_pool.cc \
	throw_location.cc \
	trace.cc \
	utf.cc \
	utils.cc \
	verifier/dex_gc_map.cc \
	verifier/instruction_flags.cc \
	verifier/method_verifier.cc \
	verifier/reg_type.cc \
	verifier/reg_type_cache.cc \
	verifier/register_line.cc \
	well_known_classes.cc \
	zip_archive.cc

LIBART_COMMON_SRC_FILES += \
	arch/context.cc \
	arch/arm/registers_arm.cc \
	arch/x86/registers_x86.cc \
	arch/mips/registers_mips.cc \
	entrypoints/entrypoint_utils.cc \
	entrypoints/interpreter/interpreter_entrypoints.cc \
	entrypoints/jni/jni_entrypoints.cc \
	entrypoints/math_entrypoints.cc \
	entrypoints/portable/portable_alloc_entrypoints.cc \
	entrypoints/portable/portable_cast_entrypoints.cc \
	entrypoints/portable/portable_dexcache_entrypoints.cc \
	entrypoints/portable/portable_field_entrypoints.cc \
	entrypoints/portable/portable_fillarray_entrypoints.cc \
	entrypoints/portable/portable_invoke_entrypoints.cc \
	entrypoints/portable/portable_jni_entrypoints.cc \
	entrypoints/portable/portable_lock_entrypoints.cc \
	entrypoints/portable/portable_thread_entrypoints.cc \
	entrypoints/portable/portable_throw_entrypoints.cc \
	entrypoints/portable/portable_trampoline_entrypoints.cc \
	entrypoints/quick/quick_alloc_entrypoints.cc \
	entrypoints/quick/quick_cast_entrypoints.cc \
	entrypoints/quick/quick_deoptimization_entrypoints.cc \
	entrypoints/quick/quick_dexcache_entrypoints.cc \
	entrypoints/quick/quick_field_entrypoints.cc \
	entrypoints/quick/quick_fillarray_entrypoints.cc \
	entrypoints/quick/quick_instrumentation_entrypoints.cc \
	entrypoints/quick/quick_invoke_entrypoints.cc \
	entrypoints/quick/quick_jni_entrypoints.cc \
	entrypoints/quick/quick_lock_entrypoints.cc \
	entrypoints/quick/quick_math_entrypoints.cc \
	entrypoints/quick/quick_thread_entrypoints.cc \
	entrypoints/quick/quick_throw_entrypoints.cc \
	entrypoints/quick/quick_trampoline_entrypoints.cc

LIBART_TARGET_SRC_FILES := \
	$(LIBART_COMMON_SRC_FILES) \
	base/logging_android.cc \
	jdwp/jdwp_adb.cc \
	monitor_android.cc \
	runtime_android.cc \
	thread_android.cc

ifeq ($(TARGET_ARCH),arm)
LIBART_TARGET_SRC_FILES += \
	arch/arm/context_arm.cc.arm \
	arch/arm/entrypoints_init_arm.cc \
	arch/arm/jni_entrypoints_arm.S \
	arch/arm/portable_entrypoints_arm.S \
	arch/arm/quick_entrypoints_arm.S \
	arch/arm/thread_arm.cc
else # TARGET_ARCH != arm
ifeq ($(TARGET_ARCH),x86)
LIBART_TARGET_SRC_FILES += \
	arch/x86/context_x86.cc \
	arch/x86/entrypoints_init_x86.cc \
	arch/x86/jni_entrypoints_x86.S \
	arch/x86/portable_entrypoints_x86.S \
	arch/x86/quick_entrypoints_x86.S \
	arch/x86/thread_x86.cc
else # TARGET_ARCH != x86
ifeq ($(TARGET_ARCH),mips)
LIBART_TARGET_SRC_FILES += \
	arch/mips/context_mips.cc \
	arch/mips/entrypoints_init_mips.cc \
	arch/mips/jni_entrypoints_mips.S \
	arch/mips/portable_entrypoints_mips.S \
	arch/mips/quick_entrypoints_mips.S \
	arch/mips/thread_mips.cc
else # TARGET_ARCH != mips
$(error unsupported TARGET_ARCH=$(TARGET_ARCH))
endif # TARGET_ARCH != mips
endif # TARGET_ARCH != x86
endif # TARGET_ARCH != arm


LIBART_HOST_SRC_FILES := \
	$(LIBART_COMMON_SRC_FILES) \
	base/logging_linux.cc \
	monitor_linux.cc \
	runtime_linux.cc \
	thread_linux.cc

ifeq ($(HOST_ARCH),x86)
LIBART_HOST_SRC_FILES += \
	arch/x86/context_x86.cc \
	arch/x86/entrypoints_init_x86.cc \
	arch/x86/jni_entrypoints_x86.S \
	arch/x86/portable_entrypoints_x86.S \
	arch/x86/quick_entrypoints_x86.S \
	arch/x86/thread_x86.cc
else # HOST_ARCH != x86
$(error unsupported HOST_ARCH=$(HOST_ARCH))
endif # HOST_ARCH != x86


LIBART_ENUM_OPERATOR_OUT_HEADER_FILES := \
	base/mutex.h \
	dex_file.h \
	dex_instruction.h \
	gc/collector/gc_type.h \
	gc/space/space.h \
	gc/heap.h \
	indirect_reference_table.h \
	instruction_set.h \
	invoke_type.h \
	jdwp/jdwp.h \
	jdwp/jdwp_constants.h \
	locks.h \
	mirror/class.h \
	thread.h \
	thread_state.h \
	verifier/method_verifier.h

LIBART_CFLAGS :=
ifeq ($(ART_USE_PORTABLE_COMPILER),true)
  LIBART_CFLAGS += -DART_USE_PORTABLE_COMPILER=1
endif

# $(1): target or host
# $(2): ndebug or debug
define build-libart
  ifneq ($(1),target)
    ifneq ($(1),host)
      $$(error expected target or host for argument 1, received $(1))
    endif
  endif
  ifneq ($(2),ndebug)
    ifneq ($(2),debug)
      $$(error expected ndebug or debug for argument 2, received $(2))
    endif
  endif

  art_target_or_host := $(1)
  art_ndebug_or_debug := $(2)

  include $(CLEAR_VARS)
  ifeq ($$(art_target_or_host),target)
    include external/stlport/libstlport.mk
  endif
  LOCAL_CPP_EXTENSION := $(ART_CPP_EXTENSION)
  ifeq ($$(art_ndebug_or_debug),ndebug)
    LOCAL_MODULE := libart
  else # debug
    LOCAL_MODULE := libartd
  endif

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE_CLASS := SHARED_LIBRARIES

  ifeq ($$(art_target_or_host),target)
    LOCAL_SRC_FILES := $(LIBART_TARGET_SRC_FILES)
  else # host
    LOCAL_SRC_FILES := $(LIBART_HOST_SRC_FILES)
    LOCAL_IS_HOST_MODULE := true
  endif

  GENERATED_SRC_DIR := $$(call intermediates-dir-for,$$(LOCAL_MODULE_CLASS),$$(LOCAL_MODULE),$$(LOCAL_IS_HOST_MODULE),)
  ENUM_OPERATOR_OUT_CC_FILES := $$(patsubst %.h,%_operator_out.cc,$$(LIBART_ENUM_OPERATOR_OUT_HEADER_FILES))
  ENUM_OPERATOR_OUT_GEN := $$(addprefix $$(GENERATED_SRC_DIR)/,$$(ENUM_OPERATOR_OUT_CC_FILES))

$$(ENUM_OPERATOR_OUT_GEN): art/tools/generate-operator-out.py
$$(ENUM_OPERATOR_OUT_GEN): PRIVATE_CUSTOM_TOOL = art/tools/generate-operator-out.py $(LOCAL_PATH) $$< > $$@
$$(ENUM_OPERATOR_OUT_GEN): $$(GENERATED_SRC_DIR)/%_operator_out.cc : $(LOCAL_PATH)/%.h
	$$(transform-generated-source)

  LOCAL_GENERATED_SOURCES += $$(ENUM_OPERATOR_OUT_GEN)

  LOCAL_CFLAGS := $(LIBART_CFLAGS)
  ifeq ($$(art_target_or_host),target)
    LOCAL_CLANG := $(ART_TARGET_CLANG)
    LOCAL_CFLAGS += $(ART_TARGET_CFLAGS)
  else # host
    LOCAL_CLANG := $(ART_HOST_CLANG)
    LOCAL_CFLAGS += $(ART_HOST_CFLAGS)
  endif
  ifeq ($$(art_ndebug_or_debug),debug)
    ifeq ($$(art_target_or_host),target)
      LOCAL_CFLAGS += $(ART_TARGET_DEBUG_CFLAGS)
    else # host
      LOCAL_CFLAGS += $(ART_HOST_DEBUG_CFLAGS)
      LOCAL_LDLIBS += $(ART_HOST_DEBUG_LDLIBS)
      LOCAL_STATIC_LIBRARIES := libgtest_host
    endif
  else
    ifeq ($$(art_target_or_host),target)
      LOCAL_CFLAGS += $(ART_TARGET_NON_DEBUG_CFLAGS)
    else # host
      LOCAL_CFLAGS += $(ART_HOST_NON_DEBUG_CFLAGS)
    endif
  endif
  LOCAL_C_INCLUDES += $(ART_C_INCLUDES)
  LOCAL_SHARED_LIBRARIES += liblog libnativehelper
  LOCAL_SHARED_LIBRARIES += libcorkscrew # native stack trace support
  ifeq ($$(art_target_or_host),target)
    LOCAL_SHARED_LIBRARIES += libcutils libz libdl libselinux
  else # host
    LOCAL_STATIC_LIBRARIES += libcutils
    LOCAL_SHARED_LIBRARIES += libz-host
    LOCAL_LDLIBS += -ldl -lpthread
    ifeq ($(HOST_OS),linux)
      LOCAL_LDLIBS += -lrt
    endif
  endif
  include $(LLVM_GEN_INTRINSICS_MK)
  LOCAL_ADDITIONAL_DEPENDENCIES := art/build/Android.common.mk
  LOCAL_ADDITIONAL_DEPENDENCIES += $(LOCAL_PATH)/Android.mk
  ifeq ($$(art_target_or_host),target)
    include $(LLVM_DEVICE_BUILD_MK)
    include $(BUILD_SHARED_LIBRARY)
  else # host
    include $(LLVM_HOST_BUILD_MK)
    include $(BUILD_HOST_SHARED_LIBRARY)
  endif
endef

ifeq ($(ART_BUILD_TARGET_NDEBUG),true)
  $(eval $(call build-libart,target,ndebug))
endif
ifeq ($(ART_BUILD_TARGET_DEBUG),true)
  $(eval $(call build-libart,target,debug))
endif

# We always build dex2oat and dependencies, even if the host build is otherwise disabled, since they are used to cross compile for the target.
ifeq ($(WITH_HOST_DALVIK),true)
  ifeq ($(ART_BUILD_NDEBUG),true)
    $(eval $(call build-libart,host,ndebug))
  endif
  ifeq ($(ART_BUILD_DEBUG),true)
    $(eval $(call build-libart,host,debug))
  endif
endif
