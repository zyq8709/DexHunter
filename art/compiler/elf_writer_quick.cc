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

#include "elf_writer_quick.h"

#include "base/logging.h"
#include "base/unix_file/fd_file.h"
#include "buffered_output_stream.h"
#include "driver/compiler_driver.h"
#include "file_output_stream.h"
#include "globals.h"
#include "oat.h"
#include "oat_writer.h"
#include "utils.h"

namespace art {

ElfWriterQuick::ElfWriterQuick(const CompilerDriver& driver, File* elf_file)
  : ElfWriter(driver, elf_file) {}

ElfWriterQuick::~ElfWriterQuick() {}

bool ElfWriterQuick::Create(File* elf_file,
                            OatWriter& oat_writer,
                            const std::vector<const DexFile*>& dex_files,
                            const std::string& android_root,
                            bool is_host,
                            const CompilerDriver& driver) {
  ElfWriterQuick elf_writer(driver, elf_file);
  return elf_writer.Write(oat_writer, dex_files, android_root, is_host);
}

bool ElfWriterQuick::Write(OatWriter& oat_writer,
                           const std::vector<const DexFile*>& dex_files_unused,
                           const std::string& android_root_unused,
                           bool is_host_unused) {
  const bool debug = false;
  // +-------------------------+
  // | Elf32_Ehdr              |
  // +-------------------------+
  // | Elf32_Phdr PHDR         |
  // | Elf32_Phdr LOAD R       | .dynsym .dynstr .hash .rodata
  // | Elf32_Phdr LOAD R X     | .text
  // | Elf32_Phdr LOAD RW      | .dynamic
  // | Elf32_Phdr DYNAMIC      | .dynamic
  // +-------------------------+
  // | .dynsym                 |
  // | Elf32_Sym  STN_UNDEF    |
  // | Elf32_Sym  oatdata      |
  // | Elf32_Sym  oatexec      |
  // | Elf32_Sym  oatlastword  |
  // +-------------------------+
  // | .dynstr                 |
  // | \0                      |
  // | oatdata\0               |
  // | oatexec\0               |
  // | oatlastword\0           |
  // | boot.oat\0              |
  // +-------------------------+
  // | .hash                   |
  // | Elf32_Word nbucket = 1  |
  // | Elf32_Word nchain  = 3  |
  // | Elf32_Word bucket[0] = 0|
  // | Elf32_Word chain[0]  = 1|
  // | Elf32_Word chain[1]  = 2|
  // | Elf32_Word chain[2]  = 3|
  // +-------------------------+
  // | .rodata                 |
  // | oatdata..oatexec-4      |
  // +-------------------------+
  // | .text                   |
  // | oatexec..oatlastword    |
  // +-------------------------+
  // | .dynamic                |
  // | Elf32_Dyn DT_SONAME     |
  // | Elf32_Dyn DT_HASH       |
  // | Elf32_Dyn DT_SYMTAB     |
  // | Elf32_Dyn DT_SYMENT     |
  // | Elf32_Dyn DT_STRTAB     |
  // | Elf32_Dyn DT_STRSZ      |
  // | Elf32_Dyn DT_NULL       |
  // +-------------------------+
  // | .shstrtab               |
  // | \0                      |
  // | .dynamic\0              |
  // | .dynsym\0               |
  // | .dynstr\0               |
  // | .hash\0                 |
  // | .rodata\0               |
  // | .text\0                 |
  // | .shstrtab\0             |
  // +-------------------------+
  // | Elf32_Shdr NULL         |
  // | Elf32_Shdr .dynsym      |
  // | Elf32_Shdr .dynstr      |
  // | Elf32_Shdr .hash        |
  // | Elf32_Shdr .text        |
  // | Elf32_Shdr .rodata      |
  // | Elf32_Shdr .dynamic     |
  // | Elf32_Shdr .shstrtab    |
  // +-------------------------+

  // phase 1: computing offsets
  uint32_t expected_offset = 0;

  // Elf32_Ehdr
  expected_offset += sizeof(llvm::ELF::Elf32_Ehdr);

  // PHDR
  uint32_t phdr_alignment = sizeof(llvm::ELF::Elf32_Word);
  uint32_t phdr_offset = expected_offset;
  const uint8_t PH_PHDR     = 0;
  const uint8_t PH_LOAD_R__ = 1;
  const uint8_t PH_LOAD_R_X = 2;
  const uint8_t PH_LOAD_RW_ = 3;
  const uint8_t PH_DYNAMIC  = 4;
  const uint8_t PH_NUM      = 5;
  uint32_t phdr_size = sizeof(llvm::ELF::Elf32_Phdr) * PH_NUM;
  expected_offset += phdr_size;
  if (debug) {
    LOG(INFO) << "phdr_offset=" << phdr_offset << std::hex << " " << phdr_offset;
    LOG(INFO) << "phdr_size=" << phdr_size << std::hex << " " << phdr_size;
  }

  // .dynsym
  uint32_t dynsym_alignment = sizeof(llvm::ELF::Elf32_Word);
  uint32_t dynsym_offset = expected_offset = RoundUp(expected_offset, dynsym_alignment);
  const uint8_t SYM_UNDEF       = 0;  // aka STN_UNDEF
  const uint8_t SYM_OATDATA     = 1;
  const uint8_t SYM_OATEXEC     = 2;
  const uint8_t SYM_OATLASTWORD = 3;
  const uint8_t SYM_NUM         = 4;
  uint32_t dynsym_size = sizeof(llvm::ELF::Elf32_Sym) * SYM_NUM;
  expected_offset += dynsym_size;
  if (debug) {
    LOG(INFO) << "dynsym_offset=" << dynsym_offset << std::hex << " " << dynsym_offset;
    LOG(INFO) << "dynsym_size=" << dynsym_size << std::hex << " " << dynsym_size;
  }

  // .dynstr
  uint32_t dynstr_alignment = 1;
  uint32_t dynstr_offset = expected_offset = RoundUp(expected_offset, dynstr_alignment);
  std::string dynstr;
  dynstr += '\0';
  uint32_t dynstr_oatdata_offset = dynstr.size();
  dynstr += "oatdata";
  dynstr += '\0';
  uint32_t dynstr_oatexec_offset = dynstr.size();
  dynstr += "oatexec";
  dynstr += '\0';
  uint32_t dynstr_oatlastword_offset = dynstr.size();
  dynstr += "oatlastword";
  dynstr += '\0';
  uint32_t dynstr_soname_offset = dynstr.size();
  std::string file_name(elf_file_->GetPath());
  size_t directory_separator_pos = file_name.rfind('/');
  if (directory_separator_pos != std::string::npos) {
    file_name = file_name.substr(directory_separator_pos + 1);
  }
  dynstr += file_name;
  dynstr += '\0';
  uint32_t dynstr_size = dynstr.size();
  expected_offset += dynstr_size;
  if (debug) {
    LOG(INFO) << "dynstr_offset=" << dynstr_offset << std::hex << " " << dynstr_offset;
    LOG(INFO) << "dynstr_size=" << dynstr_size << std::hex << " " << dynstr_size;
  }

  // .hash
  uint32_t hash_alignment = sizeof(llvm::ELF::Elf32_Word);  // Even for 64-bit
  uint32_t hash_offset = expected_offset = RoundUp(expected_offset, hash_alignment);
  const uint8_t HASH_NBUCKET = 0;
  const uint8_t HASH_NCHAIN  = 1;
  const uint8_t HASH_BUCKET0 = 2;
  const uint8_t HASH_NUM     = HASH_BUCKET0 + 1 + SYM_NUM;
  uint32_t hash_size = sizeof(llvm::ELF::Elf32_Word) * HASH_NUM;
  expected_offset += hash_size;
  if (debug) {
    LOG(INFO) << "hash_offset=" << hash_offset << std::hex << " " << hash_offset;
    LOG(INFO) << "hash_size=" << hash_size << std::hex << " " << hash_size;
  }

  // .rodata
  uint32_t oat_data_alignment = kPageSize;
  uint32_t oat_data_offset = expected_offset = RoundUp(expected_offset, oat_data_alignment);
  const OatHeader& oat_header = oat_writer.GetOatHeader();
  CHECK(oat_header.IsValid());
  uint32_t oat_data_size = oat_header.GetExecutableOffset();
  expected_offset += oat_data_size;
  if (debug) {
    LOG(INFO) << "oat_data_offset=" << oat_data_offset << std::hex << " " << oat_data_offset;
    LOG(INFO) << "oat_data_size=" << oat_data_size << std::hex << " " << oat_data_size;
  }

  // .text
  uint32_t oat_exec_alignment = kPageSize;
  CHECK_ALIGNED(expected_offset, kPageSize);
  uint32_t oat_exec_offset = expected_offset = RoundUp(expected_offset, oat_exec_alignment);
  uint32_t oat_exec_size = oat_writer.GetSize() - oat_data_size;
  expected_offset += oat_exec_size;
  CHECK_EQ(oat_data_offset + oat_writer.GetSize(), expected_offset);
  if (debug) {
    LOG(INFO) << "oat_exec_offset=" << oat_exec_offset << std::hex << " " << oat_exec_offset;
    LOG(INFO) << "oat_exec_size=" << oat_exec_size << std::hex << " " << oat_exec_size;
  }

  // .dynamic
  // alignment would naturally be sizeof(llvm::ELF::Elf32_Word), but we want this in a new segment
  uint32_t dynamic_alignment = kPageSize;
  uint32_t dynamic_offset = expected_offset = RoundUp(expected_offset, dynamic_alignment);
  const uint8_t DH_SONAME = 0;
  const uint8_t DH_HASH   = 1;
  const uint8_t DH_SYMTAB = 2;
  const uint8_t DH_SYMENT = 3;
  const uint8_t DH_STRTAB = 4;
  const uint8_t DH_STRSZ  = 5;
  const uint8_t DH_NULL   = 6;
  const uint8_t DH_NUM    = 7;
  uint32_t dynamic_size = sizeof(llvm::ELF::Elf32_Dyn) * DH_NUM;
  expected_offset += dynamic_size;
  if (debug) {
    LOG(INFO) << "dynamic_offset=" << dynamic_offset << std::hex << " " << dynamic_offset;
    LOG(INFO) << "dynamic_size=" << dynamic_size << std::hex << " " << dynamic_size;
  }

  // .shstrtab
  uint32_t shstrtab_alignment = 1;
  uint32_t shstrtab_offset = expected_offset = RoundUp(expected_offset, shstrtab_alignment);
  std::string shstrtab;
  shstrtab += '\0';
  uint32_t shstrtab_dynamic_offset = shstrtab.size();
  CHECK_EQ(1U, shstrtab_dynamic_offset);
  shstrtab += ".dynamic";
  shstrtab += '\0';
  uint32_t shstrtab_dynsym_offset = shstrtab.size();
  shstrtab += ".dynsym";
  shstrtab += '\0';
  uint32_t shstrtab_dynstr_offset = shstrtab.size();
  shstrtab += ".dynstr";
  shstrtab += '\0';
  uint32_t shstrtab_hash_offset = shstrtab.size();
  shstrtab += ".hash";
  shstrtab += '\0';
  uint32_t shstrtab_rodata_offset = shstrtab.size();
  shstrtab += ".rodata";
  shstrtab += '\0';
  uint32_t shstrtab_text_offset = shstrtab.size();
  shstrtab += ".text";
  shstrtab += '\0';
  uint32_t shstrtab_shstrtab_offset = shstrtab.size();
  shstrtab += ".shstrtab";
  shstrtab += '\0';
  uint32_t shstrtab_size = shstrtab.size();
  expected_offset += shstrtab_size;
  if (debug) {
    LOG(INFO) << "shstrtab_offset=" << shstrtab_offset << std::hex << " " << shstrtab_offset;
    LOG(INFO) << "shstrtab_size=" << shstrtab_size << std::hex << " " << shstrtab_size;
  }

  // section headers (after all sections)
  uint32_t shdr_alignment = sizeof(llvm::ELF::Elf32_Word);
  uint32_t shdr_offset = expected_offset = RoundUp(expected_offset, shdr_alignment);
  const uint8_t SH_NULL     = 0;
  const uint8_t SH_DYNSYM   = 1;
  const uint8_t SH_DYNSTR   = 2;
  const uint8_t SH_HASH     = 3;
  const uint8_t SH_RODATA   = 4;
  const uint8_t SH_TEXT     = 5;
  const uint8_t SH_DYNAMIC  = 6;
  const uint8_t SH_SHSTRTAB = 7;
  const uint8_t SH_NUM      = 8;
  uint32_t shdr_size = sizeof(llvm::ELF::Elf32_Shdr) * SH_NUM;
  expected_offset += shdr_size;
  if (debug) {
    LOG(INFO) << "shdr_offset=" << shdr_offset << std::hex << " " << shdr_offset;
    LOG(INFO) << "shdr_size=" << shdr_size << std::hex << " " << shdr_size;
  }

  // phase 2: initializing data

  // Elf32_Ehdr
  llvm::ELF::Elf32_Ehdr elf_header;
  memset(&elf_header, 0, sizeof(elf_header));
  elf_header.e_ident[llvm::ELF::EI_MAG0]       = llvm::ELF::ElfMagic[0];
  elf_header.e_ident[llvm::ELF::EI_MAG1]       = llvm::ELF::ElfMagic[1];
  elf_header.e_ident[llvm::ELF::EI_MAG2]       = llvm::ELF::ElfMagic[2];
  elf_header.e_ident[llvm::ELF::EI_MAG3]       = llvm::ELF::ElfMagic[3];
  elf_header.e_ident[llvm::ELF::EI_CLASS]      = llvm::ELF::ELFCLASS32;
  elf_header.e_ident[llvm::ELF::EI_DATA]       = llvm::ELF::ELFDATA2LSB;
  elf_header.e_ident[llvm::ELF::EI_VERSION]    = llvm::ELF::EV_CURRENT;
  elf_header.e_ident[llvm::ELF::EI_OSABI]      = llvm::ELF::ELFOSABI_LINUX;
  elf_header.e_ident[llvm::ELF::EI_ABIVERSION] = 0;
  elf_header.e_type = llvm::ELF::ET_DYN;
  switch (compiler_driver_->GetInstructionSet()) {
    case kThumb2: {
      elf_header.e_machine = llvm::ELF::EM_ARM;
      elf_header.e_flags = llvm::ELF::EF_ARM_EABI_VER5;
      break;
    }
    case kX86: {
      elf_header.e_machine = llvm::ELF::EM_386;
      elf_header.e_flags = 0;
      break;
    }
    case kMips: {
      elf_header.e_machine = llvm::ELF::EM_MIPS;
      elf_header.e_flags = (llvm::ELF::EF_MIPS_NOREORDER |
                            llvm::ELF::EF_MIPS_PIC       |
                            llvm::ELF::EF_MIPS_CPIC      |
                            llvm::ELF::EF_MIPS_ABI_O32   |
                            llvm::ELF::EF_MIPS_ARCH_32R2);
      break;
    }
    case kArm:
    default: {
      LOG(FATAL) << "Unknown instruction set: " << compiler_driver_->GetInstructionSet();
      break;
    }
  }
  elf_header.e_version = 1;
  elf_header.e_entry = 0;
  elf_header.e_phoff = phdr_offset;
  elf_header.e_shoff = shdr_offset;
  elf_header.e_ehsize = sizeof(llvm::ELF::Elf32_Ehdr);
  elf_header.e_phentsize = sizeof(llvm::ELF::Elf32_Phdr);
  elf_header.e_phnum = PH_NUM;
  elf_header.e_shentsize = sizeof(llvm::ELF::Elf32_Shdr);
  elf_header.e_shnum = SH_NUM;
  elf_header.e_shstrndx = SH_SHSTRTAB;

  // PHDR
  llvm::ELF::Elf32_Phdr program_headers[PH_NUM];
  memset(&program_headers, 0, sizeof(program_headers));

  program_headers[PH_PHDR].p_type    = llvm::ELF::PT_PHDR;
  program_headers[PH_PHDR].p_offset  = phdr_offset;
  program_headers[PH_PHDR].p_vaddr   = phdr_offset;
  program_headers[PH_PHDR].p_paddr   = phdr_offset;
  program_headers[PH_PHDR].p_filesz  = sizeof(program_headers);
  program_headers[PH_PHDR].p_memsz   = sizeof(program_headers);
  program_headers[PH_PHDR].p_flags   = llvm::ELF::PF_R;
  program_headers[PH_PHDR].p_align   = phdr_alignment;

  program_headers[PH_LOAD_R__].p_type    = llvm::ELF::PT_LOAD;
  program_headers[PH_LOAD_R__].p_offset  = 0;
  program_headers[PH_LOAD_R__].p_vaddr   = 0;
  program_headers[PH_LOAD_R__].p_paddr   = 0;
  program_headers[PH_LOAD_R__].p_filesz  = oat_data_offset + oat_data_size;
  program_headers[PH_LOAD_R__].p_memsz   = oat_data_offset + oat_data_size;
  program_headers[PH_LOAD_R__].p_flags   = llvm::ELF::PF_R;
  program_headers[PH_LOAD_R__].p_align   = oat_data_alignment;

  program_headers[PH_LOAD_R_X].p_type    = llvm::ELF::PT_LOAD;
  program_headers[PH_LOAD_R_X].p_offset  = oat_exec_offset;
  program_headers[PH_LOAD_R_X].p_vaddr   = oat_exec_offset;
  program_headers[PH_LOAD_R_X].p_paddr   = oat_exec_offset;
  program_headers[PH_LOAD_R_X].p_filesz  = oat_exec_size;
  program_headers[PH_LOAD_R_X].p_memsz   = oat_exec_size;
  program_headers[PH_LOAD_R_X].p_flags   = llvm::ELF::PF_R | llvm::ELF::PF_X;
  program_headers[PH_LOAD_R_X].p_align   = oat_exec_alignment;

  // TODO: PF_W for DYNAMIC is considered processor specific, do we need it?
  program_headers[PH_LOAD_RW_].p_type    = llvm::ELF::PT_LOAD;
  program_headers[PH_LOAD_RW_].p_offset  = dynamic_offset;
  program_headers[PH_LOAD_RW_].p_vaddr   = dynamic_offset;
  program_headers[PH_LOAD_RW_].p_paddr   = dynamic_offset;
  program_headers[PH_LOAD_RW_].p_filesz  = dynamic_size;
  program_headers[PH_LOAD_RW_].p_memsz   = dynamic_size;
  program_headers[PH_LOAD_RW_].p_flags   = llvm::ELF::PF_R | llvm::ELF::PF_W;
  program_headers[PH_LOAD_RW_].p_align   = dynamic_alignment;

  // TODO: PF_W for DYNAMIC is considered processor specific, do we need it?
  program_headers[PH_DYNAMIC].p_type    = llvm::ELF::PT_DYNAMIC;
  program_headers[PH_DYNAMIC].p_offset  = dynamic_offset;
  program_headers[PH_DYNAMIC].p_vaddr   = dynamic_offset;
  program_headers[PH_DYNAMIC].p_paddr   = dynamic_offset;
  program_headers[PH_DYNAMIC].p_filesz  = dynamic_size;
  program_headers[PH_DYNAMIC].p_memsz   = dynamic_size;
  program_headers[PH_DYNAMIC].p_flags   = llvm::ELF::PF_R | llvm::ELF::PF_W;
  program_headers[PH_DYNAMIC].p_align   = dynamic_alignment;

  // .dynsym
  llvm::ELF::Elf32_Sym dynsym[SYM_NUM];
  memset(&dynsym, 0, sizeof(dynsym));

  dynsym[SYM_UNDEF].st_name  = 0;
  dynsym[SYM_UNDEF].st_value = 0;
  dynsym[SYM_UNDEF].st_size  = 0;
  dynsym[SYM_UNDEF].st_info  = 0;
  dynsym[SYM_UNDEF].st_other = 0;
  dynsym[SYM_UNDEF].st_shndx = 0;

  dynsym[SYM_OATDATA].st_name  = dynstr_oatdata_offset;
  dynsym[SYM_OATDATA].st_value = oat_data_offset;
  dynsym[SYM_OATDATA].st_size  = oat_data_size;
  dynsym[SYM_OATDATA].setBindingAndType(llvm::ELF::STB_GLOBAL, llvm::ELF::STT_OBJECT);
  dynsym[SYM_OATDATA].st_other = llvm::ELF::STV_DEFAULT;
  dynsym[SYM_OATDATA].st_shndx = SH_RODATA;

  dynsym[SYM_OATEXEC].st_name  = dynstr_oatexec_offset;
  dynsym[SYM_OATEXEC].st_value = oat_exec_offset;
  dynsym[SYM_OATEXEC].st_size  = oat_exec_size;
  dynsym[SYM_OATEXEC].setBindingAndType(llvm::ELF::STB_GLOBAL, llvm::ELF::STT_OBJECT);
  dynsym[SYM_OATEXEC].st_other = llvm::ELF::STV_DEFAULT;
  dynsym[SYM_OATEXEC].st_shndx = SH_TEXT;

  dynsym[SYM_OATLASTWORD].st_name  = dynstr_oatlastword_offset;
  dynsym[SYM_OATLASTWORD].st_value = oat_exec_offset + oat_exec_size - 4;
  dynsym[SYM_OATLASTWORD].st_size  = 4;
  dynsym[SYM_OATLASTWORD].setBindingAndType(llvm::ELF::STB_GLOBAL, llvm::ELF::STT_OBJECT);
  dynsym[SYM_OATLASTWORD].st_other = llvm::ELF::STV_DEFAULT;
  dynsym[SYM_OATLASTWORD].st_shndx = SH_TEXT;

  // .dynstr initialized above as dynstr

  // .hash
  llvm::ELF::Elf32_Word hash[HASH_NUM];  // Note this is Elf32_Word even on 64-bit
  hash[HASH_NBUCKET] = 1;
  hash[HASH_NCHAIN]  = SYM_NUM;
  hash[HASH_BUCKET0] = SYM_OATDATA;
  hash[HASH_BUCKET0 + 1 + SYM_UNDEF]       = SYM_UNDEF;
  hash[HASH_BUCKET0 + 1 + SYM_OATDATA]     = SYM_OATEXEC;
  hash[HASH_BUCKET0 + 1 + SYM_OATEXEC]     = SYM_OATLASTWORD;
  hash[HASH_BUCKET0 + 1 + SYM_OATLASTWORD] = SYM_UNDEF;

  // .rodata and .text content come from oat_contents

  // .dynamic
  llvm::ELF::Elf32_Dyn dynamic_headers[DH_NUM];
  memset(&dynamic_headers, 0, sizeof(dynamic_headers));

  dynamic_headers[DH_SONAME].d_tag = llvm::ELF::DT_SONAME;
  dynamic_headers[DH_SONAME].d_un.d_val = dynstr_soname_offset;

  dynamic_headers[DH_HASH].d_tag = llvm::ELF::DT_HASH;
  dynamic_headers[DH_HASH].d_un.d_ptr = hash_offset;

  dynamic_headers[DH_SYMTAB].d_tag = llvm::ELF::DT_SYMTAB;
  dynamic_headers[DH_SYMTAB].d_un.d_ptr = dynsym_offset;

  dynamic_headers[DH_SYMENT].d_tag = llvm::ELF::DT_SYMENT;
  dynamic_headers[DH_SYMENT].d_un.d_val = sizeof(llvm::ELF::Elf32_Sym);

  dynamic_headers[DH_STRTAB].d_tag = llvm::ELF::DT_STRTAB;
  dynamic_headers[DH_STRTAB].d_un.d_ptr = dynstr_offset;

  dynamic_headers[DH_STRSZ].d_tag = llvm::ELF::DT_STRSZ;
  dynamic_headers[DH_STRSZ].d_un.d_val = dynstr_size;

  dynamic_headers[DH_NULL].d_tag = llvm::ELF::DT_NULL;
  dynamic_headers[DH_NULL].d_un.d_val = 0;

  // .shstrtab initialized above as shstrtab

  // section headers (after all sections)
  llvm::ELF::Elf32_Shdr section_headers[SH_NUM];
  memset(&section_headers, 0, sizeof(section_headers));

  section_headers[SH_NULL].sh_name      = 0;
  section_headers[SH_NULL].sh_type      = llvm::ELF::SHT_NULL;
  section_headers[SH_NULL].sh_flags     = 0;
  section_headers[SH_NULL].sh_addr      = 0;
  section_headers[SH_NULL].sh_offset    = 0;
  section_headers[SH_NULL].sh_size      = 0;
  section_headers[SH_NULL].sh_link      = 0;
  section_headers[SH_NULL].sh_info      = 0;
  section_headers[SH_NULL].sh_addralign = 0;
  section_headers[SH_NULL].sh_entsize   = 0;

  section_headers[SH_DYNSYM].sh_name      = shstrtab_dynsym_offset;
  section_headers[SH_DYNSYM].sh_type      = llvm::ELF::SHT_DYNSYM;
  section_headers[SH_DYNSYM].sh_flags     = llvm::ELF::SHF_ALLOC;
  section_headers[SH_DYNSYM].sh_addr      = dynsym_offset;
  section_headers[SH_DYNSYM].sh_offset    = dynsym_offset;
  section_headers[SH_DYNSYM].sh_size      = dynsym_size;
  section_headers[SH_DYNSYM].sh_link      = SH_DYNSTR;
  section_headers[SH_DYNSYM].sh_info      = 1;  // 1 because we have not STB_LOCAL symbols
  section_headers[SH_DYNSYM].sh_addralign = dynsym_alignment;
  section_headers[SH_DYNSYM].sh_entsize   = sizeof(llvm::ELF::Elf32_Sym);

  section_headers[SH_DYNSTR].sh_name      = shstrtab_dynstr_offset;
  section_headers[SH_DYNSTR].sh_type      = llvm::ELF::SHT_STRTAB;
  section_headers[SH_DYNSTR].sh_flags     = llvm::ELF::SHF_ALLOC;
  section_headers[SH_DYNSTR].sh_addr      = dynstr_offset;
  section_headers[SH_DYNSTR].sh_offset    = dynstr_offset;
  section_headers[SH_DYNSTR].sh_size      = dynstr_size;
  section_headers[SH_DYNSTR].sh_link      = 0;
  section_headers[SH_DYNSTR].sh_info      = 0;
  section_headers[SH_DYNSTR].sh_addralign = dynstr_alignment;
  section_headers[SH_DYNSTR].sh_entsize   = 0;

  section_headers[SH_HASH].sh_name      = shstrtab_hash_offset;
  section_headers[SH_HASH].sh_type      = llvm::ELF::SHT_HASH;
  section_headers[SH_HASH].sh_flags     = llvm::ELF::SHF_ALLOC;
  section_headers[SH_HASH].sh_addr      = hash_offset;
  section_headers[SH_HASH].sh_offset    = hash_offset;
  section_headers[SH_HASH].sh_size      = hash_size;
  section_headers[SH_HASH].sh_link      = SH_DYNSYM;
  section_headers[SH_HASH].sh_info      = 0;
  section_headers[SH_HASH].sh_addralign = hash_alignment;
  section_headers[SH_HASH].sh_entsize   = sizeof(llvm::ELF::Elf32_Word);  // This is Elf32_Word even on 64-bit

  section_headers[SH_RODATA].sh_name      = shstrtab_rodata_offset;
  section_headers[SH_RODATA].sh_type      = llvm::ELF::SHT_PROGBITS;
  section_headers[SH_RODATA].sh_flags     = llvm::ELF::SHF_ALLOC;
  section_headers[SH_RODATA].sh_addr      = oat_data_offset;
  section_headers[SH_RODATA].sh_offset    = oat_data_offset;
  section_headers[SH_RODATA].sh_size      = oat_data_size;
  section_headers[SH_RODATA].sh_link      = 0;
  section_headers[SH_RODATA].sh_info      = 0;
  section_headers[SH_RODATA].sh_addralign = oat_data_alignment;
  section_headers[SH_RODATA].sh_entsize   = 0;

  section_headers[SH_TEXT].sh_name      = shstrtab_text_offset;
  section_headers[SH_TEXT].sh_type      = llvm::ELF::SHT_PROGBITS;
  section_headers[SH_TEXT].sh_flags     = llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR;
  section_headers[SH_TEXT].sh_addr      = oat_exec_offset;
  section_headers[SH_TEXT].sh_offset    = oat_exec_offset;
  section_headers[SH_TEXT].sh_size      = oat_exec_size;
  section_headers[SH_TEXT].sh_link      = 0;
  section_headers[SH_TEXT].sh_info      = 0;
  section_headers[SH_TEXT].sh_addralign = oat_exec_alignment;
  section_headers[SH_TEXT].sh_entsize   = 0;

  // TODO: SHF_WRITE for .dynamic is considered processor specific, do we need it?
  section_headers[SH_DYNAMIC].sh_name      = shstrtab_dynamic_offset;
  section_headers[SH_DYNAMIC].sh_type      = llvm::ELF::SHT_DYNAMIC;
  section_headers[SH_DYNAMIC].sh_flags     = llvm::ELF::SHF_WRITE | llvm::ELF::SHF_ALLOC;
  section_headers[SH_DYNAMIC].sh_addr      = dynamic_offset;
  section_headers[SH_DYNAMIC].sh_offset    = dynamic_offset;
  section_headers[SH_DYNAMIC].sh_size      = dynamic_size;
  section_headers[SH_DYNAMIC].sh_link      = SH_DYNSTR;
  section_headers[SH_DYNAMIC].sh_info      = 0;
  section_headers[SH_DYNAMIC].sh_addralign = dynamic_alignment;
  section_headers[SH_DYNAMIC].sh_entsize   = sizeof(llvm::ELF::Elf32_Dyn);

  section_headers[SH_SHSTRTAB].sh_name      = shstrtab_shstrtab_offset;
  section_headers[SH_SHSTRTAB].sh_type      = llvm::ELF::SHT_STRTAB;
  section_headers[SH_SHSTRTAB].sh_flags     = 0;
  section_headers[SH_SHSTRTAB].sh_addr      = shstrtab_offset;
  section_headers[SH_SHSTRTAB].sh_offset    = shstrtab_offset;
  section_headers[SH_SHSTRTAB].sh_size      = shstrtab_size;
  section_headers[SH_SHSTRTAB].sh_link      = 0;
  section_headers[SH_SHSTRTAB].sh_info      = 0;
  section_headers[SH_SHSTRTAB].sh_addralign = shstrtab_alignment;
  section_headers[SH_SHSTRTAB].sh_entsize   = 0;

  // phase 3: writing file

  // Elf32_Ehdr
  if (!elf_file_->WriteFully(&elf_header, sizeof(elf_header))) {
    PLOG(ERROR) << "Failed to write ELF header for " << elf_file_->GetPath();
    return false;
  }

  // PHDR
  if (static_cast<off_t>(phdr_offset) != lseek(elf_file_->Fd(), 0, SEEK_CUR)) {
    PLOG(ERROR) << "Failed to be at expected ELF program header offset phdr_offset "
                << " for " << elf_file_->GetPath();
    return false;
  }
  if (!elf_file_->WriteFully(program_headers, sizeof(program_headers))) {
    PLOG(ERROR) << "Failed to write ELF program headers for " << elf_file_->GetPath();
    return false;
  }

  // .dynsym
  DCHECK_LE(phdr_offset + phdr_size, dynsym_offset);
  if (static_cast<off_t>(dynsym_offset) != lseek(elf_file_->Fd(), dynsym_offset, SEEK_SET)) {
    PLOG(ERROR) << "Failed to seek to .dynsym offset location " << dynsym_offset
                << " for " << elf_file_->GetPath();
    return false;
  }
  if (!elf_file_->WriteFully(dynsym, sizeof(dynsym))) {
    PLOG(ERROR) << "Failed to write .dynsym for " << elf_file_->GetPath();
    return false;
  }

  // .dynstr
  DCHECK_LE(dynsym_offset + dynsym_size, dynstr_offset);
  if (static_cast<off_t>(dynstr_offset) != lseek(elf_file_->Fd(), dynstr_offset, SEEK_SET)) {
    PLOG(ERROR) << "Failed to seek to .dynstr offset " << dynstr_offset
                << " for " << elf_file_->GetPath();
    return false;
  }
  if (!elf_file_->WriteFully(&dynstr[0], dynstr_size)) {
    PLOG(ERROR) << "Failed to write .dynsym for " << elf_file_->GetPath();
    return false;
  }

  // .hash
  DCHECK_LE(dynstr_offset + dynstr_size, hash_offset);
  if (static_cast<off_t>(hash_offset) != lseek(elf_file_->Fd(), hash_offset, SEEK_SET)) {
    PLOG(ERROR) << "Failed to seek to .hash offset " << hash_offset
                << " for " << elf_file_->GetPath();
    return false;
  }
  if (!elf_file_->WriteFully(hash, sizeof(hash))) {
    PLOG(ERROR) << "Failed to write .dynsym for " << elf_file_->GetPath();
    return false;
  }

  // .rodata .text
  DCHECK_LE(hash_offset + hash_size, oat_data_offset);
  if (static_cast<off_t>(oat_data_offset) != lseek(elf_file_->Fd(), oat_data_offset, SEEK_SET)) {
    PLOG(ERROR) << "Failed to seek to .rodata offset " << oat_data_offset
                << " for " << elf_file_->GetPath();
    return false;
  }
  BufferedOutputStream output_stream(new FileOutputStream(elf_file_));
  if (!oat_writer.Write(output_stream)) {
    PLOG(ERROR) << "Failed to write .rodata and .text for " << elf_file_->GetPath();
    return false;
  }

  // .dynamic
  DCHECK_LE(oat_data_offset + oat_writer.GetSize(), dynamic_offset);
  if (static_cast<off_t>(dynamic_offset) != lseek(elf_file_->Fd(), dynamic_offset, SEEK_SET)) {
    PLOG(ERROR) << "Failed to seek to .dynamic offset " << dynamic_offset
                << " for " << elf_file_->GetPath();
    return false;
  }
  if (!elf_file_->WriteFully(&dynamic_headers[0], dynamic_size)) {
    PLOG(ERROR) << "Failed to write .dynamic for " << elf_file_->GetPath();
    return false;
  }

  // .shstrtab
  DCHECK_LE(dynamic_offset + dynamic_size, shstrtab_offset);
  if (static_cast<off_t>(shstrtab_offset) != lseek(elf_file_->Fd(), shstrtab_offset, SEEK_SET)) {
    PLOG(ERROR) << "Failed to seek to .shstrtab offset " << shstrtab_offset
                << " for " << elf_file_->GetPath();
    return false;
  }
  if (!elf_file_->WriteFully(&shstrtab[0], shstrtab_size)) {
    PLOG(ERROR) << "Failed to write .shstrtab for " << elf_file_->GetPath();
    return false;
  }

  // section headers (after all sections)
  DCHECK_LE(shstrtab_offset + shstrtab_size, shdr_offset);
  if (static_cast<off_t>(shdr_offset) != lseek(elf_file_->Fd(), shdr_offset, SEEK_SET)) {
    PLOG(ERROR) << "Failed to seek to ELF section headers offset " << shdr_offset
                << " for " << elf_file_->GetPath();
    return false;
  }
  if (!elf_file_->WriteFully(section_headers, sizeof(section_headers))) {
    PLOG(ERROR) << "Failed to write ELF section headers for " << elf_file_->GetPath();
    return false;
  }

  VLOG(compiler) << "ELF file written successfully: " << elf_file_->GetPath();
  return true;
}

}  // namespace art
