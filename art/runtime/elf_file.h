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

#ifndef ART_RUNTIME_ELF_FILE_H_
#define ART_RUNTIME_ELF_FILE_H_

#include <map>
#include <vector>

#include <llvm/Support/ELF.h>

#include "base/unix_file/fd_file.h"
#include "globals.h"
#include "mem_map.h"
#include "os.h"
#include "UniquePtr.h"

namespace art {

// Used for compile time and runtime for ElfFile access. Because of
// the need for use at runtime, cannot directly use LLVM classes such as
// ELFObjectFile.
class ElfFile {
 public:
  static ElfFile* Open(File* file, bool writable, bool program_header_only);
  ~ElfFile();

  // Load segments into memory based on PT_LOAD program headers

  File& GetFile() const {
    return *file_;
  }

  byte* Begin() {
    return map_->Begin();
  }

  byte* End() {
    return map_->End();
  }

  size_t Size() const {
    return map_->Size();
  }

  ::llvm::ELF::Elf32_Ehdr& GetHeader();

  ::llvm::ELF::Elf32_Word GetProgramHeaderNum();
  ::llvm::ELF::Elf32_Phdr& GetProgramHeader(::llvm::ELF::Elf32_Word);
  ::llvm::ELF::Elf32_Phdr* FindProgamHeaderByType(::llvm::ELF::Elf32_Word type);

  ::llvm::ELF::Elf32_Word GetSectionHeaderNum();
  ::llvm::ELF::Elf32_Shdr& GetSectionHeader(::llvm::ELF::Elf32_Word);
  ::llvm::ELF::Elf32_Shdr* FindSectionByType(::llvm::ELF::Elf32_Word type);

  ::llvm::ELF::Elf32_Shdr& GetSectionNameStringSection();

  // Find .dynsym using .hash for more efficient lookup than FindSymbolAddress.
  byte* FindDynamicSymbolAddress(const std::string& symbol_name);

  static bool IsSymbolSectionType(::llvm::ELF::Elf32_Word section_type);
  ::llvm::ELF::Elf32_Word GetSymbolNum(::llvm::ELF::Elf32_Shdr&);
  ::llvm::ELF::Elf32_Sym& GetSymbol(::llvm::ELF::Elf32_Word section_type, ::llvm::ELF::Elf32_Word i);

  // Find symbol in specified table, returning NULL if it is not found.
  //
  // If build_map is true, builds a map to speed repeated access. The
  // map does not included untyped symbol values (aka STT_NOTYPE)
  // since they can contain duplicates. If build_map is false, the map
  // will be used if it was already created. Typically build_map
  // should be set unless only a small number of symbols will be
  // looked up.
  ::llvm::ELF::Elf32_Sym* FindSymbolByName(::llvm::ELF::Elf32_Word section_type,
                                           const std::string& symbol_name,
                                           bool build_map);

  // Find address of symbol in specified table, returning 0 if it is
  // not found. See FindSymbolByName for an explanation of build_map.
  ::llvm::ELF::Elf32_Addr FindSymbolAddress(::llvm::ELF::Elf32_Word section_type,
                                            const std::string& symbol_name,
                                            bool build_map);

  // Lookup a string given string section and offset. Returns NULL for
  // special 0 offset.
  const char* GetString(::llvm::ELF::Elf32_Shdr&, ::llvm::ELF::Elf32_Word);

  // Lookup a string by section type. Returns NULL for special 0 offset.
  const char* GetString(::llvm::ELF::Elf32_Word section_type, ::llvm::ELF::Elf32_Word);

  ::llvm::ELF::Elf32_Word GetDynamicNum();
  ::llvm::ELF::Elf32_Dyn& GetDynamic(::llvm::ELF::Elf32_Word);
  ::llvm::ELF::Elf32_Word FindDynamicValueByType(::llvm::ELF::Elf32_Sword type);

  ::llvm::ELF::Elf32_Word GetRelNum(::llvm::ELF::Elf32_Shdr&);
  ::llvm::ELF::Elf32_Rel& GetRel(::llvm::ELF::Elf32_Shdr&, ::llvm::ELF::Elf32_Word);

  ::llvm::ELF::Elf32_Word GetRelaNum(::llvm::ELF::Elf32_Shdr&);
  ::llvm::ELF::Elf32_Rela& GetRela(::llvm::ELF::Elf32_Shdr&, ::llvm::ELF::Elf32_Word);

  // Returns the expected size when the file is loaded at runtime
  size_t GetLoadedSize();

  // Load segments into memory based on PT_LOAD program headers.
  // executable is true at run time, false at compile time.
  bool Load(bool executable);

 private:
  ElfFile();

  bool Setup(File* file, bool writable, bool program_header_only);

  bool SetMap(MemMap* map);

  byte* GetProgramHeadersStart();
  byte* GetSectionHeadersStart();
  ::llvm::ELF::Elf32_Phdr& GetDynamicProgramHeader();
  ::llvm::ELF::Elf32_Dyn* GetDynamicSectionStart();
  ::llvm::ELF::Elf32_Sym* GetSymbolSectionStart(::llvm::ELF::Elf32_Word section_type);
  const char* GetStringSectionStart(::llvm::ELF::Elf32_Word section_type);
  ::llvm::ELF::Elf32_Rel* GetRelSectionStart(::llvm::ELF::Elf32_Shdr&);
  ::llvm::ELF::Elf32_Rela* GetRelaSectionStart(::llvm::ELF::Elf32_Shdr&);
  ::llvm::ELF::Elf32_Word* GetHashSectionStart();
  ::llvm::ELF::Elf32_Word GetHashBucketNum();
  ::llvm::ELF::Elf32_Word GetHashChainNum();
  ::llvm::ELF::Elf32_Word GetHashBucket(size_t i);
  ::llvm::ELF::Elf32_Word GetHashChain(size_t i);

  typedef std::map<std::string, ::llvm::ELF::Elf32_Sym*> SymbolTable;
  SymbolTable** GetSymbolTable(::llvm::ELF::Elf32_Word section_type);

  File* file_;
  bool writable_;
  bool program_header_only_;

  // ELF header mapping. If program_header_only_ is false, will actually point to the entire elf file.
  UniquePtr<MemMap> map_;
  ::llvm::ELF::Elf32_Ehdr* header_;
  std::vector<MemMap*> segments_;

  // Pointer to start of first PT_LOAD program segment after Load() when program_header_only_ is true.
  byte* base_address_;

  // The program header should always available but use GetProgramHeadersStart() to be sure.
  byte* program_headers_start_;

  // Conditionally available values. Use accessors to ensure they exist if they are required.
  byte* section_headers_start_;
  ::llvm::ELF::Elf32_Phdr* dynamic_program_header_;
  ::llvm::ELF::Elf32_Dyn* dynamic_section_start_;
  ::llvm::ELF::Elf32_Sym* symtab_section_start_;
  ::llvm::ELF::Elf32_Sym* dynsym_section_start_;
  const char* strtab_section_start_;
  const char* dynstr_section_start_;
  ::llvm::ELF::Elf32_Word* hash_section_start_;

  SymbolTable* symtab_symbol_table_;
  SymbolTable* dynsym_symbol_table_;
};

}  // namespace art

#endif  // ART_RUNTIME_ELF_FILE_H_
