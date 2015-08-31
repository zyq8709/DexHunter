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

#ifndef ART_RUNTIME_DISASSEMBLER_ARM_H_
#define ART_RUNTIME_DISASSEMBLER_ARM_H_

#include <vector>

#include "disassembler.h"

namespace art {
namespace arm {

class DisassemblerArm : public Disassembler {
 public:
  DisassemblerArm();

  virtual size_t Dump(std::ostream& os, const uint8_t* begin);
  virtual void Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end);
 private:
  void DumpArm(std::ostream& os, const uint8_t* instr);

  // Returns the size of the instruction just decoded
  size_t DumpThumb16(std::ostream& os, const uint8_t* instr);
  size_t DumpThumb32(std::ostream& os, const uint8_t* instr_ptr);

  void DumpBranchTarget(std::ostream& os, const uint8_t* instr_ptr, int32_t imm32);
  void DumpCond(std::ostream& os, uint32_t cond);

  std::vector<const char*> it_conditions_;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerArm);
};

}  // namespace arm
}  // namespace art

#endif  // ART_RUNTIME_DISASSEMBLER_ARM_H_
