/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "calling_convention_x86.h"

#include "base/logging.h"
#include "utils/x86/managed_register_x86.h"
#include "utils.h"

namespace art {
namespace x86 {

// Calling convention

ManagedRegister X86ManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return X86ManagedRegister::FromCpuRegister(ECX);
}

ManagedRegister X86JniCallingConvention::InterproceduralScratchRegister() {
  return X86ManagedRegister::FromCpuRegister(ECX);
}

ManagedRegister X86JniCallingConvention::ReturnScratchRegister() const {
  return ManagedRegister::NoRegister();  // No free regs, so assembler uses push/pop
}

static ManagedRegister ReturnRegisterForShorty(const char* shorty, bool jni) {
  if (shorty[0] == 'F' || shorty[0] == 'D') {
    if (jni) {
      return X86ManagedRegister::FromX87Register(ST0);
    } else {
      return X86ManagedRegister::FromXmmRegister(XMM0);
    }
  } else if (shorty[0] == 'J') {
    return X86ManagedRegister::FromRegisterPair(EAX_EDX);
  } else if (shorty[0] == 'V') {
    return ManagedRegister::NoRegister();
  } else {
    return X86ManagedRegister::FromCpuRegister(EAX);
  }
}

ManagedRegister X86ManagedRuntimeCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty(), false);
}

ManagedRegister X86JniCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty(), true);
}

ManagedRegister X86JniCallingConvention::IntReturnRegister() {
  return X86ManagedRegister::FromCpuRegister(EAX);
}

// Managed runtime calling convention

ManagedRegister X86ManagedRuntimeCallingConvention::MethodRegister() {
  return X86ManagedRegister::FromCpuRegister(EAX);
}

bool X86ManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything is passed by stack
}

bool X86ManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return true;  // Everything is passed by stack
}

ManagedRegister X86ManagedRuntimeCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset X86ManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  return FrameOffset(displacement_.Int32Value() +   // displacement
                     kPointerSize +                 // Method*
                     (itr_slots_ * kPointerSize));  // offset into in args
}

const std::vector<ManagedRegister>& X86ManagedRuntimeCallingConvention::EntrySpills() {
  // We spill the argument registers on X86 to free them up for scratch use, we then assume
  // all arguments are on the stack.
  if (entry_spills_.size() == 0) {
    size_t num_spills = NumArgs() + NumLongOrDoubleArgs();
    if (num_spills > 0) {
      entry_spills_.push_back(X86ManagedRegister::FromCpuRegister(ECX));
      if (num_spills > 1) {
        entry_spills_.push_back(X86ManagedRegister::FromCpuRegister(EDX));
        if (num_spills > 2) {
          entry_spills_.push_back(X86ManagedRegister::FromCpuRegister(EBX));
        }
      }
    }
  }
  return entry_spills_;
}

// JNI calling convention

X86JniCallingConvention::X86JniCallingConvention(bool is_static, bool is_synchronized,
                                                 const char* shorty)
    : JniCallingConvention(is_static, is_synchronized, shorty) {
  callee_save_regs_.push_back(X86ManagedRegister::FromCpuRegister(EBP));
  callee_save_regs_.push_back(X86ManagedRegister::FromCpuRegister(ESI));
  callee_save_regs_.push_back(X86ManagedRegister::FromCpuRegister(EDI));
}

uint32_t X86JniCallingConvention::CoreSpillMask() const {
  return 1 << EBP | 1 << ESI | 1 << EDI | 1 << kNumberOfCpuRegisters;
}

size_t X86JniCallingConvention::FrameSize() {
  // Method*, return address and callee save area size, local reference segment state
  size_t frame_data_size = (3 + CalleeSaveRegisters().size()) * kPointerSize;
  // References plus 2 words for SIRT header
  size_t sirt_size = (ReferenceCount() + 2) * kPointerSize;
  // Plus return value spill area size
  return RoundUp(frame_data_size + sirt_size + SizeOfReturnValue(), kStackAlignment);
}

size_t X86JniCallingConvention::OutArgSize() {
  return RoundUp(NumberOfOutgoingStackArgs() * kPointerSize, kStackAlignment);
}

bool X86JniCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything is passed by stack.
}

bool X86JniCallingConvention::IsCurrentParamOnStack() {
  return true;  // Everything is passed by stack.
}

ManagedRegister X86JniCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset X86JniCallingConvention::CurrentParamStackOffset() {
  return FrameOffset(displacement_.Int32Value() - OutArgSize() + (itr_slots_ * kPointerSize));
}

size_t X86JniCallingConvention::NumberOfOutgoingStackArgs() {
  size_t static_args = IsStatic() ? 1 : 0;  // count jclass
  // regular argument parameters and this
  size_t param_args = NumArgs() + NumLongOrDoubleArgs();
  // count JNIEnv* and return pc (pushed after Method*)
  size_t total_args = static_args + param_args + 2;
  return total_args;
}

}  // namespace x86
}  // namespace art
