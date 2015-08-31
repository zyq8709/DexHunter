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

#ifndef ART_COMPILER_ELF_WRITER_QUICK_H_
#define ART_COMPILER_ELF_WRITER_QUICK_H_

#include "elf_writer.h"

namespace art {

class ElfWriterQuick : public ElfWriter {
 public:
  // Write an ELF file. Returns true on success, false on failure.
  static bool Create(File* file,
                     OatWriter& oat_writer,
                     const std::vector<const DexFile*>& dex_files,
                     const std::string& android_root,
                     bool is_host,
                     const CompilerDriver& driver)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 protected:
  virtual bool Write(OatWriter& oat_writer,
                     const std::vector<const DexFile*>& dex_files,
                     const std::string& android_root,
                     bool is_host)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  ElfWriterQuick(const CompilerDriver& driver, File* elf_file);
  ~ElfWriterQuick();

  DISALLOW_IMPLICIT_CONSTRUCTORS(ElfWriterQuick);
};

}  // namespace art

#endif  // ART_COMPILER_ELF_WRITER_QUICK_H_
