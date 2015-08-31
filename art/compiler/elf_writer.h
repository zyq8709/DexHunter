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

#ifndef ART_COMPILER_ELF_WRITER_H_
#define ART_COMPILER_ELF_WRITER_H_

#include <stdint.h>

#include <cstddef>
#include <string>
#include <vector>

#include <llvm/Support/ELF.h>

#include "base/macros.h"
#include "os.h"

namespace art {

class CompilerDriver;
class DexFile;
class ElfFile;
class OatWriter;

class ElfWriter {
 public:
  // Looks up information about location of oat file in elf file container.
  // Used for ImageWriter to perform memory layout.
  static void GetOatElfInformation(File* file,
                                   size_t& oat_loaded_size,
                                   size_t& oat_data_offset);

  // Returns runtime oat_data runtime address for an opened ElfFile.
  static llvm::ELF::Elf32_Addr GetOatDataAddress(ElfFile* elf_file);

 protected:
  ElfWriter(const CompilerDriver& driver, File* elf_file);
  virtual ~ElfWriter();

  virtual bool Write(OatWriter& oat_writer,
                     const std::vector<const DexFile*>& dex_files,
                     const std::string& android_root,
                     bool is_host)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  // Setup by constructor
  const CompilerDriver* compiler_driver_;
  File* elf_file_;
};

}  // namespace art

#endif  // ART_COMPILER_ELF_WRITER_H_
