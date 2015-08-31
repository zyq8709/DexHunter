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

#ifndef ART_COMPILER_JNI_QUICK_X86_CALLING_CONVENTION_X86_H_
#define ART_COMPILER_JNI_QUICK_X86_CALLING_CONVENTION_X86_H_

#include "jni/quick/calling_convention.h"

namespace art {
namespace x86 {

class X86ManagedRuntimeCallingConvention : public ManagedRuntimeCallingConvention {
 public:
  explicit X86ManagedRuntimeCallingConvention(bool is_static, bool is_synchronized,
                                              const char* shorty)
      : ManagedRuntimeCallingConvention(is_static, is_synchronized, shorty) {}
  virtual ~X86ManagedRuntimeCallingConvention() {}
  // Calling convention
  virtual ManagedRegister ReturnRegister();
  virtual ManagedRegister InterproceduralScratchRegister();
  // Managed runtime calling convention
  virtual ManagedRegister MethodRegister();
  virtual bool IsCurrentParamInRegister();
  virtual bool IsCurrentParamOnStack();
  virtual ManagedRegister CurrentParamRegister();
  virtual FrameOffset CurrentParamStackOffset();
  virtual const std::vector<ManagedRegister>& EntrySpills();
 private:
  std::vector<ManagedRegister> entry_spills_;
  DISALLOW_COPY_AND_ASSIGN(X86ManagedRuntimeCallingConvention);
};

class X86JniCallingConvention : public JniCallingConvention {
 public:
  explicit X86JniCallingConvention(bool is_static, bool is_synchronized, const char* shorty);
  virtual ~X86JniCallingConvention() {}
  // Calling convention
  virtual ManagedRegister ReturnRegister();
  virtual ManagedRegister IntReturnRegister();
  virtual ManagedRegister InterproceduralScratchRegister();
  // JNI calling convention
  virtual size_t FrameSize();
  virtual size_t OutArgSize();
  virtual const std::vector<ManagedRegister>& CalleeSaveRegisters() const {
    return callee_save_regs_;
  }
  virtual ManagedRegister ReturnScratchRegister() const;
  virtual uint32_t CoreSpillMask() const;
  virtual uint32_t FpSpillMask() const {
    return 0;
  }
  virtual bool IsCurrentParamInRegister();
  virtual bool IsCurrentParamOnStack();
  virtual ManagedRegister CurrentParamRegister();
  virtual FrameOffset CurrentParamStackOffset();

 protected:
  virtual size_t NumberOfOutgoingStackArgs();

 private:
  // TODO: these values aren't unique and can be shared amongst instances
  std::vector<ManagedRegister> callee_save_regs_;

  DISALLOW_COPY_AND_ASSIGN(X86JniCallingConvention);
};

}  // namespace x86
}  // namespace art

#endif  // ART_COMPILER_JNI_QUICK_X86_CALLING_CONVENTION_X86_H_
