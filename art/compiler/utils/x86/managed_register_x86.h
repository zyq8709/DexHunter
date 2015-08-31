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

#ifndef ART_COMPILER_UTILS_X86_MANAGED_REGISTER_X86_H_
#define ART_COMPILER_UTILS_X86_MANAGED_REGISTER_X86_H_

#include "constants_x86.h"
#include "utils/managed_register.h"

namespace art {
namespace x86 {

// Values for register pairs.
// The registers in kReservedCpuRegistersArray in x86.cc are not used in pairs.
// The table kRegisterPairs in x86.cc must be kept in sync with this enum.
enum RegisterPair {
  EAX_EDX = 0,
  EAX_ECX = 1,
  EAX_EBX = 2,
  EAX_EDI = 3,
  EDX_ECX = 4,
  EDX_EBX = 5,
  EDX_EDI = 6,
  ECX_EBX = 7,
  ECX_EDI = 8,
  EBX_EDI = 9,
  kNumberOfRegisterPairs = 10,
  kNoRegisterPair = -1,
};

std::ostream& operator<<(std::ostream& os, const RegisterPair& reg);

const int kNumberOfCpuRegIds = kNumberOfCpuRegisters;
const int kNumberOfCpuAllocIds = kNumberOfCpuRegisters;

const int kNumberOfXmmRegIds = kNumberOfXmmRegisters;
const int kNumberOfXmmAllocIds = kNumberOfXmmRegisters;

const int kNumberOfX87RegIds = kNumberOfX87Registers;
const int kNumberOfX87AllocIds = kNumberOfX87Registers;

const int kNumberOfPairRegIds = kNumberOfRegisterPairs;

const int kNumberOfRegIds = kNumberOfCpuRegIds + kNumberOfXmmRegIds +
    kNumberOfX87RegIds + kNumberOfPairRegIds;
const int kNumberOfAllocIds = kNumberOfCpuAllocIds + kNumberOfXmmAllocIds +
    kNumberOfX87RegIds;

// Register ids map:
//   [0..R[  cpu registers (enum Register)
//   [R..X[  xmm registers (enum XmmRegister)
//   [X..S[  x87 registers (enum X87Register)
//   [S..P[  register pairs (enum RegisterPair)
// where
//   R = kNumberOfCpuRegIds
//   X = R + kNumberOfXmmRegIds
//   S = X + kNumberOfX87RegIds
//   P = X + kNumberOfRegisterPairs

// Allocation ids map:
//   [0..R[  cpu registers (enum Register)
//   [R..X[  xmm registers (enum XmmRegister)
//   [X..S[  x87 registers (enum X87Register)
// where
//   R = kNumberOfCpuRegIds
//   X = R + kNumberOfXmmRegIds
//   S = X + kNumberOfX87RegIds


// An instance of class 'ManagedRegister' represents a single cpu register (enum
// Register), an xmm register (enum XmmRegister), or a pair of cpu registers
// (enum RegisterPair).
// 'ManagedRegister::NoRegister()' provides an invalid register.
// There is a one-to-one mapping between ManagedRegister and register id.
class X86ManagedRegister : public ManagedRegister {
 public:
  ByteRegister AsByteRegister() const {
    CHECK(IsCpuRegister());
    CHECK_LT(AsCpuRegister(), ESP);  // ESP, EBP, ESI and EDI cannot be encoded as byte registers.
    return static_cast<ByteRegister>(id_);
  }

  Register AsCpuRegister() const {
    CHECK(IsCpuRegister());
    return static_cast<Register>(id_);
  }

  XmmRegister AsXmmRegister() const {
    CHECK(IsXmmRegister());
    return static_cast<XmmRegister>(id_ - kNumberOfCpuRegIds);
  }

  X87Register AsX87Register() const {
    CHECK(IsX87Register());
    return static_cast<X87Register>(id_ -
                                    (kNumberOfCpuRegIds + kNumberOfXmmRegIds));
  }

  Register AsRegisterPairLow() const {
    CHECK(IsRegisterPair());
    // Appropriate mapping of register ids allows to use AllocIdLow().
    return FromRegId(AllocIdLow()).AsCpuRegister();
  }

  Register AsRegisterPairHigh() const {
    CHECK(IsRegisterPair());
    // Appropriate mapping of register ids allows to use AllocIdHigh().
    return FromRegId(AllocIdHigh()).AsCpuRegister();
  }

  bool IsCpuRegister() const {
    CHECK(IsValidManagedRegister());
    return (0 <= id_) && (id_ < kNumberOfCpuRegIds);
  }

  bool IsXmmRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - kNumberOfCpuRegIds;
    return (0 <= test) && (test < kNumberOfXmmRegIds);
  }

  bool IsX87Register() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - (kNumberOfCpuRegIds + kNumberOfXmmRegIds);
    return (0 <= test) && (test < kNumberOfX87RegIds);
  }

  bool IsRegisterPair() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ -
        (kNumberOfCpuRegIds + kNumberOfXmmRegIds + kNumberOfX87RegIds);
    return (0 <= test) && (test < kNumberOfPairRegIds);
  }

  void Print(std::ostream& os) const;

  // Returns true if the two managed-registers ('this' and 'other') overlap.
  // Either managed-register may be the NoRegister. If both are the NoRegister
  // then false is returned.
  bool Overlaps(const X86ManagedRegister& other) const;

  static X86ManagedRegister FromCpuRegister(Register r) {
    CHECK_NE(r, kNoRegister);
    return FromRegId(r);
  }

  static X86ManagedRegister FromXmmRegister(XmmRegister r) {
    CHECK_NE(r, kNoXmmRegister);
    return FromRegId(r + kNumberOfCpuRegIds);
  }

  static X86ManagedRegister FromX87Register(X87Register r) {
    CHECK_NE(r, kNoX87Register);
    return FromRegId(r + kNumberOfCpuRegIds + kNumberOfXmmRegIds);
  }

  static X86ManagedRegister FromRegisterPair(RegisterPair r) {
    CHECK_NE(r, kNoRegisterPair);
    return FromRegId(r + (kNumberOfCpuRegIds + kNumberOfXmmRegIds +
                          kNumberOfX87RegIds));
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
    CHECK(IsValidManagedRegister() && !IsRegisterPair());
    CHECK_LT(id_, kNumberOfAllocIds);
    return id_;
  }

  int AllocIdLow() const;
  int AllocIdHigh() const;

  friend class ManagedRegister;

  explicit X86ManagedRegister(int reg_id) : ManagedRegister(reg_id) {}

  static X86ManagedRegister FromRegId(int reg_id) {
    X86ManagedRegister reg(reg_id);
    CHECK(reg.IsValidManagedRegister());
    return reg;
  }
};

std::ostream& operator<<(std::ostream& os, const X86ManagedRegister& reg);

}  // namespace x86

inline x86::X86ManagedRegister ManagedRegister::AsX86() const {
  x86::X86ManagedRegister reg(id_);
  CHECK(reg.IsNoRegister() || reg.IsValidManagedRegister());
  return reg;
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_X86_MANAGED_REGISTER_X86_H_
