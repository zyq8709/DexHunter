/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "callee_save_frame.h"
#include "dex_instruction-inl.h"
#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"

namespace art {

// Determine target of interface dispatch. This object is known non-null.
extern "C" uint64_t artInvokeInterfaceTrampoline(mirror::ArtMethod* interface_method,
                                                 mirror::Object* this_object,
                                                 mirror::ArtMethod* caller_method,
                                                 Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtMethod* method;
  if (LIKELY(interface_method->GetDexMethodIndex() != DexFile::kDexNoIndex)) {
    method = this_object->GetClass()->FindVirtualMethodForInterface(interface_method);
    if (UNLIKELY(method == NULL)) {
      FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsAndArgs);
      ThrowIncompatibleClassChangeErrorClassForInterfaceDispatch(interface_method, this_object,
                                                                 caller_method);
      return 0;  // Failure.
    }
  } else {
    FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsAndArgs);
    DCHECK(interface_method == Runtime::Current()->GetResolutionMethod());
    // Determine method index from calling dex instruction.
#if defined(__arm__)
    // On entry the stack pointed by sp is:
    // | argN       |  |
    // | ...        |  |
    // | arg4       |  |
    // | arg3 spill |  |  Caller's frame
    // | arg2 spill |  |
    // | arg1 spill |  |
    // | Method*    | ---
    // | LR         |
    // | ...        |    callee saves
    // | R3         |    arg3
    // | R2         |    arg2
    // | R1         |    arg1
    // | R0         |
    // | Method*    |  <- sp
    DCHECK_EQ(48U, Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs)->GetFrameSizeInBytes());
    uintptr_t* regs = reinterpret_cast<uintptr_t*>(reinterpret_cast<byte*>(sp) + kPointerSize);
    uintptr_t caller_pc = regs[10];
#elif defined(__i386__)
    // On entry the stack pointed by sp is:
    // | argN        |  |
    // | ...         |  |
    // | arg4        |  |
    // | arg3 spill  |  |  Caller's frame
    // | arg2 spill  |  |
    // | arg1 spill  |  |
    // | Method*     | ---
    // | Return      |
    // | EBP,ESI,EDI |    callee saves
    // | EBX         |    arg3
    // | EDX         |    arg2
    // | ECX         |    arg1
    // | EAX/Method* |  <- sp
    DCHECK_EQ(32U, Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs)->GetFrameSizeInBytes());
    uintptr_t* regs = reinterpret_cast<uintptr_t*>(reinterpret_cast<byte*>(sp));
    uintptr_t caller_pc = regs[7];
#elif defined(__mips__)
    // On entry the stack pointed by sp is:
    // | argN       |  |
    // | ...        |  |
    // | arg4       |  |
    // | arg3 spill |  |  Caller's frame
    // | arg2 spill |  |
    // | arg1 spill |  |
    // | Method*    | ---
    // | RA         |
    // | ...        |    callee saves
    // | A3         |    arg3
    // | A2         |    arg2
    // | A1         |    arg1
    // | A0/Method* |  <- sp
    DCHECK_EQ(64U, Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs)->GetFrameSizeInBytes());
    uintptr_t* regs = reinterpret_cast<uintptr_t*>(reinterpret_cast<byte*>(sp));
    uintptr_t caller_pc = regs[15];
#else
    UNIMPLEMENTED(FATAL);
    uintptr_t caller_pc = 0;
#endif
    uint32_t dex_pc = caller_method->ToDexPc(caller_pc);
    const DexFile::CodeItem* code = MethodHelper(caller_method).GetCodeItem();
    CHECK_LT(dex_pc, code->insns_size_in_code_units_);
    const Instruction* instr = Instruction::At(&code->insns_[dex_pc]);
    Instruction::Code instr_code = instr->Opcode();
    CHECK(instr_code == Instruction::INVOKE_INTERFACE ||
          instr_code == Instruction::INVOKE_INTERFACE_RANGE)
        << "Unexpected call into interface trampoline: " << instr->DumpString(NULL);
    uint32_t dex_method_idx;
    if (instr_code == Instruction::INVOKE_INTERFACE) {
      dex_method_idx = instr->VRegB_35c();
    } else {
      DCHECK_EQ(instr_code, Instruction::INVOKE_INTERFACE_RANGE);
      dex_method_idx = instr->VRegB_3rc();
    }
    method = FindMethodFromCode(dex_method_idx, this_object, caller_method, self,
                                false, kInterface);
    if (UNLIKELY(method == NULL)) {
      CHECK(self->IsExceptionPending());
      return 0;  // Failure.
    }
  }
  const void* code = method->GetEntryPointFromCompiledCode();

#ifndef NDEBUG
  // When we return, the caller will branch to this address, so it had better not be 0!
  if (UNLIKELY(code == NULL)) {
      MethodHelper mh(method);
      LOG(FATAL) << "Code was NULL in method: " << PrettyMethod(method)
                 << " location: " << mh.GetDexFile().GetLocation();
  }
#endif

  uint32_t method_uint = reinterpret_cast<uint32_t>(method);
  uint64_t code_uint = reinterpret_cast<uint32_t>(code);
  uint64_t result = ((code_uint << 32) | method_uint);
  return result;
}


static uint64_t artInvokeCommon(uint32_t method_idx, mirror::Object* this_object,
                                mirror::ArtMethod* caller_method,
                                Thread* self, mirror::ArtMethod** sp, bool access_check,
                                InvokeType type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtMethod* method = FindMethodFast(method_idx, this_object, caller_method,
                                                  access_check, type);
  if (UNLIKELY(method == NULL)) {
    FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsAndArgs);
    method = FindMethodFromCode(method_idx, this_object, caller_method, self, access_check, type);
    if (UNLIKELY(method == NULL)) {
      CHECK(self->IsExceptionPending());
      return 0;  // failure
    }
  }
  DCHECK(!self->IsExceptionPending());
  const void* code = method->GetEntryPointFromCompiledCode();

#ifndef NDEBUG
  // When we return, the caller will branch to this address, so it had better not be 0!
  if (UNLIKELY(code == NULL)) {
      MethodHelper mh(method);
      LOG(FATAL) << "Code was NULL in method: " << PrettyMethod(method)
                 << " location: " << mh.GetDexFile().GetLocation();
  }
#endif

  uint32_t method_uint = reinterpret_cast<uint32_t>(method);
  uint64_t code_uint = reinterpret_cast<uint32_t>(code);
  uint64_t result = ((code_uint << 32) | method_uint);
  return result;
}

// See comments in runtime_support_asm.S
extern "C" uint64_t artInvokeInterfaceTrampolineWithAccessCheck(uint32_t method_idx,
                                                                mirror::Object* this_object,
                                                                mirror::ArtMethod* caller_method,
                                                                Thread* self,
                                                                mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon(method_idx, this_object, caller_method, self, sp, true, kInterface);
}


extern "C" uint64_t artInvokeDirectTrampolineWithAccessCheck(uint32_t method_idx,
                                                             mirror::Object* this_object,
                                                             mirror::ArtMethod* caller_method,
                                                             Thread* self,
                                                             mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon(method_idx, this_object, caller_method, self, sp, true, kDirect);
}

extern "C" uint64_t artInvokeStaticTrampolineWithAccessCheck(uint32_t method_idx,
                                                             mirror::Object* this_object,
                                                             mirror::ArtMethod* caller_method,
                                                             Thread* self,
                                                             mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon(method_idx, this_object, caller_method, self, sp, true, kStatic);
}

extern "C" uint64_t artInvokeSuperTrampolineWithAccessCheck(uint32_t method_idx,
                                                            mirror::Object* this_object,
                                                            mirror::ArtMethod* caller_method,
                                                            Thread* self,
                                                            mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon(method_idx, this_object, caller_method, self, sp, true, kSuper);
}

extern "C" uint64_t artInvokeVirtualTrampolineWithAccessCheck(uint32_t method_idx,
                                                              mirror::Object* this_object,
                                                              mirror::ArtMethod* caller_method,
                                                              Thread* self,
                                                              mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon(method_idx, this_object, caller_method, self, sp, true, kVirtual);
}

}  // namespace art
