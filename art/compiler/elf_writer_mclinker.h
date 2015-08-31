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

#ifndef ART_COMPILER_ELF_WRITER_MCLINKER_H_
#define ART_COMPILER_ELF_WRITER_MCLINKER_H_

#include "elf_writer.h"

#include "UniquePtr.h"
#include "safe_map.h"

namespace mcld {
class IRBuilder;
class Input;
class LDSection;
class LDSymbol;
class Linker;
class LinkerConfig;
class Module;
}  // namespace mcld

namespace art {

class CompiledCode;

class ElfWriterMclinker : public ElfWriter {
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
  ElfWriterMclinker(const CompilerDriver& driver, File* elf_file);
  ~ElfWriterMclinker();

  void Init();
  void AddOatInput(std::vector<uint8_t>& oat_contents);
  void AddMethodInputs(const std::vector<const DexFile*>& dex_files);
  void AddCompiledCodeInput(const CompiledCode& compiled_code);
  void AddRuntimeInputs(const std::string& android_root, bool is_host);
  bool Link();
#if defined(ART_USE_PORTABLE_COMPILER)
  void FixupOatMethodOffsets(const std::vector<const DexFile*>& dex_files)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  uint32_t FixupCompiledCodeOffset(ElfFile& elf_file,
                                   llvm::ELF::Elf32_Addr oatdata_address,
                                   const CompiledCode& compiled_code);
#endif

  // Setup by Init()
  UniquePtr<mcld::LinkerConfig> linker_config_;
  UniquePtr<mcld::Module> module_;
  UniquePtr<mcld::IRBuilder> ir_builder_;
  UniquePtr<mcld::Linker> linker_;

  // Setup by AddOatInput()
  // TODO: ownership of oat_input_?
  mcld::Input* oat_input_;

  // Setup by AddCompiledCodeInput
  // set of symbols for already added mcld::Inputs
  SafeMap<const std::string*, const std::string*> added_symbols_;

  // Setup by FixupCompiledCodeOffset
  // map of symbol names to oatdata offset
  SafeMap<const std::string*, uint32_t> symbol_to_compiled_code_offset_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ElfWriterMclinker);
};

}  // namespace art

#endif  // ART_COMPILER_ELF_WRITER_MCLINKER_H_
