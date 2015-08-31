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

#ifndef ART_RUNTIME_DISASSEMBLER_H_
#define ART_RUNTIME_DISASSEMBLER_H_

#include <stdint.h>

#include <iosfwd>

#include "base/macros.h"
#include "instruction_set.h"

namespace art {

class Disassembler {
 public:
  static Disassembler* Create(InstructionSet instruction_set);
  virtual ~Disassembler() {}

  // Dump a single instruction returning the length of that instruction.
  virtual size_t Dump(std::ostream& os, const uint8_t* begin) = 0;
  // Dump instructions within a range.
  virtual void Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) = 0;

 protected:
  Disassembler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Disassembler);
};

}  // namespace art

#endif  // ART_RUNTIME_DISASSEMBLER_H_
