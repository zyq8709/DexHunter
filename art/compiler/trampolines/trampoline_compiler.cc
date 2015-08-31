/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "trampoline_compiler.h"

#include "jni_internal.h"
#include "utils/arm/assembler_arm.h"
#include "utils/mips/assembler_mips.h"
#include "utils/x86/assembler_x86.h"

#define __ assembler->

namespace art {

namespace arm {
static const std::vector<uint8_t>* CreateTrampoline(EntryPointCallingConvention abi,
                                                    ThreadOffset offset) {
  UniquePtr<ArmAssembler> assembler(static_cast<ArmAssembler*>(Assembler::Create(kArm)));

  switch (abi) {
    case kInterpreterAbi:  // Thread* is first argument (R0) in interpreter ABI.
      __ LoadFromOffset(kLoadWord, PC, R0, offset.Int32Value());
      break;
    case kJniAbi:  // Load via Thread* held in JNIEnv* in first argument (R0).
      __ LoadFromOffset(kLoadWord, IP, R0, JNIEnvExt::SelfOffset().Int32Value());
      __ LoadFromOffset(kLoadWord, PC, IP, offset.Int32Value());
      break;
    case kPortableAbi:  // R9 holds Thread*.
    case kQuickAbi:  // Fall-through.
      __ LoadFromOffset(kLoadWord, PC, R9, offset.Int32Value());
  }
  __ bkpt(0);

  size_t cs = assembler->CodeSize();
  UniquePtr<std::vector<uint8_t> > entry_stub(new std::vector<uint8_t>(cs));
  MemoryRegion code(&(*entry_stub)[0], entry_stub->size());
  assembler->FinalizeInstructions(code);

  return entry_stub.release();
}
}  // namespace arm

namespace mips {
static const std::vector<uint8_t>* CreateTrampoline(EntryPointCallingConvention abi,
                                                    ThreadOffset offset) {
  UniquePtr<MipsAssembler> assembler(static_cast<MipsAssembler*>(Assembler::Create(kMips)));

  switch (abi) {
    case kInterpreterAbi:  // Thread* is first argument (A0) in interpreter ABI.
      __ LoadFromOffset(kLoadWord, T9, A0, offset.Int32Value());
      break;
    case kJniAbi:  // Load via Thread* held in JNIEnv* in first argument (A0).
      __ LoadFromOffset(kLoadWord, T9, A0, JNIEnvExt::SelfOffset().Int32Value());
      __ LoadFromOffset(kLoadWord, T9, T9, offset.Int32Value());
      break;
    case kPortableAbi:  // S1 holds Thread*.
    case kQuickAbi:  // Fall-through.
      __ LoadFromOffset(kLoadWord, T9, S1, offset.Int32Value());
  }
  __ Jr(T9);
  __ Nop();
  __ Break();

  size_t cs = assembler->CodeSize();
  UniquePtr<std::vector<uint8_t> > entry_stub(new std::vector<uint8_t>(cs));
  MemoryRegion code(&(*entry_stub)[0], entry_stub->size());
  assembler->FinalizeInstructions(code);

  return entry_stub.release();
}
}  // namespace mips

namespace x86 {
static const std::vector<uint8_t>* CreateTrampoline(ThreadOffset offset) {
  UniquePtr<X86Assembler> assembler(static_cast<X86Assembler*>(Assembler::Create(kX86)));

  // All x86 trampolines call via the Thread* held in fs.
  __ fs()->jmp(Address::Absolute(offset));
  __ int3();

  size_t cs = assembler->CodeSize();
  UniquePtr<std::vector<uint8_t> > entry_stub(new std::vector<uint8_t>(cs));
  MemoryRegion code(&(*entry_stub)[0], entry_stub->size());
  assembler->FinalizeInstructions(code);

  return entry_stub.release();
}
}  // namespace x86

const std::vector<uint8_t>* CreateTrampoline(InstructionSet isa, EntryPointCallingConvention abi,
                                             ThreadOffset offset) {
  switch (isa) {
    case kArm:
    case kThumb2:
      return arm::CreateTrampoline(abi, offset);
    case kMips:
      return mips::CreateTrampoline(abi, offset);
    case kX86:
      return x86::CreateTrampoline(offset);
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << isa;
      return NULL;
  }
}

}  // namespace art
