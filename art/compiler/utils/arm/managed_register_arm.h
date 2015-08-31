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

#ifndef ART_COMPILER_UTILS_ARM_MANAGED_REGISTER_ARM_H_
#define ART_COMPILER_UTILS_ARM_MANAGED_REGISTER_ARM_H_

#include "base/logging.h"
#include "constants_arm.h"
#include "utils/managed_register.h"

namespace art {
namespace arm {

// Values for register pairs.
enum RegisterPair {
  R0_R1 = 0,
  R2_R3 = 1,
  R4_R5 = 2,
  R6_R7 = 3,
  R1_R2 = 4,  // Dalvik style passing
  kNumberOfRegisterPairs = 5,
  kNoRegisterPair = -1,
};

std::ostream& operator<<(std::ostream& os, const RegisterPair& reg);

const int kNumberOfCoreRegIds = kNumberOfCoreRegisters;
const int kNumberOfCoreAllocIds = kNumberOfCoreRegisters;

const int kNumberOfSRegIds = kNumberOfSRegisters;
const int kNumberOfSAllocIds = kNumberOfSRegisters;

const int kNumberOfDRegIds = kNumberOfDRegisters;
const int kNumberOfOverlappingDRegIds = kNumberOfOverlappingDRegisters;
const int kNumberOfDAllocIds = kNumberOfDRegIds - kNumberOfOverlappingDRegIds;

const int kNumberOfPairRegIds = kNumberOfRegisterPairs;

const int kNumberOfRegIds = kNumberOfCoreRegIds + kNumberOfSRegIds +
    kNumberOfDRegIds + kNumberOfPairRegIds;
const int kNumberOfAllocIds =
    kNumberOfCoreAllocIds + kNumberOfSAllocIds + kNumberOfDAllocIds;

// Register ids map:
//   [0..R[  core registers (enum Register)
//   [R..S[  single precision VFP registers (enum SRegister)
//   [S..D[  double precision VFP registers (enum DRegister)
//   [D..P[  core register pairs (enum RegisterPair)
// where
//   R = kNumberOfCoreRegIds
//   S = R + kNumberOfSRegIds
//   D = S + kNumberOfDRegIds
//   P = D + kNumberOfRegisterPairs

// Allocation ids map:
//   [0..R[  core registers (enum Register)
//   [R..S[  single precision VFP registers (enum SRegister)
//   [S..N[  non-overlapping double precision VFP registers (16-31 in enum
//           DRegister, VFPv3-D32 only)
// where
//   R = kNumberOfCoreAllocIds
//   S = R + kNumberOfSAllocIds
//   N = S + kNumberOfDAllocIds


// An instance of class 'ManagedRegister' represents a single ARM register or a
// pair of core ARM registers (enum RegisterPair). A single register is either a
// core register (enum Register), a VFP single precision register
// (enum SRegister), or a VFP double precision register (enum DRegister).
// 'ManagedRegister::NoRegister()' returns an invalid ManagedRegister.
// There is a one-to-one mapping between ManagedRegister and register id.
class ArmManagedRegister : public ManagedRegister {
 public:
  Register AsCoreRegister() const {
    CHECK(IsCoreRegister());
    return static_cast<Register>(id_);
  }

  SRegister AsSRegister() const {
    CHECK(IsSRegister());
    return static_cast<SRegister>(id_ - kNumberOfCoreRegIds);
  }

  DRegister AsDRegister() const {
    CHECK(IsDRegister());
    return static_cast<DRegister>(id_ - kNumberOfCoreRegIds - kNumberOfSRegIds);
  }

  SRegister AsOverlappingDRegisterLow() const {
    CHECK(IsOverlappingDRegister());
    DRegister d_reg = AsDRegister();
    return static_cast<SRegister>(d_reg * 2);
  }

  SRegister AsOverlappingDRegisterHigh() const {
    CHECK(IsOverlappingDRegister());
    DRegister d_reg = AsDRegister();
    return static_cast<SRegister>(d_reg * 2 + 1);
  }

  RegisterPair AsRegisterPair() const {
    CHECK(IsRegisterPair());
    Register reg_low = AsRegisterPairLow();
    if (reg_low == R1) {
      return R1_R2;
    } else {
      return static_cast<RegisterPair>(reg_low / 2);
    }
  }

  Register AsRegisterPairLow() const {
    CHECK(IsRegisterPair());
    // Appropriate mapping of register ids allows to use AllocIdLow().
    return FromRegId(AllocIdLow()).AsCoreRegister();
  }

  Register AsRegisterPairHigh() const {
    CHECK(IsRegisterPair());
    // Appropriate mapping of register ids allows to use AllocIdHigh().
    return FromRegId(AllocIdHigh()).AsCoreRegister();
  }

  bool IsCoreRegister() const {
    CHECK(IsValidManagedRegister());
    return (0 <= id_) && (id_ < kNumberOfCoreRegIds);
  }

  bool IsSRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - kNumberOfCoreRegIds;
    return (0 <= test) && (test < kNumberOfSRegIds);
  }

  bool IsDRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - (kNumberOfCoreRegIds + kNumberOfSRegIds);
    return (0 <= test) && (test < kNumberOfDRegIds);
  }

  // Returns true if this DRegister overlaps SRegisters.
  bool IsOverlappingDRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - (kNumberOfCoreRegIds + kNumberOfSRegIds);
    return (0 <= test) && (test < kNumberOfOverlappingDRegIds);
  }

  bool IsRegisterPair() const {
    CHECK(IsValidManagedRegister());
    const int test =
        id_ - (kNumberOfCoreRegIds + kNumberOfSRegIds + kNumberOfDRegIds);
    return (0 <= test) && (test < kNumberOfPairRegIds);
  }

  bool IsSameType(ArmManagedRegister test) const {
    CHECK(IsValidManagedRegister() && test.IsValidManagedRegister());
    return
      (IsCoreRegister() && test.IsCoreRegister()) ||
      (IsSRegister() && test.IsSRegister()) ||
      (IsDRegister() && test.IsDRegister()) ||
      (IsRegisterPair() && test.IsRegisterPair());
  }


  // Returns true if the two managed-registers ('this' and 'other') overlap.
  // Either managed-register may be the NoRegister. If both are the NoRegister
  // then false is returned.
  bool Overlaps(const ArmManagedRegister& other) const;

  void Print(std::ostream& os) const;

  static ArmManagedRegister FromCoreRegister(Register r) {
    CHECK_NE(r, kNoRegister);
    return FromRegId(r);
  }

  static ArmManagedRegister FromSRegister(SRegister r) {
    CHECK_NE(r, kNoSRegister);
    return FromRegId(r + kNumberOfCoreRegIds);
  }

  static ArmManagedRegister FromDRegister(DRegister r) {
    CHECK_NE(r, kNoDRegister);
    return FromRegId(r + (kNumberOfCoreRegIds + kNumberOfSRegIds));
  }

  static ArmManagedRegister FromRegisterPair(RegisterPair r) {
    CHECK_NE(r, kNoRegisterPair);
    return FromRegId(r + (kNumberOfCoreRegIds +
                          kNumberOfSRegIds + kNumberOfDRegIds));
  }

  // Return a RegisterPair consisting of Register r_low and r_low + 1.
  static ArmManagedRegister FromCoreRegisterPair(Register r_low) {
    if (r_low != R1) {  // not the dalvik special case
      CHECK_NE(r_low, kNoRegister);
      CHECK_EQ(0, (r_low % 2));
      const int r = r_low / 2;
      CHECK_LT(r, kNumberOfPairRegIds);
      return FromRegisterPair(static_cast<RegisterPair>(r));
    } else {
      return FromRegisterPair(R1_R2);
    }
  }

  // Return a DRegister overlapping SRegister r_low and r_low + 1.
  static ArmManagedRegister FromSRegisterPair(SRegister r_low) {
    CHECK_NE(r_low, kNoSRegister);
    CHECK_EQ(0, (r_low % 2));
    const int r = r_low / 2;
    CHECK_LT(r, kNumberOfOverlappingDRegIds);
    return FromDRegister(static_cast<DRegister>(r));
  }

 private:
  bool IsValidManagedRegister() const {
    return (0 <= id_) && (id_ < kNumberOfRegIds);
  }

  int RegId() const {
    CHECK(!IsNoRegister());
    return id_;
  }

  int AllocId() const {
    CHECK(IsValidManagedRegister() &&
           !IsOverlappingDRegister() && !IsRegisterPair());
    int r = id_;
    if ((kNumberOfDAllocIds > 0) && IsDRegister()) {  // VFPv3-D32 only.
      r -= kNumberOfOverlappingDRegIds;
    }
    CHECK_LT(r, kNumberOfAllocIds);
    return r;
  }

  int AllocIdLow() const;
  int AllocIdHigh() const;

  friend class ManagedRegister;

  explicit ArmManagedRegister(int reg_id) : ManagedRegister(reg_id) {}

  static ArmManagedRegister FromRegId(int reg_id) {
    ArmManagedRegister reg(reg_id);
    CHECK(reg.IsValidManagedRegister());
    return reg;
  }
};

std::ostream& operator<<(std::ostream& os, const ArmManagedRegister& reg);

}  // namespace arm

inline arm::ArmManagedRegister ManagedRegister::AsArm() const {
  arm::ArmManagedRegister reg(id_);
  CHECK(reg.IsNoRegister() || reg.IsValidManagedRegister());
  return reg;
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM_MANAGED_REGISTER_ARM_H_
