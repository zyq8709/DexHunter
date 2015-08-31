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

#include "registers_mips.h"

#include <ostream>

namespace art {
namespace mips {

static const char* kRegisterNames[] = {
  "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
  "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
  "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};
std::ostream& operator<<(std::ostream& os, const Register& rhs) {
  if (rhs >= ZERO && rhs <= RA) {
    os << kRegisterNames[rhs];
  } else {
    os << "Register[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const FRegister& rhs) {
  if (rhs >= F0 && rhs < kNumberOfFRegisters) {
    os << "f" << static_cast<int>(rhs);
  } else {
    os << "FRegister[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

}  // namespace mips
}  // namespace art
