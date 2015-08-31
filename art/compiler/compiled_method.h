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

#ifndef ART_COMPILER_COMPILED_METHOD_H_
#define ART_COMPILER_COMPILED_METHOD_H_

#include <string>
#include <vector>

#include "instruction_set.h"
#include "utils.h"
#include "UniquePtr.h"

namespace llvm {
  class Function;
}  // namespace llvm

namespace art {

class CompilerDriver;

class CompiledCode {
 public:
  // For Quick to supply an code blob
  CompiledCode(CompilerDriver* compiler_driver, InstructionSet instruction_set,
               const std::vector<uint8_t>& code);

  // For Portable to supply an ELF object
  CompiledCode(CompilerDriver* compiler_driver, InstructionSet instruction_set,
               const std::string& elf_object, const std::string &symbol);

  InstructionSet GetInstructionSet() const {
    return instruction_set_;
  }

  const std::vector<uint8_t>& GetCode() const {
    return *code_;
  }

  void SetCode(const std::vector<uint8_t>& code);

  bool operator==(const CompiledCode& rhs) const {
    return (code_ == rhs.code_);
  }

  // To align an offset from a page-aligned value to make it suitable
  // for code storage. For example on ARM, to ensure that PC relative
  // valu computations work out as expected.
  uint32_t AlignCode(uint32_t offset) const;
  static uint32_t AlignCode(uint32_t offset, InstructionSet instruction_set);

  // returns the difference between the code address and a usable PC.
  // mainly to cope with kThumb2 where the lower bit must be set.
  size_t CodeDelta() const;

  // Returns a pointer suitable for invoking the code at the argument
  // code_pointer address.  Mainly to cope with kThumb2 where the
  // lower bit must be set to indicate Thumb mode.
  static const void* CodePointer(const void* code_pointer,
                                 InstructionSet instruction_set);

#if defined(ART_USE_PORTABLE_COMPILER)
  const std::string& GetSymbol() const;
  const std::vector<uint32_t>& GetOatdataOffsetsToCompliledCodeOffset() const;
  void AddOatdataOffsetToCompliledCodeOffset(uint32_t offset);
#endif

 private:
  CompilerDriver* compiler_driver_;

  const InstructionSet instruction_set_;

  // Used to store the PIC code for Quick and an ELF image for portable.
  std::vector<uint8_t>* code_;

  // Used for the Portable ELF symbol name.
  const std::string symbol_;

  // There are offsets from the oatdata symbol to where the offset to
  // the compiled method will be found. These are computed by the
  // OatWriter and then used by the ElfWriter to add relocations so
  // that MCLinker can update the values to the location in the linked .so.
  std::vector<uint32_t> oatdata_offsets_to_compiled_code_offset_;
};

class CompiledMethod : public CompiledCode {
 public:
  // Constructs a CompiledMethod for the non-LLVM compilers.
  CompiledMethod(CompilerDriver& driver,
                 InstructionSet instruction_set,
                 const std::vector<uint8_t>& code,
                 const size_t frame_size_in_bytes,
                 const uint32_t core_spill_mask,
                 const uint32_t fp_spill_mask,
                 const std::vector<uint8_t>& mapping_table,
                 const std::vector<uint8_t>& vmap_table,
                 const std::vector<uint8_t>& native_gc_map);

  // Constructs a CompiledMethod for the JniCompiler.
  CompiledMethod(CompilerDriver& driver,
                 InstructionSet instruction_set,
                 const std::vector<uint8_t>& code,
                 const size_t frame_size_in_bytes,
                 const uint32_t core_spill_mask,
                 const uint32_t fp_spill_mask);

  // Constructs a CompiledMethod for the Portable compiler.
  CompiledMethod(CompilerDriver& driver, InstructionSet instruction_set, const std::string& code,
                 const std::vector<uint8_t>& gc_map, const std::string& symbol);

  // Constructs a CompiledMethod for the Portable JniCompiler.
  CompiledMethod(CompilerDriver& driver, InstructionSet instruction_set, const std::string& code,
                 const std::string& symbol);

  ~CompiledMethod() {}

  size_t GetFrameSizeInBytes() const {
    return frame_size_in_bytes_;
  }

  uint32_t GetCoreSpillMask() const {
    return core_spill_mask_;
  }

  uint32_t GetFpSpillMask() const {
    return fp_spill_mask_;
  }

  const std::vector<uint8_t>& GetMappingTable() const {
    DCHECK(mapping_table_ != nullptr);
    return *mapping_table_;
  }

  const std::vector<uint8_t>& GetVmapTable() const {
    DCHECK(vmap_table_ != nullptr);
    return *vmap_table_;
  }

  const std::vector<uint8_t>& GetGcMap() const {
    DCHECK(gc_map_ != nullptr);
    return *gc_map_;
  }

 private:
  // For quick code, the size of the activation used by the code.
  const size_t frame_size_in_bytes_;
  // For quick code, a bit mask describing spilled GPR callee-save registers.
  const uint32_t core_spill_mask_;
  // For quick code, a bit mask describing spilled FPR callee-save registers.
  const uint32_t fp_spill_mask_;
  // For quick code, a uleb128 encoded map from native PC offset to dex PC aswell as dex PC to
  // native PC offset. Size prefixed.
  std::vector<uint8_t>* mapping_table_;
  // For quick code, a uleb128 encoded map from GPR/FPR register to dex register. Size prefixed.
  std::vector<uint8_t>* vmap_table_;
  // For quick code, a map keyed by native PC indices to bitmaps describing what dalvik registers
  // are live. For portable code, the key is a dalvik PC.
  std::vector<uint8_t>* gc_map_;
};

}  // namespace art

#endif  // ART_COMPILER_COMPILED_METHOD_H_
