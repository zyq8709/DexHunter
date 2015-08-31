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

#ifndef ART_COMPILER_ELF_FIXUP_H_
#define ART_COMPILER_ELF_FIXUP_H_

#include <stdint.h>

#include "base/macros.h"
#include "os.h"

namespace art {

class ElfFile;

class ElfFixup {
 public:
  // Fixup an ELF file so that that oat header will be loaded at oat_begin.
  // Returns true on success, false on failure.
  static bool Fixup(File* file, uintptr_t oat_data_begin);

 private:
  // Fixup .dynamic d_ptr values for the expected base_address.
  static bool FixupDynamic(ElfFile& elf_file, uintptr_t base_address);

  // Fixup Elf32_Shdr p_vaddr to load at the desired address.
  static bool FixupSectionHeaders(ElfFile& elf_file, uintptr_t base_address);

  // Fixup Elf32_Phdr p_vaddr to load at the desired address.
  static bool FixupProgramHeaders(ElfFile& elf_file, uintptr_t base_address);

  // Fixup symbol table
  static bool FixupSymbols(ElfFile& elf_file, uintptr_t base_address, bool dynamic);

  // Fixup dynamic relocations
  static bool FixupRelocations(ElfFile& elf_file, uintptr_t base_address);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ElfFixup);
};

}  // namespace art

#endif  // ART_COMPILER_ELF_FIXUP_H_
