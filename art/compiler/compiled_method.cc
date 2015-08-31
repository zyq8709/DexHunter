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

#include "compiled_method.h"
#include "driver/compiler_driver.h"

namespace art {

CompiledCode::CompiledCode(CompilerDriver* compiler_driver, InstructionSet instruction_set,
                           const std::vector<uint8_t>& code)
    : compiler_driver_(compiler_driver), instruction_set_(instruction_set), code_(nullptr) {
  SetCode(code);
}

CompiledCode::CompiledCode(CompilerDriver* compiler_driver, InstructionSet instruction_set,
                           const std::string& elf_object, const std::string& symbol)
    : compiler_driver_(compiler_driver), instruction_set_(instruction_set), symbol_(symbol) {
  CHECK_NE(elf_object.size(), 0U);
  CHECK_NE(symbol.size(), 0U);
  std::vector<uint8_t> temp_code(elf_object.size());
  for (size_t i = 0; i < elf_object.size(); ++i) {
    temp_code[i] = elf_object[i];
  }
  // TODO: we shouldn't just shove ELF objects in as "code" but
  // change to have different kinds of compiled methods.  This is
  // being deferred until we work on hybrid execution or at least
  // until we work on batch compilation.
  SetCode(temp_code);
}

void CompiledCode::SetCode(const std::vector<uint8_t>& code) {
  CHECK(!code.empty());
  code_ = compiler_driver_->DeduplicateCode(code);
}

uint32_t CompiledCode::AlignCode(uint32_t offset) const {
  return AlignCode(offset, instruction_set_);
}

uint32_t CompiledCode::AlignCode(uint32_t offset, InstructionSet instruction_set) {
  switch (instruction_set) {
    case kArm:
    case kThumb2:
      return RoundUp(offset, kArmAlignment);
    case kMips:
      return RoundUp(offset, kMipsAlignment);
    case kX86:
      return RoundUp(offset, kX86Alignment);
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << instruction_set;
      return 0;
  }
}

size_t CompiledCode::CodeDelta() const {
  switch (instruction_set_) {
    case kArm:
    case kMips:
    case kX86:
      return 0;
    case kThumb2: {
      // +1 to set the low-order bit so a BLX will switch to Thumb mode
      return 1;
    }
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << instruction_set_;
      return 0;
  }
}

const void* CompiledCode::CodePointer(const void* code_pointer,
                                      InstructionSet instruction_set) {
  switch (instruction_set) {
    case kArm:
    case kMips:
    case kX86:
      return code_pointer;
    case kThumb2: {
      uintptr_t address = reinterpret_cast<uintptr_t>(code_pointer);
      // Set the low-order bit so a BLX will switch to Thumb mode
      address |= 0x1;
      return reinterpret_cast<const void*>(address);
    }
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << instruction_set;
      return NULL;
  }
}

#if defined(ART_USE_PORTABLE_COMPILER)
const std::string& CompiledCode::GetSymbol() const {
  CHECK_NE(0U, symbol_.size());
  return symbol_;
}

const std::vector<uint32_t>& CompiledCode::GetOatdataOffsetsToCompliledCodeOffset() const {
  CHECK_NE(0U, oatdata_offsets_to_compiled_code_offset_.size()) << symbol_;
  return oatdata_offsets_to_compiled_code_offset_;
}

void CompiledCode::AddOatdataOffsetToCompliledCodeOffset(uint32_t offset) {
  oatdata_offsets_to_compiled_code_offset_.push_back(offset);
}
#endif

CompiledMethod::CompiledMethod(CompilerDriver& driver,
                               InstructionSet instruction_set,
                               const std::vector<uint8_t>& code,
                               const size_t frame_size_in_bytes,
                               const uint32_t core_spill_mask,
                               const uint32_t fp_spill_mask,
                               const std::vector<uint8_t>& mapping_table,
                               const std::vector<uint8_t>& vmap_table,
                               const std::vector<uint8_t>& native_gc_map)
    : CompiledCode(&driver, instruction_set, code), frame_size_in_bytes_(frame_size_in_bytes),
      core_spill_mask_(core_spill_mask), fp_spill_mask_(fp_spill_mask),
  mapping_table_(driver.DeduplicateMappingTable(mapping_table)),
  vmap_table_(driver.DeduplicateVMapTable(vmap_table)),
  gc_map_(driver.DeduplicateGCMap(native_gc_map)) {
}

CompiledMethod::CompiledMethod(CompilerDriver& driver,
                               InstructionSet instruction_set,
                               const std::vector<uint8_t>& code,
                               const size_t frame_size_in_bytes,
                               const uint32_t core_spill_mask,
                               const uint32_t fp_spill_mask)
    : CompiledCode(&driver, instruction_set, code),
      frame_size_in_bytes_(frame_size_in_bytes),
      core_spill_mask_(core_spill_mask), fp_spill_mask_(fp_spill_mask) {
  mapping_table_ = driver.DeduplicateMappingTable(std::vector<uint8_t>());
  vmap_table_ = driver.DeduplicateVMapTable(std::vector<uint8_t>());
  gc_map_ = driver.DeduplicateGCMap(std::vector<uint8_t>());
}

// Constructs a CompiledMethod for the Portable compiler.
CompiledMethod::CompiledMethod(CompilerDriver& driver, InstructionSet instruction_set,
                               const std::string& code, const std::vector<uint8_t>& gc_map,
                               const std::string& symbol)
    : CompiledCode(&driver, instruction_set, code, symbol),
      frame_size_in_bytes_(kStackAlignment), core_spill_mask_(0),
      fp_spill_mask_(0), gc_map_(driver.DeduplicateGCMap(gc_map)) {
  mapping_table_ = driver.DeduplicateMappingTable(std::vector<uint8_t>());
  vmap_table_ = driver.DeduplicateVMapTable(std::vector<uint8_t>());
}

CompiledMethod::CompiledMethod(CompilerDriver& driver, InstructionSet instruction_set,
                               const std::string& code, const std::string& symbol)
    : CompiledCode(&driver, instruction_set, code, symbol),
      frame_size_in_bytes_(kStackAlignment), core_spill_mask_(0),
      fp_spill_mask_(0) {
  mapping_table_ = driver.DeduplicateMappingTable(std::vector<uint8_t>());
  vmap_table_ = driver.DeduplicateVMapTable(std::vector<uint8_t>());
  gc_map_ = driver.DeduplicateGCMap(std::vector<uint8_t>());
}

}  // namespace art
