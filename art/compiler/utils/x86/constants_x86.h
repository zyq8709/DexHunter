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

#ifndef ART_COMPILER_UTILS_X86_CONSTANTS_X86_H_
#define ART_COMPILER_UTILS_X86_CONSTANTS_X86_H_

#include <iosfwd>

#include "arch/x86/registers_x86.h"
#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"

namespace art {
namespace x86 {

enum ByteRegister {
  AL = 0,
  CL = 1,
  DL = 2,
  BL = 3,
  AH = 4,
  CH = 5,
  DH = 6,
  BH = 7,
  kNoByteRegister = -1  // Signals an illegal register.
};


enum XmmRegister {
  XMM0 = 0,
  XMM1 = 1,
  XMM2 = 2,
  XMM3 = 3,
  XMM4 = 4,
  XMM5 = 5,
  XMM6 = 6,
  XMM7 = 7,
  kNumberOfXmmRegisters = 8,
  kNoXmmRegister = -1  // Signals an illegal register.
};
std::ostream& operator<<(std::ostream& os, const XmmRegister& reg);

enum X87Register {
  ST0 = 0,
  ST1 = 1,
  ST2 = 2,
  ST3 = 3,
  ST4 = 4,
  ST5 = 5,
  ST6 = 6,
  ST7 = 7,
  kNumberOfX87Registers = 8,
  kNoX87Register = -1  // Signals an illegal register.
};
std::ostream& operator<<(std::ostream& os, const X87Register& reg);

enum ScaleFactor {
  TIMES_1 = 0,
  TIMES_2 = 1,
  TIMES_4 = 2,
  TIMES_8 = 3
};

enum Condition {
  kOverflow     =  0,
  kNoOverflow   =  1,
  kBelow        =  2,
  kAboveEqual   =  3,
  kEqual        =  4,
  kNotEqual     =  5,
  kBelowEqual   =  6,
  kAbove        =  7,
  kSign         =  8,
  kNotSign      =  9,
  kParityEven   = 10,
  kParityOdd    = 11,
  kLess         = 12,
  kGreaterEqual = 13,
  kLessEqual    = 14,
  kGreater      = 15,

  kZero         = kEqual,
  kNotZero      = kNotEqual,
  kNegative     = kSign,
  kPositive     = kNotSign
};


class Instr {
 public:
  static const uint8_t kHltInstruction = 0xF4;
  // We prefer not to use the int3 instruction since it conflicts with gdb.
  static const uint8_t kBreakPointInstruction = kHltInstruction;

  bool IsBreakPoint() {
    return (*reinterpret_cast<const uint8_t*>(this)) == kBreakPointInstruction;
  }

  // Instructions are read out of a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instr.
  // Use the At(pc) function to create references to Instr.
  static Instr* At(uintptr_t pc) { return reinterpret_cast<Instr*>(pc); }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instr);
};

}  // namespace x86
}  // namespace art

#endif  // ART_COMPILER_UTILS_X86_CONSTANTS_X86_H_
