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

#include "elf_file.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "utils.h"

namespace art {

ElfFile::ElfFile()
  : file_(NULL),
    writable_(false),
    program_header_only_(false),
    header_(NULL),
    base_address_(NULL),
    program_headers_start_(NULL),
    section_headers_start_(NULL),
    dynamic_program_header_(NULL),
    dynamic_section_start_(NULL),
    symtab_section_start_(NULL),
    dynsym_section_start_(NULL),
    strtab_section_start_(NULL),
    dynstr_section_start_(NULL),
    hash_section_start_(NULL),
    symtab_symbol_table_(NULL),
    dynsym_symbol_table_(NULL) {}

ElfFile* ElfFile::Open(File* file, bool writable, bool program_header_only) {
  UniquePtr<ElfFile> elf_file(new ElfFile());
  if (!elf_file->Setup(file, writable, program_header_only)) {
    return NULL;
  }
  return elf_file.release();
}

bool ElfFile::Setup(File* file, bool writable, bool program_header_only) {
  CHECK(file != NULL);
  file_ = file;
  writable_ = writable;
  program_header_only_ = program_header_only;

  int prot;
  int flags;
  if (writable_) {
    prot = PROT_READ | PROT_WRITE;
    flags = MAP_SHARED;
  } else {
    prot = PROT_READ;
    flags = MAP_PRIVATE;
  }
  int64_t file_length = file_->GetLength();
  if (file_length < 0) {
    errno = -file_length;
    PLOG(WARNING) << "Failed to get length of file: " << file_->GetPath() << " fd=" << file_->Fd();
    return false;
  }
  if (file_length < sizeof(llvm::ELF::Elf32_Ehdr)) {
    if (writable) {
      LOG(WARNING) << "File size of " << file_length
                   << " bytes not large enough to contain ELF header of "
                   << sizeof(llvm::ELF::Elf32_Ehdr) << " bytes: " << file_->GetPath();
    }
    return false;
  }

  if (program_header_only) {
    // first just map ELF header to get program header size information
    size_t elf_header_size = sizeof(llvm::ELF::Elf32_Ehdr);
    if (!SetMap(MemMap::MapFile(elf_header_size, prot, flags, file_->Fd(), 0))) {
      return false;
    }
    // then remap to cover program header
    size_t program_header_size = header_->e_phoff + (header_->e_phentsize * header_->e_phnum);
    if (file_length < program_header_size) {
      LOG(WARNING) << "File size of " << file_length
                   << " bytes not large enough to contain ELF program header of "
                   << program_header_size << " bytes: " << file_->GetPath();
      return false;
    }
    if (!SetMap(MemMap::MapFile(program_header_size, prot, flags, file_->Fd(), 0))) {
      LOG(WARNING) << "Failed to map ELF program headers: " << file_->GetPath();
      return false;
    }
  } else {
    // otherwise map entire file
    if (!SetMap(MemMap::MapFile(file_->GetLength(), prot, flags, file_->Fd(), 0))) {
      LOG(WARNING) << "Failed to map ELF file: " << file_->GetPath();
      return false;
    }
  }

  // Either way, the program header is relative to the elf header
  program_headers_start_ = Begin() + GetHeader().e_phoff;

  if (!program_header_only) {
    // Setup section headers.
    section_headers_start_ = Begin() + GetHeader().e_shoff;

    // Find .dynamic section info from program header
    dynamic_program_header_ = FindProgamHeaderByType(llvm::ELF::PT_DYNAMIC);
    if (dynamic_program_header_ == NULL) {
      LOG(WARNING) << "Failed to find PT_DYNAMIC program header in ELF file: " << file_->GetPath();
      return false;
    }

    dynamic_section_start_
        = reinterpret_cast<llvm::ELF::Elf32_Dyn*>(Begin() + GetDynamicProgramHeader().p_offset);

    // Find other sections from section headers
    for (llvm::ELF::Elf32_Word i = 0; i < GetSectionHeaderNum(); i++) {
      llvm::ELF::Elf32_Shdr& section_header = GetSectionHeader(i);
      byte* section_addr = Begin() + section_header.sh_offset;
      switch (section_header.sh_type) {
        case llvm::ELF::SHT_SYMTAB: {
          symtab_section_start_ = reinterpret_cast<llvm::ELF::Elf32_Sym*>(section_addr);
          break;
        }
        case llvm::ELF::SHT_DYNSYM: {
          dynsym_section_start_ = reinterpret_cast<llvm::ELF::Elf32_Sym*>(section_addr);
          break;
        }
        case llvm::ELF::SHT_STRTAB: {
          // TODO: base these off of sh_link from .symtab and .dynsym above
          if ((section_header.sh_flags & llvm::ELF::SHF_ALLOC) != 0) {
            dynstr_section_start_ = reinterpret_cast<char*>(section_addr);
          } else {
            strtab_section_start_ = reinterpret_cast<char*>(section_addr);
          }
          break;
        }
        case llvm::ELF::SHT_DYNAMIC: {
          if (reinterpret_cast<byte*>(dynamic_section_start_) != section_addr) {
            LOG(WARNING) << "Failed to find matching SHT_DYNAMIC for PT_DYNAMIC in "
                         << file_->GetPath() << ": " << std::hex
                         << reinterpret_cast<void*>(dynamic_section_start_)
                         << " != " << reinterpret_cast<void*>(section_addr);
            return false;
          }
          break;
        }
        case llvm::ELF::SHT_HASH: {
          hash_section_start_ = reinterpret_cast<llvm::ELF::Elf32_Word*>(section_addr);
          break;
        }
      }
    }
  }
  return true;
}

ElfFile::~ElfFile() {
  STLDeleteElements(&segments_);
  delete symtab_symbol_table_;
  delete dynsym_symbol_table_;
}

bool ElfFile::SetMap(MemMap* map) {
  if (map == NULL) {
    // MemMap::Open should have already logged
    return false;
  }
  map_.reset(map);
  CHECK(map_.get() != NULL) << file_->GetPath();
  CHECK(map_->Begin() != NULL) << file_->GetPath();

  header_ = reinterpret_cast<llvm::ELF::Elf32_Ehdr*>(map_->Begin());
  if ((llvm::ELF::ElfMagic[0] != header_->e_ident[llvm::ELF::EI_MAG0])
      || (llvm::ELF::ElfMagic[1] != header_->e_ident[llvm::ELF::EI_MAG1])
      || (llvm::ELF::ElfMagic[2] != header_->e_ident[llvm::ELF::EI_MAG2])
      || (llvm::ELF::ElfMagic[3] != header_->e_ident[llvm::ELF::EI_MAG3])) {
    LOG(WARNING) << "Failed to find ELF magic in " << file_->GetPath()
                 << ": " << std::hex
                 << static_cast<uint8_t>(header_->e_ident[llvm::ELF::EI_MAG0])
                 << static_cast<uint8_t>(header_->e_ident[llvm::ELF::EI_MAG1])
                 << static_cast<uint8_t>(header_->e_ident[llvm::ELF::EI_MAG2])
                 << static_cast<uint8_t>(header_->e_ident[llvm::ELF::EI_MAG3]);
    return false;
  }


  // TODO: remove these static_casts from enum when using -std=gnu++0x
  CHECK_EQ(static_cast<unsigned char>(llvm::ELF::ELFCLASS32),  header_->e_ident[llvm::ELF::EI_CLASS])   << file_->GetPath();
  CHECK_EQ(static_cast<unsigned char>(llvm::ELF::ELFDATA2LSB), header_->e_ident[llvm::ELF::EI_DATA])    << file_->GetPath();
  CHECK_EQ(static_cast<unsigned char>(llvm::ELF::EV_CURRENT),  header_->e_ident[llvm::ELF::EI_VERSION]) << file_->GetPath();

  // TODO: remove these static_casts from enum when using -std=gnu++0x
  CHECK_EQ(static_cast<llvm::ELF::Elf32_Half>(llvm::ELF::ET_DYN), header_->e_type) << file_->GetPath();
  CHECK_EQ(static_cast<llvm::ELF::Elf32_Word>(llvm::ELF::EV_CURRENT), header_->e_version) << file_->GetPath();
  CHECK_EQ(0U, header_->e_entry) << file_->GetPath();

  CHECK_NE(0U, header_->e_phoff) << file_->GetPath();
  CHECK_NE(0U, header_->e_shoff) << file_->GetPath();
  CHECK_NE(0U, header_->e_ehsize) << file_->GetPath();
  CHECK_NE(0U, header_->e_phentsize) << file_->GetPath();
  CHECK_NE(0U, header_->e_phnum) << file_->GetPath();
  CHECK_NE(0U, header_->e_shentsize) << file_->GetPath();
  CHECK_NE(0U, header_->e_shnum) << file_->GetPath();
  CHECK_NE(0U, header_->e_shstrndx) << file_->GetPath();
  CHECK_GE(header_->e_shnum, header_->e_shstrndx) << file_->GetPath();
  if (!program_header_only_) {
    CHECK_GT(Size(), header_->e_phoff) << file_->GetPath();
    CHECK_GT(Size(), header_->e_shoff) << file_->GetPath();
  }
  return true;
}


llvm::ELF::Elf32_Ehdr& ElfFile::GetHeader() {
  CHECK(header_ != NULL);
  return *header_;
}

byte* ElfFile::GetProgramHeadersStart() {
  CHECK(program_headers_start_ != NULL);
  return program_headers_start_;
}

byte* ElfFile::GetSectionHeadersStart() {
  CHECK(section_headers_start_ != NULL);
  return section_headers_start_;
}

llvm::ELF::Elf32_Phdr& ElfFile::GetDynamicProgramHeader() {
  CHECK(dynamic_program_header_ != NULL);
  return *dynamic_program_header_;
}

llvm::ELF::Elf32_Dyn* ElfFile::GetDynamicSectionStart() {
  CHECK(dynamic_section_start_ != NULL);
  return dynamic_section_start_;
}

llvm::ELF::Elf32_Sym* ElfFile::GetSymbolSectionStart(llvm::ELF::Elf32_Word section_type) {
  CHECK(IsSymbolSectionType(section_type)) << file_->GetPath() << " " << section_type;
  llvm::ELF::Elf32_Sym* symbol_section_start;
  switch (section_type) {
    case llvm::ELF::SHT_SYMTAB: {
      symbol_section_start = symtab_section_start_;
      break;
    }
    case llvm::ELF::SHT_DYNSYM: {
      symbol_section_start = dynsym_section_start_;
      break;
    }
    default: {
      LOG(FATAL) << section_type;
      symbol_section_start = NULL;
    }
  }
  CHECK(symbol_section_start != NULL);
  return symbol_section_start;
}

const char* ElfFile::GetStringSectionStart(llvm::ELF::Elf32_Word section_type) {
  CHECK(IsSymbolSectionType(section_type)) << file_->GetPath() << " " << section_type;
  const char* string_section_start;
  switch (section_type) {
    case llvm::ELF::SHT_SYMTAB: {
      string_section_start = strtab_section_start_;
      break;
    }
    case llvm::ELF::SHT_DYNSYM: {
      string_section_start = dynstr_section_start_;
      break;
    }
    default: {
      LOG(FATAL) << section_type;
      string_section_start = NULL;
    }
  }
  CHECK(string_section_start != NULL);
  return string_section_start;
}

const char* ElfFile::GetString(llvm::ELF::Elf32_Word section_type, llvm::ELF::Elf32_Word i) {
  CHECK(IsSymbolSectionType(section_type)) << file_->GetPath() << " " << section_type;
  if (i == 0) {
    return NULL;
  }
  const char* string_section_start = GetStringSectionStart(section_type);
  const char* string = string_section_start + i;
  return string;
}

llvm::ELF::Elf32_Word* ElfFile::GetHashSectionStart() {
  CHECK(hash_section_start_ != NULL);
  return hash_section_start_;
}

llvm::ELF::Elf32_Word ElfFile::GetHashBucketNum() {
  return GetHashSectionStart()[0];
}

llvm::ELF::Elf32_Word ElfFile::GetHashChainNum() {
  return GetHashSectionStart()[1];
}

llvm::ELF::Elf32_Word ElfFile::GetHashBucket(size_t i) {
  CHECK_LT(i, GetHashBucketNum());
  // 0 is nbucket, 1 is nchain
  return GetHashSectionStart()[2 + i];
}

llvm::ELF::Elf32_Word ElfFile::GetHashChain(size_t i) {
  CHECK_LT(i, GetHashChainNum());
  // 0 is nbucket, 1 is nchain, & chains are after buckets
  return GetHashSectionStart()[2 + GetHashBucketNum() + i];
}

llvm::ELF::Elf32_Word ElfFile::GetProgramHeaderNum() {
  return GetHeader().e_phnum;
}

llvm::ELF::Elf32_Phdr& ElfFile::GetProgramHeader(llvm::ELF::Elf32_Word i) {
  CHECK_LT(i, GetProgramHeaderNum()) << file_->GetPath();
  byte* program_header = GetProgramHeadersStart() + (i * GetHeader().e_phentsize);
  CHECK_LT(program_header, End()) << file_->GetPath();
  return *reinterpret_cast<llvm::ELF::Elf32_Phdr*>(program_header);
}

llvm::ELF::Elf32_Phdr* ElfFile::FindProgamHeaderByType(llvm::ELF::Elf32_Word type) {
  for (llvm::ELF::Elf32_Word i = 0; i < GetProgramHeaderNum(); i++) {
    llvm::ELF::Elf32_Phdr& program_header = GetProgramHeader(i);
    if (program_header.p_type == type) {
      return &program_header;
    }
  }
  return NULL;
}

llvm::ELF::Elf32_Word ElfFile::GetSectionHeaderNum() {
  return GetHeader().e_shnum;
}

llvm::ELF::Elf32_Shdr& ElfFile::GetSectionHeader(llvm::ELF::Elf32_Word i) {
  // Can only access arbitrary sections when we have the whole file, not just program header.
  // Even if we Load(), it doesn't bring in all the sections.
  CHECK(!program_header_only_) << file_->GetPath();
  CHECK_LT(i, GetSectionHeaderNum()) << file_->GetPath();
  byte* section_header = GetSectionHeadersStart() + (i * GetHeader().e_shentsize);
  CHECK_LT(section_header, End()) << file_->GetPath();
  return *reinterpret_cast<llvm::ELF::Elf32_Shdr*>(section_header);
}

llvm::ELF::Elf32_Shdr* ElfFile::FindSectionByType(llvm::ELF::Elf32_Word type) {
  // Can only access arbitrary sections when we have the whole file, not just program header.
  // We could change this to switch on known types if they were detected during loading.
  CHECK(!program_header_only_) << file_->GetPath();
  for (llvm::ELF::Elf32_Word i = 0; i < GetSectionHeaderNum(); i++) {
    llvm::ELF::Elf32_Shdr& section_header = GetSectionHeader(i);
    if (section_header.sh_type == type) {
      return &section_header;
    }
  }
  return NULL;
}

// from bionic
static unsigned elfhash(const char *_name) {
  const unsigned char *name = (const unsigned char *) _name;
  unsigned h = 0, g;

  while (*name) {
    h = (h << 4) + *name++;
    g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }
  return h;
}

llvm::ELF::Elf32_Shdr& ElfFile::GetSectionNameStringSection() {
  return GetSectionHeader(GetHeader().e_shstrndx);
}

byte* ElfFile::FindDynamicSymbolAddress(const std::string& symbol_name) {
  llvm::ELF::Elf32_Word hash = elfhash(symbol_name.c_str());
  llvm::ELF::Elf32_Word bucket_index = hash % GetHashBucketNum();
  llvm::ELF::Elf32_Word symbol_and_chain_index = GetHashBucket(bucket_index);
  while (symbol_and_chain_index != 0 /* STN_UNDEF */) {
    llvm::ELF::Elf32_Sym& symbol = GetSymbol(llvm::ELF::SHT_DYNSYM, symbol_and_chain_index);
    const char* name = GetString(llvm::ELF::SHT_DYNSYM, symbol.st_name);
    if (symbol_name == name) {
      return base_address_ + symbol.st_value;
    }
    symbol_and_chain_index = GetHashChain(symbol_and_chain_index);
  }
  return NULL;
}

bool ElfFile::IsSymbolSectionType(llvm::ELF::Elf32_Word section_type) {
  return ((section_type == llvm::ELF::SHT_SYMTAB) || (section_type == llvm::ELF::SHT_DYNSYM));
}

llvm::ELF::Elf32_Word ElfFile::GetSymbolNum(llvm::ELF::Elf32_Shdr& section_header) {
  CHECK(IsSymbolSectionType(section_header.sh_type)) << file_->GetPath() << " " << section_header.sh_type;
  CHECK_NE(0U, section_header.sh_entsize) << file_->GetPath();
  return section_header.sh_size / section_header.sh_entsize;
}

llvm::ELF::Elf32_Sym& ElfFile::GetSymbol(llvm::ELF::Elf32_Word section_type,
                                         llvm::ELF::Elf32_Word i) {
  return *(GetSymbolSectionStart(section_type) + i);
}

ElfFile::SymbolTable** ElfFile::GetSymbolTable(llvm::ELF::Elf32_Word section_type) {
  CHECK(IsSymbolSectionType(section_type)) << file_->GetPath() << " " << section_type;
  switch (section_type) {
    case llvm::ELF::SHT_SYMTAB: {
      return &symtab_symbol_table_;
    }
    case llvm::ELF::SHT_DYNSYM: {
      return &dynsym_symbol_table_;
    }
    default: {
      LOG(FATAL) << section_type;
      return NULL;
    }
  }
}

llvm::ELF::Elf32_Sym* ElfFile::FindSymbolByName(llvm::ELF::Elf32_Word section_type,
                                                const std::string& symbol_name,
                                                bool build_map) {
  CHECK(!program_header_only_) << file_->GetPath();
  CHECK(IsSymbolSectionType(section_type)) << file_->GetPath() << " " << section_type;

  SymbolTable** symbol_table = GetSymbolTable(section_type);
  if (*symbol_table != NULL || build_map) {
    if (*symbol_table == NULL) {
      DCHECK(build_map);
      *symbol_table = new SymbolTable;
      llvm::ELF::Elf32_Shdr* symbol_section = FindSectionByType(section_type);
      CHECK(symbol_section != NULL) << file_->GetPath();
      llvm::ELF::Elf32_Shdr& string_section = GetSectionHeader(symbol_section->sh_link);
      for (uint32_t i = 0; i < GetSymbolNum(*symbol_section); i++) {
        llvm::ELF::Elf32_Sym& symbol = GetSymbol(section_type, i);
        unsigned char type = symbol.getType();
        if (type == llvm::ELF::STT_NOTYPE) {
          continue;
        }
        const char* name = GetString(string_section, symbol.st_name);
        if (name == NULL) {
          continue;
        }
        std::pair<SymbolTable::iterator, bool> result = (*symbol_table)->insert(std::make_pair(name, &symbol));
        if (!result.second) {
          // If a duplicate, make sure it has the same logical value. Seen on x86.
          CHECK_EQ(symbol.st_value, result.first->second->st_value);
          CHECK_EQ(symbol.st_size, result.first->second->st_size);
          CHECK_EQ(symbol.st_info, result.first->second->st_info);
          CHECK_EQ(symbol.st_other, result.first->second->st_other);
          CHECK_EQ(symbol.st_shndx, result.first->second->st_shndx);
        }
      }
    }
    CHECK(*symbol_table != NULL);
    SymbolTable::const_iterator it = (*symbol_table)->find(symbol_name);
    if (it == (*symbol_table)->end()) {
      return NULL;
    }
    return it->second;
  }

  // Fall back to linear search
  llvm::ELF::Elf32_Shdr* symbol_section = FindSectionByType(section_type);
  CHECK(symbol_section != NULL) << file_->GetPath();
  llvm::ELF::Elf32_Shdr& string_section = GetSectionHeader(symbol_section->sh_link);
  for (uint32_t i = 0; i < GetSymbolNum(*symbol_section); i++) {
    llvm::ELF::Elf32_Sym& symbol = GetSymbol(section_type, i);
    const char* name = GetString(string_section, symbol.st_name);
    if (name == NULL) {
      continue;
    }
    if (symbol_name == name) {
      return &symbol;
    }
  }
  return NULL;
}

llvm::ELF::Elf32_Addr ElfFile::FindSymbolAddress(llvm::ELF::Elf32_Word section_type,
                                                 const std::string& symbol_name,
                                                 bool build_map) {
  llvm::ELF::Elf32_Sym* symbol = FindSymbolByName(section_type, symbol_name, build_map);
  if (symbol == NULL) {
    return 0;
  }
  return symbol->st_value;
}

const char* ElfFile::GetString(llvm::ELF::Elf32_Shdr& string_section, llvm::ELF::Elf32_Word i) {
  CHECK(!program_header_only_) << file_->GetPath();
  // TODO: remove this static_cast from enum when using -std=gnu++0x
  CHECK_EQ(static_cast<llvm::ELF::Elf32_Word>(llvm::ELF::SHT_STRTAB), string_section.sh_type) << file_->GetPath();
  CHECK_LT(i, string_section.sh_size) << file_->GetPath();
  if (i == 0) {
    return NULL;
  }
  byte* strings = Begin() + string_section.sh_offset;
  byte* string = strings + i;
  CHECK_LT(string, End()) << file_->GetPath();
  return reinterpret_cast<const char*>(string);
}

llvm::ELF::Elf32_Word ElfFile::GetDynamicNum() {
  return GetDynamicProgramHeader().p_filesz / sizeof(llvm::ELF::Elf32_Dyn);
}

llvm::ELF::Elf32_Dyn& ElfFile::GetDynamic(llvm::ELF::Elf32_Word i) {
  CHECK_LT(i, GetDynamicNum()) << file_->GetPath();
  return *(GetDynamicSectionStart() + i);
}

llvm::ELF::Elf32_Word ElfFile::FindDynamicValueByType(llvm::ELF::Elf32_Sword type) {
  for (llvm::ELF::Elf32_Word i = 0; i < GetDynamicNum(); i++) {
    llvm::ELF::Elf32_Dyn& elf_dyn = GetDynamic(i);
    if (elf_dyn.d_tag == type) {
      return elf_dyn.d_un.d_val;
    }
  }
  return 0;
}

llvm::ELF::Elf32_Rel* ElfFile::GetRelSectionStart(llvm::ELF::Elf32_Shdr& section_header) {
  CHECK(llvm::ELF::SHT_REL == section_header.sh_type) << file_->GetPath() << " " << section_header.sh_type;
  return reinterpret_cast<llvm::ELF::Elf32_Rel*>(Begin() + section_header.sh_offset);
}

llvm::ELF::Elf32_Word ElfFile::GetRelNum(llvm::ELF::Elf32_Shdr& section_header) {
  CHECK(llvm::ELF::SHT_REL == section_header.sh_type) << file_->GetPath() << " " << section_header.sh_type;
  CHECK_NE(0U, section_header.sh_entsize) << file_->GetPath();
  return section_header.sh_size / section_header.sh_entsize;
}

llvm::ELF::Elf32_Rel& ElfFile::GetRel(llvm::ELF::Elf32_Shdr& section_header, llvm::ELF::Elf32_Word i) {
  CHECK(llvm::ELF::SHT_REL == section_header.sh_type) << file_->GetPath() << " " << section_header.sh_type;
  CHECK_LT(i, GetRelNum(section_header)) << file_->GetPath();
  return *(GetRelSectionStart(section_header) + i);
}

llvm::ELF::Elf32_Rela* ElfFile::GetRelaSectionStart(llvm::ELF::Elf32_Shdr& section_header) {
  CHECK(llvm::ELF::SHT_RELA == section_header.sh_type) << file_->GetPath() << " " << section_header.sh_type;
  return reinterpret_cast<llvm::ELF::Elf32_Rela*>(Begin() + section_header.sh_offset);
}

llvm::ELF::Elf32_Word ElfFile::GetRelaNum(llvm::ELF::Elf32_Shdr& section_header) {
  CHECK(llvm::ELF::SHT_RELA == section_header.sh_type) << file_->GetPath() << " " << section_header.sh_type;
  return section_header.sh_size / section_header.sh_entsize;
}

llvm::ELF::Elf32_Rela& ElfFile::GetRela(llvm::ELF::Elf32_Shdr& section_header,
                                        llvm::ELF::Elf32_Word i) {
  CHECK(llvm::ELF::SHT_RELA == section_header.sh_type) << file_->GetPath() << " " << section_header.sh_type;
  CHECK_LT(i, GetRelaNum(section_header)) << file_->GetPath();
  return *(GetRelaSectionStart(section_header) + i);
}

// Base on bionic phdr_table_get_load_size
size_t ElfFile::GetLoadedSize() {
  llvm::ELF::Elf32_Addr min_vaddr = 0xFFFFFFFFu;
  llvm::ELF::Elf32_Addr max_vaddr = 0x00000000u;
  for (llvm::ELF::Elf32_Word i = 0; i < GetProgramHeaderNum(); i++) {
    llvm::ELF::Elf32_Phdr& program_header = GetProgramHeader(i);
    if (program_header.p_type != llvm::ELF::PT_LOAD) {
      continue;
    }
    llvm::ELF::Elf32_Addr begin_vaddr = program_header.p_vaddr;
    if (begin_vaddr < min_vaddr) {
       min_vaddr = begin_vaddr;
    }
    llvm::ELF::Elf32_Addr end_vaddr = program_header.p_vaddr + program_header.p_memsz;
    if (end_vaddr > max_vaddr) {
      max_vaddr = end_vaddr;
    }
  }
  min_vaddr = RoundDown(min_vaddr, kPageSize);
  max_vaddr = RoundUp(max_vaddr, kPageSize);
  CHECK_LT(min_vaddr, max_vaddr) << file_->GetPath();
  size_t loaded_size = max_vaddr - min_vaddr;
  return loaded_size;
}

bool ElfFile::Load(bool executable) {
  // TODO: actually return false error
  CHECK(program_header_only_) << file_->GetPath();
  for (llvm::ELF::Elf32_Word i = 0; i < GetProgramHeaderNum(); i++) {
    llvm::ELF::Elf32_Phdr& program_header = GetProgramHeader(i);

    // Record .dynamic header information for later use
    if (program_header.p_type == llvm::ELF::PT_DYNAMIC) {
      dynamic_program_header_ = &program_header;
      continue;
    }

    // Not something to load, move on.
    if (program_header.p_type != llvm::ELF::PT_LOAD) {
      continue;
    }

    // Found something to load.

    // If p_vaddr is zero, it must be the first loadable segment,
    // since they must be in order.  Since it is zero, there isn't a
    // specific address requested, so first request a contiguous chunk
    // of required size for all segments, but with no
    // permissions. We'll then carve that up with the proper
    // permissions as we load the actual segments. If p_vaddr is
    // non-zero, the segments require the specific address specified,
    // which either was specified in the file because we already set
    // base_address_ after the first zero segment).
    int64_t file_length = file_->GetLength();
    if (program_header.p_vaddr == 0) {
      std::string reservation_name("ElfFile reservation for ");
      reservation_name += file_->GetPath();
      UniquePtr<MemMap> reserve(MemMap::MapAnonymous(reservation_name.c_str(),
                                                     NULL, GetLoadedSize(), PROT_NONE));
      CHECK(reserve.get() != NULL) << file_->GetPath();
      base_address_ = reserve->Begin();
      segments_.push_back(reserve.release());
    }
    // empty segment, nothing to map
    if (program_header.p_memsz == 0) {
      continue;
    }
    byte* p_vaddr = base_address_ + program_header.p_vaddr;
    int prot = 0;
    if (executable && ((program_header.p_flags & llvm::ELF::PF_X) != 0)) {
      prot |= PROT_EXEC;
    }
    if ((program_header.p_flags & llvm::ELF::PF_W) != 0) {
      prot |= PROT_WRITE;
    }
    if ((program_header.p_flags & llvm::ELF::PF_R) != 0) {
      prot |= PROT_READ;
    }
    int flags = MAP_FIXED;
    if (writable_) {
      prot |= PROT_WRITE;
      flags |= MAP_SHARED;
    } else {
      flags |= MAP_PRIVATE;
    }
    if (file_length < (program_header.p_offset + program_header.p_memsz)) {
      LOG(WARNING) << "File size of " << file_length
                   << " bytes not large enough to contain ELF segment " << i
                   << " of " << (program_header.p_offset + program_header.p_memsz)
                   << " bytes: " << file_->GetPath();
      return false;
    }
    UniquePtr<MemMap> segment(MemMap::MapFileAtAddress(p_vaddr,
                                                       program_header.p_memsz,
                                                       prot, flags, file_->Fd(),
                                                       program_header.p_offset,
                                                       true));
    CHECK(segment.get() != NULL) << file_->GetPath();
    CHECK_EQ(segment->Begin(), p_vaddr) << file_->GetPath();
    segments_.push_back(segment.release());
  }

  // Now that we are done loading, .dynamic should be in memory to find .dynstr, .dynsym, .hash
  dynamic_section_start_
      = reinterpret_cast<llvm::ELF::Elf32_Dyn*>(base_address_ + GetDynamicProgramHeader().p_vaddr);
  for (llvm::ELF::Elf32_Word i = 0; i < GetDynamicNum(); i++) {
    llvm::ELF::Elf32_Dyn& elf_dyn = GetDynamic(i);
    byte* d_ptr = base_address_ + elf_dyn.d_un.d_ptr;
    switch (elf_dyn.d_tag) {
      case llvm::ELF::DT_HASH: {
        hash_section_start_ = reinterpret_cast<llvm::ELF::Elf32_Word*>(d_ptr);
        break;
      }
      case llvm::ELF::DT_STRTAB: {
        dynstr_section_start_ = reinterpret_cast<char*>(d_ptr);
        break;
      }
      case llvm::ELF::DT_SYMTAB: {
        dynsym_section_start_ = reinterpret_cast<llvm::ELF::Elf32_Sym*>(d_ptr);
        break;
      }
      case llvm::ELF::DT_NULL: {
        CHECK_EQ(GetDynamicNum(), i+1);
        break;
      }
    }
  }

  return true;
}

}  // namespace art
