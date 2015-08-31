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

LOCAL_PATH := $(call my-dir)

include art/build/Android.common.mk

LIBART_COMPILER_SRC_FILES := \
	compiled_method.cc \
	dex/local_value_numbering.cc \
	dex/arena_allocator.cc \
	dex/arena_bit_vector.cc \
	dex/quick/arm/assemble_arm.cc \
	dex/quick/arm/call_arm.cc \
	dex/quick/arm/fp_arm.cc \
	dex/quick/arm/int_arm.cc \
	dex/quick/arm/target_arm.cc \
	dex/quick/arm/utility_arm.cc \
	dex/quick/codegen_util.cc \
	dex/quick/gen_common.cc \
	dex/quick/gen_invoke.cc \
	dex/quick/gen_loadstore.cc \
	dex/quick/local_optimizations.cc \
	dex/quick/mips/assemble_mips.cc \
	dex/quick/mips/call_mips.cc \
	dex/quick/mips/fp_mips.cc \
	dex/quick/mips/int_mips.cc \
	dex/quick/mips/target_mips.cc \
	dex/quick/mips/utility_mips.cc \
	dex/quick/mir_to_lir.cc \
	dex/quick/ralloc_util.cc \
	dex/quick/x86/assemble_x86.cc \
	dex/quick/x86/call_x86.cc \
	dex/quick/x86/fp_x86.cc \
	dex/quick/x86/int_x86.cc \
	dex/quick/x86/target_x86.cc \
	dex/quick/x86/utility_x86.cc \
	dex/portable/mir_to_gbc.cc \
	dex/dex_to_dex_compiler.cc \
	dex/mir_dataflow.cc \
	dex/mir_optimization.cc \
	dex/frontend.cc \
	dex/mir_graph.cc \
	dex/mir_analysis.cc \
	dex/vreg_analysis.cc \
	dex/ssa_transformation.cc \
	driver/compiler_driver.cc \
	driver/dex_compilation_unit.cc \
	jni/portable/jni_compiler.cc \
	jni/quick/arm/calling_convention_arm.cc \
	jni/quick/mips/calling_convention_mips.cc \
	jni/quick/x86/calling_convention_x86.cc \
	jni/quick/calling_convention.cc \
	jni/quick/jni_compiler.cc \
	llvm/compiler_llvm.cc \
	llvm/gbc_expander.cc \
	llvm/generated/art_module.cc \
	llvm/intrinsic_helper.cc \
	llvm/ir_builder.cc \
	llvm/llvm_compilation_unit.cc \
	llvm/md_builder.cc \
	llvm/runtime_support_builder.cc \
	llvm/runtime_support_builder_arm.cc \
	llvm/runtime_support_builder_thumb2.cc \
	llvm/runtime_support_builder_x86.cc \
	trampolines/trampoline_compiler.cc \
	utils/arm/assembler_arm.cc \
	utils/arm/managed_register_arm.cc \
	utils/assembler.cc \
	utils/mips/assembler_mips.cc \
	utils/mips/managed_register_mips.cc \
	utils/x86/assembler_x86.cc \
	utils/x86/managed_register_x86.cc \
	buffered_output_stream.cc \
	elf_fixup.cc \
	elf_stripper.cc \
	elf_writer.cc \
	elf_writer_quick.cc \
	file_output_stream.cc \
	image_writer.cc \
	oat_writer.cc \
	vector_output_stream.cc

ifeq ($(ART_SEA_IR_MODE),true)
LIBART_COMPILER_SRC_FILES += \
	sea_ir/frontend.cc \
	sea_ir/ir/instruction_tools.cc \
	sea_ir/ir/sea.cc \
	sea_ir/code_gen/code_gen.cc \
	sea_ir/code_gen/code_gen_data.cc \
	sea_ir/types/type_inference.cc \
	sea_ir/types/type_inference_visitor.cc \
	sea_ir/debug/dot_gen.cc
endif

LIBART_COMPILER_CFLAGS :=
ifeq ($(ART_USE_PORTABLE_COMPILER),true)
  LIBART_COMPILER_SRC_FILES += elf_writer_mclinker.cc
  LIBART_COMPILER_CFLAGS += -DART_USE_PORTABLE_COMPILER=1
endif

LIBART_COMPILER_ENUM_OPERATOR_OUT_HEADER_FILES := \
	dex/compiler_enums.h

# $(1): target or host
# $(2): ndebug or debug
define build-libart-compiler
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
  else
    LOCAL_IS_HOST_MODULE := true
  endif
  LOCAL_CPP_EXTENSION := $(ART_CPP_EXTENSION)
  ifeq ($$(art_ndebug_or_debug),ndebug)
    LOCAL_MODULE := libart-compiler
  else # debug
    LOCAL_MODULE := libartd-compiler
  endif

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE_CLASS := SHARED_LIBRARIES

  LOCAL_SRC_FILES := $$(LIBART_COMPILER_SRC_FILES)

  GENERATED_SRC_DIR := $$(call intermediates-dir-for,$$(LOCAL_MODULE_CLASS),$$(LOCAL_MODULE),$$(LOCAL_IS_HOST_MODULE),)
  ENUM_OPERATOR_OUT_CC_FILES := $$(patsubst %.h,%_operator_out.cc,$$(LIBART_COMPILER_ENUM_OPERATOR_OUT_HEADER_FILES))
  ENUM_OPERATOR_OUT_GEN := $$(addprefix $$(GENERATED_SRC_DIR)/,$$(ENUM_OPERATOR_OUT_CC_FILES))

$$(ENUM_OPERATOR_OUT_GEN): art/tools/generate-operator-out.py
$$(ENUM_OPERATOR_OUT_GEN): PRIVATE_CUSTOM_TOOL = art/tools/generate-operator-out.py $(LOCAL_PATH) $$< > $$@
$$(ENUM_OPERATOR_OUT_GEN): $$(GENERATED_SRC_DIR)/%_operator_out.cc : $(LOCAL_PATH)/%.h
	$$(transform-generated-source)

  LOCAL_GENERATED_SOURCES += $$(ENUM_OPERATOR_OUT_GEN)

  LOCAL_CFLAGS := $$(LIBART_COMPILER_CFLAGS)
  ifeq ($$(art_target_or_host),target)
    LOCAL_CLANG := $(ART_TARGET_CLANG)
    LOCAL_CFLAGS += $(ART_TARGET_CFLAGS)
  else # host
    LOCAL_CLANG := $(ART_HOST_CLANG)
    LOCAL_CFLAGS += $(ART_HOST_CFLAGS)
  endif

  # TODO: clean up the compilers and remove this.
  LOCAL_CFLAGS += -Wno-unused-parameter

  LOCAL_SHARED_LIBRARIES += liblog
  ifeq ($$(art_ndebug_or_debug),debug)
    ifeq ($$(art_target_or_host),target)
      LOCAL_CFLAGS += $(ART_TARGET_DEBUG_CFLAGS)
    else # host
      LOCAL_CFLAGS += $(ART_HOST_DEBUG_CFLAGS)
    endif
    LOCAL_SHARED_LIBRARIES += libartd
  else
    ifeq ($$(art_target_or_host),target)
      LOCAL_CFLAGS += $(ART_TARGET_NON_DEBUG_CFLAGS)
    else # host
      LOCAL_CFLAGS += $(ART_HOST_NON_DEBUG_CFLAGS)
    endif
    LOCAL_SHARED_LIBRARIES += libart
  endif
  LOCAL_SHARED_LIBRARIES += libbcc libbcinfo libLLVM
  ifeq ($(ART_USE_PORTABLE_COMPILER),true)
    LOCAL_CFLAGS += -DART_USE_PORTABLE_COMPILER=1
  endif

  LOCAL_C_INCLUDES += $(ART_C_INCLUDES) art/runtime

  ifeq ($$(art_target_or_host),host)
    LOCAL_LDLIBS := -ldl -lpthread
  endif
  LOCAL_ADDITIONAL_DEPENDENCIES := art/build/Android.common.mk
  LOCAL_ADDITIONAL_DEPENDENCIES += $(LOCAL_PATH)/Android.mk
  ifeq ($$(art_target_or_host),target)
    LOCAL_SHARED_LIBRARIES += libcutils
    include $(LLVM_GEN_INTRINSICS_MK)
    include $(LLVM_DEVICE_BUILD_MK)
    include $(BUILD_SHARED_LIBRARY)
  else # host
    LOCAL_STATIC_LIBRARIES += libcutils
    include $(LLVM_GEN_INTRINSICS_MK)
    include $(LLVM_HOST_BUILD_MK)
    include $(BUILD_HOST_SHARED_LIBRARY)
  endif

  ifeq ($$(art_target_or_host),target)
    ifeq ($$(art_ndebug_or_debug),debug)
      $(TARGET_OUT_EXECUTABLES)/dex2oatd: $$(LOCAL_INSTALLED_MODULE)
    else
      $(TARGET_OUT_EXECUTABLES)/dex2oat: $$(LOCAL_INSTALLED_MODULE)
    endif
  else # host
    ifeq ($$(art_ndebug_or_debug),debug)
      $(HOST_OUT_EXECUTABLES)/dex2oatd: $$(LOCAL_INSTALLED_MODULE)
    else
      $(HOST_OUT_EXECUTABLES)/dex2oat: $$(LOCAL_INSTALLED_MODULE)
    endif
  endif

endef

ifeq ($(ART_BUILD_TARGET_NDEBUG),true)
  $(eval $(call build-libart-compiler,target,ndebug))
endif
ifeq ($(ART_BUILD_TARGET_DEBUG),true)
  $(eval $(call build-libart-compiler,target,debug))
endif
ifeq ($(WITH_HOST_DALVIK),true)
  # We always build dex2oat and dependencies, even if the host build is otherwise disabled, since they are used to cross compile for the target.
  ifeq ($(ART_BUILD_NDEBUG),true)
    $(eval $(call build-libart-compiler,host,ndebug))
  endif
  ifeq ($(ART_BUILD_DEBUG),true)
    $(eval $(call build-libart-compiler,host,debug))
  endif
endif

# Rule to build /system/lib/libcompiler_rt.a
# Usually static libraries are not installed on the device.
ifeq ($(ART_USE_PORTABLE_COMPILER),true)
ifeq ($(ART_BUILD_TARGET),true)
# TODO: Move to external/compiler_rt
$(eval $(call copy-one-file, $(call intermediates-dir-for,STATIC_LIBRARIES,libcompiler_rt,,)/libcompiler_rt.a, $(TARGET_OUT_SHARED_LIBRARIES)/libcompiler_rt.a))

$(DEX2OAT): $(TARGET_OUT_SHARED_LIBRARIES)/libcompiler_rt.a

endif
endif
