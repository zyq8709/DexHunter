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

#include "registers_arm.h"

#include <ostream>

namespace art {
namespace arm {

static const char* kRegisterNames[] = {
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
  "fp", "ip", "sp", "lr", "pc"
};
std::ostream& operator<<(std::ostream& os, const Register& rhs) {
  if (rhs >= R0 && rhs <= PC) {
    os << kRegisterNames[rhs];
  } else {
    os << "Register[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const SRegister& rhs) {
  if (rhs >= S0 && rhs < kNumberOfSRegisters) {
    os << "s" << static_cast<int>(rhs);
  } else {
    os << "SRegister[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

}  // namespace arm
}  // namespace art
