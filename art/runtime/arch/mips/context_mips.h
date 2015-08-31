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

#ifndef ART_RUNTIME_ARCH_MIPS_CONTEXT_MIPS_H_
#define ART_RUNTIME_ARCH_MIPS_CONTEXT_MIPS_H_

#include "arch/context.h"
#include "base/logging.h"
#include "registers_mips.h"

namespace art {
namespace mips {

class MipsContext : public Context {
 public:
  MipsContext() {
    Reset();
  }
  virtual ~MipsContext() {}

  virtual void Reset();

  virtual void FillCalleeSaves(const StackVisitor& fr);

  virtual void SetSP(uintptr_t new_sp) {
    SetGPR(SP, new_sp);
  }

  virtual void SetPC(uintptr_t new_pc) {
    SetGPR(RA, new_pc);
  }

  virtual uintptr_t GetGPR(uint32_t reg) {
    CHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
    return *gprs_[reg];
  }

  virtual void SetGPR(uint32_t reg, uintptr_t value);
  virtual void SmashCallerSaves();
  virtual void DoLongJump();

 private:
  // Pointers to registers in the stack, initialized to NULL except for the special cases below.
  uintptr_t* gprs_[kNumberOfCoreRegisters];
  uint32_t* fprs_[kNumberOfFRegisters];
  // Hold values for sp and ra (return address) if they are not located within a stack frame.
  uintptr_t sp_, ra_;
};
}  // namespace mips
}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS_CONTEXT_MIPS_H_
