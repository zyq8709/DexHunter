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

#include "context_arm.h"

#include "mirror/art_method.h"
#include "mirror/object-inl.h"
#include "stack.h"
#include "thread.h"

namespace art {
namespace arm {

static const uint32_t gZero = 0;

void ArmContext::Reset() {
  for (size_t i = 0; i < kNumberOfCoreRegisters; i++) {
    gprs_[i] = NULL;
  }
  for (size_t i = 0; i < kNumberOfSRegisters; i++) {
    fprs_[i] = NULL;
  }
  gprs_[SP] = &sp_;
  gprs_[PC] = &pc_;
  // Initialize registers with easy to spot debug values.
  sp_ = ArmContext::kBadGprBase + SP;
  pc_ = ArmContext::kBadGprBase + PC;
}

void ArmContext::FillCalleeSaves(const StackVisitor& fr) {
  mirror::ArtMethod* method = fr.GetMethod();
  uint32_t core_spills = method->GetCoreSpillMask();
  uint32_t fp_core_spills = method->GetFpSpillMask();
  size_t spill_count = __builtin_popcount(core_spills);
  size_t fp_spill_count = __builtin_popcount(fp_core_spills);
  size_t frame_size = method->GetFrameSizeInBytes();
  if (spill_count > 0) {
    // Lowest number spill is farthest away, walk registers and fill into context
    int j = 1;
    for (size_t i = 0; i < kNumberOfCoreRegisters; i++) {
      if (((core_spills >> i) & 1) != 0) {
        gprs_[i] = fr.CalleeSaveAddress(spill_count - j, frame_size);
        j++;
      }
    }
  }
  if (fp_spill_count > 0) {
    // Lowest number spill is farthest away, walk registers and fill into context
    int j = 1;
    for (size_t i = 0; i < kNumberOfSRegisters; i++) {
      if (((fp_core_spills >> i) & 1) != 0) {
        fprs_[i] = fr.CalleeSaveAddress(spill_count + fp_spill_count - j, frame_size);
        j++;
      }
    }
  }
}

void ArmContext::SetGPR(uint32_t reg, uintptr_t value) {
  DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
  DCHECK_NE(gprs_[reg], &gZero);  // Can't overwrite this static value since they are never reset.
  DCHECK(gprs_[reg] != NULL);
  *gprs_[reg] = value;
}

void ArmContext::SmashCallerSaves() {
  // This needs to be 0 because we want a null/zero return value.
  gprs_[R0] = const_cast<uint32_t*>(&gZero);
  gprs_[R1] = const_cast<uint32_t*>(&gZero);
  gprs_[R2] = NULL;
  gprs_[R3] = NULL;
}

extern "C" void art_quick_do_long_jump(uint32_t*, uint32_t*);

void ArmContext::DoLongJump() {
  uintptr_t gprs[16];
  uint32_t fprs[32];
  for (size_t i = 0; i < kNumberOfCoreRegisters; ++i) {
    gprs[i] = gprs_[i] != NULL ? *gprs_[i] : ArmContext::kBadGprBase + i;
  }
  for (size_t i = 0; i < kNumberOfSRegisters; ++i) {
    fprs[i] = fprs_[i] != NULL ? *fprs_[i] : ArmContext::kBadGprBase + i;
  }
  DCHECK_EQ(reinterpret_cast<uintptr_t>(Thread::Current()), gprs[TR]);
  art_quick_do_long_jump(gprs, fprs);
}

}  // namespace arm
}  // namespace art
