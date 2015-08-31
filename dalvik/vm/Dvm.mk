# Copyright (C) 2008 The Android Open Source Project
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
# Common definitions for host or target builds of libdvm.
#
# If you enable or disable optional features here, make sure you do
# a "clean" build -- not everything depends on Dalvik.h.  (See Android.mk
# for the exact command.)
#


#
# Compiler defines.
#

LOCAL_CFLAGS += -fstrict-aliasing -Wstrict-aliasing=2
LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_CFLAGS += -DARCH_VARIANT=\"$(dvm_arch_variant)\"

ifneq ($(strip $(LOCAL_CLANG)),true)
LOCAL_CFLAGS += -fno-align-jumps
endif

#
# Optional features.  These may impact the size or performance of the VM.
#

# Make a debugging version when building the simulator (if not told
# otherwise) and when explicitly asked.
dvm_make_debug_vm := false
ifneq ($(strip $(DEBUG_DALVIK_VM)),)
  dvm_make_debug_vm := $(DEBUG_DALVIK_VM)
endif

ifeq ($(dvm_make_debug_vm),true)
  #
  # "Debug" profile:
  # - debugger enabled
  # - profiling enabled
  # - tracked-reference verification enabled
  # - allocation limits enabled
  # - GDB helpers enabled
  # - LOGV
  # - assert()
  #
  LOCAL_CFLAGS += -DWITH_INSTR_CHECKS
  LOCAL_CFLAGS += -DWITH_EXTRA_OBJECT_VALIDATION
  LOCAL_CFLAGS += -DWITH_TRACKREF_CHECKS
  LOCAL_CFLAGS += -DWITH_EXTRA_GC_CHECKS=1
  #LOCAL_CFLAGS += -DCHECK_MUTEX
  LOCAL_CFLAGS += -DDVM_SHOW_EXCEPTION=3
  # add some extra stuff to make it easier to examine with GDB
  LOCAL_CFLAGS += -DEASY_GDB
  # overall config may be for a "release" build, so reconfigure these
  LOCAL_CFLAGS += -UNDEBUG -DDEBUG=1 -DLOG_NDEBUG=1 -DWITH_DALVIK_ASSERT
else  # !dvm_make_debug_vm
  #
  # "Performance" profile:
  # - all development features disabled
  # - compiler optimizations enabled (redundant for "release" builds)
  # - (debugging and profiling still enabled)
  #
  #LOCAL_CFLAGS += -DNDEBUG -DLOG_NDEBUG=1
  # "-O2" is redundant for device (release) but useful for sim (debug)
  #LOCAL_CFLAGS += -O2 -Winline
  #LOCAL_CFLAGS += -DWITH_EXTRA_OBJECT_VALIDATION
  LOCAL_CFLAGS += -DDVM_SHOW_EXCEPTION=1
  # if you want to try with assertions on the device, add:
  #LOCAL_CFLAGS += -UNDEBUG -DDEBUG=1 -DLOG_NDEBUG=1 -DWITH_DALVIK_ASSERT
endif  # !dvm_make_debug_vm

# bug hunting: checksum and verify interpreted stack when making JNI calls
#LOCAL_CFLAGS += -DWITH_JNI_STACK_CHECK

LOCAL_SRC_FILES := \
	AllocTracker.cpp \
	Atomic.cpp.arm \
	AtomicCache.cpp \
	BitVector.cpp.arm \
	CheckJni.cpp \
	Ddm.cpp \
	Debugger.cpp \
	DvmDex.cpp \
	Exception.cpp \
	Hash.cpp \
	IndirectRefTable.cpp.arm \
	Init.cpp \
	InitRefs.cpp \
	InlineNative.cpp.arm \
	Inlines.cpp \
	Intern.cpp \
	Jni.cpp \
	JarFile.cpp \
	LinearAlloc.cpp \
	Misc.cpp \
	Native.cpp \
	PointerSet.cpp \
	Profile.cpp \
	RawDexFile.cpp \
	ReferenceTable.cpp \
	SignalCatcher.cpp \
	StdioConverter.cpp \
	Sync.cpp \
	Thread.cpp \
	UtfString.cpp \
	alloc/Alloc.cpp \
	alloc/CardTable.cpp \
	alloc/HeapBitmap.cpp.arm \
	alloc/HeapDebug.cpp \
	alloc/Heap.cpp.arm \
	alloc/DdmHeap.cpp \
	alloc/Verify.cpp \
	alloc/Visit.cpp \
	analysis/CodeVerify.cpp \
	analysis/DexPrepare.cpp \
	analysis/DexVerify.cpp \
	analysis/Liveness.cpp \
	analysis/Optimize.cpp \
	analysis/RegisterMap.cpp \
	analysis/VerifySubs.cpp \
	analysis/VfyBasicBlock.cpp \
	hprof/Hprof.cpp \
	hprof/HprofClass.cpp \
	hprof/HprofHeap.cpp \
	hprof/HprofOutput.cpp \
	hprof/HprofString.cpp \
	interp/Interp.cpp.arm \
	interp/Stack.cpp \
	jdwp/ExpandBuf.cpp \
	jdwp/JdwpAdb.cpp \
	jdwp/JdwpConstants.cpp \
	jdwp/JdwpEvent.cpp \
	jdwp/JdwpHandler.cpp \
	jdwp/JdwpMain.cpp \
	jdwp/JdwpSocket.cpp \
	mterp/Mterp.cpp.arm \
	mterp/out/InterpC-portable.cpp.arm \
	native/InternalNative.cpp \
	native/dalvik_bytecode_OpcodeInfo.cpp \
	native/dalvik_system_DexFile.cpp \
	native/dalvik_system_VMDebug.cpp \
	native/dalvik_system_VMRuntime.cpp \
	native/dalvik_system_VMStack.cpp \
	native/dalvik_system_Zygote.cpp \
	native/java_lang_Class.cpp \
	native/java_lang_Double.cpp \
	native/java_lang_Float.cpp \
	native/java_lang_Math.cpp \
	native/java_lang_Object.cpp \
	native/java_lang_Runtime.cpp \
	native/java_lang_String.cpp \
	native/java_lang_System.cpp \
	native/java_lang_Throwable.cpp \
	native/java_lang_VMClassLoader.cpp \
	native/java_lang_VMThread.cpp \
	native/java_lang_reflect_AccessibleObject.cpp \
	native/java_lang_reflect_Array.cpp \
	native/java_lang_reflect_Constructor.cpp \
	native/java_lang_reflect_Field.cpp \
	native/java_lang_reflect_Method.cpp \
	native/java_lang_reflect_Proxy.cpp \
	native/java_util_concurrent_atomic_AtomicLong.cpp \
	native/org_apache_harmony_dalvik_NativeTestTarget.cpp \
	native/org_apache_harmony_dalvik_ddmc_DdmServer.cpp \
	native/org_apache_harmony_dalvik_ddmc_DdmVmInternal.cpp \
	native/sun_misc_Unsafe.cpp \
	oo/AccessCheck.cpp \
	oo/Array.cpp \
	oo/Class.cpp \
	oo/Object.cpp \
	oo/Resolve.cpp \
	oo/TypeCheck.cpp \
	reflect/Annotation.cpp \
	reflect/Proxy.cpp \
	reflect/Reflect.cpp \
	test/AtomicTest.cpp.arm \
	test/TestHash.cpp \
	test/TestIndirectRefTable.cpp

# TODO: this is the wrong test, but what's the right one?
ifneq ($(filter arm mips,$(dvm_arch)),)
  LOCAL_SRC_FILES += os/android.cpp
else
  LOCAL_SRC_FILES += os/linux.cpp
endif

WITH_COPYING_GC := $(strip $(WITH_COPYING_GC))

ifeq ($(WITH_COPYING_GC),true)
  LOCAL_CFLAGS += -DWITH_COPYING_GC
  LOCAL_SRC_FILES += \
	alloc/Copying.cpp.arm
else
  LOCAL_SRC_FILES += \
	alloc/DlMalloc.cpp \
	alloc/HeapSource.cpp \
	alloc/MarkSweep.cpp.arm
endif

WITH_JIT := $(strip $(WITH_JIT))

ifeq ($(WITH_JIT),true)
  LOCAL_CFLAGS += -DWITH_JIT
  LOCAL_SRC_FILES += \
	compiler/Compiler.cpp \
	compiler/Frontend.cpp \
	compiler/Utility.cpp \
	compiler/InlineTransformation.cpp \
	compiler/IntermediateRep.cpp \
	compiler/Dataflow.cpp \
	compiler/SSATransformation.cpp \
	compiler/Loop.cpp \
	compiler/Ralloc.cpp \
	interp/Jit.cpp
endif

LOCAL_C_INCLUDES += \
	dalvik \
	dalvik/vm \
	external/zlib \
	libcore/include \

MTERP_ARCH_KNOWN := false

ifeq ($(dvm_arch),arm)
  #dvm_arch_variant := armv7-a
  #LOCAL_CFLAGS += -march=armv7-a -mfloat-abi=softfp -mfpu=vfp
  LOCAL_CFLAGS += -Werror
  MTERP_ARCH_KNOWN := true
  # Select architecture-specific sources (armv5te, armv7-a, etc.)
  LOCAL_SRC_FILES += \
		arch/arm/CallOldABI.S \
		arch/arm/CallEABI.S \
		arch/arm/HintsEABI.cpp \
		mterp/out/InterpC-$(dvm_arch_variant).cpp.arm \
		mterp/out/InterpAsm-$(dvm_arch_variant).S

  ifeq ($(WITH_JIT),true)
    LOCAL_SRC_FILES += \
		compiler/codegen/RallocUtil.cpp \
		compiler/codegen/arm/$(dvm_arch_variant)/Codegen.cpp \
		compiler/codegen/arm/$(dvm_arch_variant)/CallingConvention.S \
		compiler/codegen/arm/Assemble.cpp \
		compiler/codegen/arm/ArchUtility.cpp \
		compiler/codegen/arm/LocalOptimizations.cpp \
		compiler/codegen/arm/GlobalOptimizations.cpp \
		compiler/codegen/arm/ArmRallocUtil.cpp \
		compiler/template/out/CompilerTemplateAsm-$(dvm_arch_variant).S
  endif
endif

ifeq ($(dvm_arch),mips)
  MTERP_ARCH_KNOWN := true
  LOCAL_C_INCLUDES += external/libffi/$(TARGET_OS)-$(TARGET_ARCH)
  LOCAL_SHARED_LIBRARIES += libffi
  LOCAL_SRC_FILES += \
		arch/mips/CallO32.S \
		arch/mips/HintsO32.cpp \
		arch/generic/Call.cpp \
		mterp/out/InterpC-mips.cpp \
		mterp/out/InterpAsm-mips.S

  ifeq ($(WITH_JIT),true)
    dvm_arch_variant := mips
    LOCAL_SRC_FILES += \
		compiler/codegen/mips/RallocUtil.cpp \
		compiler/codegen/mips/$(dvm_arch_variant)/Codegen.cpp \
		compiler/codegen/mips/$(dvm_arch_variant)/CallingConvention.S \
		compiler/codegen/mips/Assemble.cpp \
		compiler/codegen/mips/ArchUtility.cpp \
		compiler/codegen/mips/LocalOptimizations.cpp \
		compiler/codegen/mips/GlobalOptimizations.cpp \
		compiler/template/out/CompilerTemplateAsm-$(dvm_arch_variant).S
  endif
endif

ifeq ($(dvm_arch),x86)
  ifeq ($(dvm_os),linux)
    MTERP_ARCH_KNOWN := true
    LOCAL_CFLAGS += -DDVM_JMP_TABLE_MTERP=1 \
                    -DMTERP_STUB
    LOCAL_SRC_FILES += \
		arch/$(dvm_arch_variant)/Call386ABI.S \
		arch/$(dvm_arch_variant)/Hints386ABI.cpp \
		mterp/out/InterpC-$(dvm_arch_variant).cpp \
		mterp/out/InterpAsm-$(dvm_arch_variant).S
    ifeq ($(WITH_JIT),true)
      LOCAL_CFLAGS += -DARCH_IA32
      LOCAL_SRC_FILES += \
                compiler/codegen/x86/LowerAlu.cpp \
                compiler/codegen/x86/LowerConst.cpp \
                compiler/codegen/x86/LowerMove.cpp \
                compiler/codegen/x86/Lower.cpp \
                compiler/codegen/x86/LowerHelper.cpp \
                compiler/codegen/x86/LowerJump.cpp \
                compiler/codegen/x86/LowerObject.cpp \
                compiler/codegen/x86/AnalysisO1.cpp \
                compiler/codegen/x86/BytecodeVisitor.cpp \
                compiler/codegen/x86/NcgAot.cpp \
                compiler/codegen/x86/CodegenInterface.cpp \
                compiler/codegen/x86/LowerInvoke.cpp \
                compiler/codegen/x86/LowerReturn.cpp \
                compiler/codegen/x86/NcgHelper.cpp \
                compiler/codegen/x86/LowerGetPut.cpp

      # need apache harmony x86 encoder/decoder
      LOCAL_C_INCLUDES += \
                dalvik/vm/compiler/codegen/x86/libenc
      LOCAL_SRC_FILES += \
                compiler/codegen/x86/libenc/enc_base.cpp \
                compiler/codegen/x86/libenc/dec_base.cpp \
                compiler/codegen/x86/libenc/enc_wrapper.cpp \
                compiler/codegen/x86/libenc/enc_tabl.cpp

    endif
  endif
endif

ifeq ($(MTERP_ARCH_KNOWN),false)
  # unknown architecture, try to use FFI
  LOCAL_C_INCLUDES += external/libffi/$(dvm_os)-$(dvm_arch)

  ifeq ($(dvm_os)-$(dvm_arch),darwin-x86)
      # OSX includes libffi, so just make the linker aware of it directly.
      LOCAL_LDLIBS += -lffi
  else
      LOCAL_SHARED_LIBRARIES += libffi
  endif

  LOCAL_SRC_FILES += \
		arch/generic/Call.cpp \
		arch/generic/Hints.cpp \
		mterp/out/InterpC-allstubs.cpp

  # The following symbols are usually defined in the asm file, but
  # since we don't have an asm file in this case, we instead just
  # peg them at 0 here, and we add an #ifdef'able define for good
  # measure, too.
  LOCAL_CFLAGS += -DdvmAsmInstructionStart=0 -DdvmAsmInstructionEnd=0 \
	-DdvmAsmSisterStart=0 -DdvmAsmSisterEnd=0 -DDVM_NO_ASM_INTERP=1
endif
