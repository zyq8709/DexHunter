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

#include "context_x86.h"

#include "mirror/art_method.h"
#include "mirror/object-inl.h"
#include "stack.h"

namespace art {
namespace x86 {

static const uint32_t gZero = 0;

void X86Context::Reset() {
  for (int i = 0; i < kNumberOfCpuRegisters; i++) {
    gprs_[i] = NULL;
  }
  gprs_[ESP] = &esp_;
  // Initialize registers with easy to spot debug values.
  esp_ = X86Context::kBadGprBase + ESP;
  eip_ = X86Context::kBadGprBase + kNumberOfCpuRegisters;
}

void X86Context::FillCalleeSaves(const StackVisitor& fr) {
  mirror::ArtMethod* method = fr.GetMethod();
  uint32_t core_spills = method->GetCoreSpillMask();
  size_t spill_count = __builtin_popcount(core_spills);
  DCHECK_EQ(method->GetFpSpillMask(), 0u);
  size_t frame_size = method->GetFrameSizeInBytes();
  if (spill_count > 0) {
    // Lowest number spill is farthest away, walk registers and fill into context.
    int j = 2;  // Offset j to skip return address spill.
    for (int i = 0; i < kNumberOfCpuRegisters; i++) {
      if (((core_spills >> i) & 1) != 0) {
        gprs_[i] = fr.CalleeSaveAddress(spill_count - j, frame_size);
        j++;
      }
    }
  }
}

void X86Context::SmashCallerSaves() {
  // This needs to be 0 because we want a null/zero return value.
  gprs_[EAX] = const_cast<uint32_t*>(&gZero);
  gprs_[EDX] = const_cast<uint32_t*>(&gZero);
  gprs_[ECX] = NULL;
  gprs_[EBX] = NULL;
}

void X86Context::SetGPR(uint32_t reg, uintptr_t value) {
  CHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
  CHECK_NE(gprs_[reg], &gZero);
  CHECK(gprs_[reg] != NULL);
  *gprs_[reg] = value;
}

void X86Context::DoLongJump() {
#if defined(__i386__)
  // Array of GPR values, filled from the context backward for the long jump pop. We add a slot at
  // the top for the stack pointer that doesn't get popped in a pop-all.
  volatile uintptr_t gprs[kNumberOfCpuRegisters + 1];
  for (size_t i = 0; i < kNumberOfCpuRegisters; ++i) {
    gprs[kNumberOfCpuRegisters - i - 1] = gprs_[i] != NULL ? *gprs_[i] : X86Context::kBadGprBase + i;
  }
  // We want to load the stack pointer one slot below so that the ret will pop eip.
  uintptr_t esp = gprs[kNumberOfCpuRegisters - ESP - 1] - kWordSize;
  gprs[kNumberOfCpuRegisters] = esp;
  *(reinterpret_cast<uintptr_t*>(esp)) = eip_;
  __asm__ __volatile__(
      "movl %0, %%esp\n\t"  // ESP points to gprs.
      "popal\n\t"           // Load all registers except ESP and EIP with values in gprs.
      "popl %%esp\n\t"      // Load stack pointer.
      "ret\n\t"             // From higher in the stack pop eip.
      :  // output.
      : "g"(&gprs[0])  // input.
      :);  // clobber.
#else
    UNIMPLEMENTED(FATAL);
#endif
}

}  // namespace x86
}  // namespace art
