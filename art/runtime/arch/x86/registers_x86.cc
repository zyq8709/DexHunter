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

#include "registers_x86.h"

#include <ostream>

namespace art {
namespace x86 {

static const char* kRegisterNames[] = {
  "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
};
std::ostream& operator<<(std::ostream& os, const Register& rhs) {
  if (rhs >= EAX && rhs <= EDI) {
    os << kRegisterNames[rhs];
  } else {
    os << "Register[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

}  // namespace x86
}  // namespace art
