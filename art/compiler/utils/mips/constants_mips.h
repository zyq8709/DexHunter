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

#ifndef ART_COMPILER_UTILS_MIPS_CONSTANTS_MIPS_H_
#define ART_COMPILER_UTILS_MIPS_CONSTANTS_MIPS_H_

#include <iosfwd>

#include "arch/mips/registers_mips.h"
#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"

namespace art {
namespace mips {

// Values for double-precision floating point registers.
enum DRegister {
  D0  =  0,
  D1  =  1,
  D2  =  2,
  D3  =  3,
  D4  =  4,
  D5  =  5,
  D6  =  6,
  D7  =  7,
  D8  =  8,
  D9  =  9,
  D10 = 10,
  D11 = 11,
  D12 = 12,
  D13 = 13,
  D14 = 14,
  D15 = 15,
  kNumberOfDRegisters = 16,
  kNumberOfOverlappingDRegisters = 16,
  kNoDRegister = -1,
};
std::ostream& operator<<(std::ostream& os, const DRegister& rhs);

// Constants used for the decoding or encoding of the individual fields of instructions.
enum InstructionFields {
  kOpcodeShift = 26,
  kOpcodeBits = 6,
  kRsShift = 21,
  kRsBits = 5,
  kRtShift = 16,
  kRtBits = 5,
  kRdShift = 11,
  kRdBits = 5,
  kShamtShift = 6,
  kShamtBits = 5,
  kFunctShift = 0,
  kFunctBits = 6,

  kFmtShift = 21,
  kFmtBits = 5,
  kFtShift = 16,
  kFtBits = 5,
  kFsShift = 11,
  kFsBits = 5,
  kFdShift = 6,
  kFdBits = 5,

  kBranchOffsetMask = 0x0000ffff,
  kJumpOffsetMask = 0x03ffffff,
};

enum ScaleFactor {
  TIMES_1 = 0,
  TIMES_2 = 1,
  TIMES_4 = 2,
  TIMES_8 = 3
};

class Instr {
 public:
  static const uint32_t kBreakPointInstruction = 0x0000000D;

  bool IsBreakPoint() {
    return ((*reinterpret_cast<const uint32_t*>(this)) & 0xFC0000CF) == kBreakPointInstruction;
  }

  // Instructions are read out of a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instr.
  // Use the At(pc) function to create references to Instr.
  static Instr* At(uintptr_t pc) { return reinterpret_cast<Instr*>(pc); }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instr);
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_UTILS_MIPS_CONSTANTS_MIPS_H_
