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

#include "elf_fixup.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "elf_file.h"
#include "elf_writer.h"
#include "UniquePtr.h"

namespace art {

static const bool DEBUG_FIXUP = false;

bool ElfFixup::Fixup(File* file, uintptr_t oat_data_begin) {
  UniquePtr<ElfFile> elf_file(ElfFile::Open(file, true, false));
  CHECK(elf_file.get() != NULL);

  // Lookup "oatdata" symbol address.
  ::llvm::ELF::Elf32_Addr oatdata_address = ElfWriter::GetOatDataAddress(elf_file.get());
  ::llvm::ELF::Elf32_Off base_address = oat_data_begin - oatdata_address;

  if (!FixupDynamic(*elf_file.get(), base_address)) {
      LOG(WARNING) << "Failed fo fixup .dynamic in " << file->GetPath();
      return false;
  }
  if (!FixupSectionHeaders(*elf_file.get(), base_address)) {
      LOG(WARNING) << "Failed fo fixup section headers in " << file->GetPath();
      return false;
  }
  if (!FixupProgramHeaders(*elf_file.get(), base_address)) {
      LOG(WARNING) << "Failed fo fixup program headers in " << file->GetPath();
      return false;
  }
  if (!FixupSymbols(*elf_file.get(), base_address, true)) {
      LOG(WARNING) << "Failed fo fixup .dynsym in " << file->GetPath();
      return false;
  }
  if (!FixupSymbols(*elf_file.get(), base_address, false)) {
      LOG(WARNING) << "Failed fo fixup .symtab in " << file->GetPath();
      return false;
  }
  if (!FixupRelocations(*elf_file.get(), base_address)) {
      LOG(WARNING) << "Failed fo fixup .rel.dyn in " << file->GetPath();
      return false;
  }
  return true;
}

// MIPS seems to break the rules d_val vs d_ptr even though their values are between DT_LOPROC and DT_HIPROC
#define DT_MIPS_RLD_VERSION  0x70000001  // d_val
#define DT_MIPS_TIME_STAMP   0x70000002  // d_val
#define DT_MIPS_ICHECKSUM    0x70000003  // d_val
#define DT_MIPS_IVERSION     0x70000004  // d_val
#define DT_MIPS_FLAGS        0x70000005  // d_val
#define DT_MIPS_BASE_ADDRESS 0x70000006  // d_ptr
#define DT_MIPS_CONFLICT     0x70000008  // d_ptr
#define DT_MIPS_LIBLIST      0x70000009  // d_ptr
#define DT_MIPS_LOCAL_GOTNO  0x7000000A  // d_val
#define DT_MIPS_CONFLICTNO   0x7000000B  // d_val
#define DT_MIPS_LIBLISTNO    0x70000010  // d_val
#define DT_MIPS_SYMTABNO     0x70000011  // d_val
#define DT_MIPS_UNREFEXTNO   0x70000012  // d_val
#define DT_MIPS_GOTSYM       0x70000013  // d_val
#define DT_MIPS_HIPAGENO     0x70000014  // d_val
#define DT_MIPS_RLD_MAP      0x70000016  // d_ptr

bool ElfFixup::FixupDynamic(ElfFile& elf_file, uintptr_t base_address) {
  for (::llvm::ELF::Elf32_Word i = 0; i < elf_file.GetDynamicNum(); i++) {
    ::llvm::ELF::Elf32_Dyn& elf_dyn = elf_file.GetDynamic(i);
    ::llvm::ELF::Elf32_Word d_tag = elf_dyn.d_tag;
    bool elf_dyn_needs_fixup = false;
    switch (d_tag) {
      // case 1: well known d_tag values that imply Elf32_Dyn.d_un contains an address in d_ptr
      case ::llvm::ELF::DT_PLTGOT:
      case ::llvm::ELF::DT_HASH:
      case ::llvm::ELF::DT_STRTAB:
      case ::llvm::ELF::DT_SYMTAB:
      case ::llvm::ELF::DT_RELA:
      case ::llvm::ELF::DT_INIT:
      case ::llvm::ELF::DT_FINI:
      case ::llvm::ELF::DT_REL:
      case ::llvm::ELF::DT_DEBUG:
      case ::llvm::ELF::DT_JMPREL: {
        elf_dyn_needs_fixup = true;
        break;
      }
      // d_val or ignored values
      case ::llvm::ELF::DT_NULL:
      case ::llvm::ELF::DT_NEEDED:
      case ::llvm::ELF::DT_PLTRELSZ:
      case ::llvm::ELF::DT_RELASZ:
      case ::llvm::ELF::DT_RELAENT:
      case ::llvm::ELF::DT_STRSZ:
      case ::llvm::ELF::DT_SYMENT:
      case ::llvm::ELF::DT_SONAME:
      case ::llvm::ELF::DT_RPATH:
      case ::llvm::ELF::DT_SYMBOLIC:
      case ::llvm::ELF::DT_RELSZ:
      case ::llvm::ELF::DT_RELENT:
      case ::llvm::ELF::DT_PLTREL:
      case ::llvm::ELF::DT_TEXTREL:
      case ::llvm::ELF::DT_BIND_NOW:
      case ::llvm::ELF::DT_INIT_ARRAYSZ:
      case ::llvm::ELF::DT_FINI_ARRAYSZ:
      case ::llvm::ELF::DT_RUNPATH:
      case ::llvm::ELF::DT_FLAGS: {
        break;
      }
      // boundary values that should not be used
      case ::llvm::ELF::DT_ENCODING:
      case ::llvm::ELF::DT_LOOS:
      case ::llvm::ELF::DT_HIOS:
      case ::llvm::ELF::DT_LOPROC:
      case ::llvm::ELF::DT_HIPROC: {
        LOG(FATAL) << "Illegal d_tag value 0x" << std::hex << d_tag;
        break;
      }
      default: {
        // case 2: "regular" DT_* ranges where even d_tag values imply an address in d_ptr
        if ((::llvm::ELF::DT_ENCODING  < d_tag && d_tag < ::llvm::ELF::DT_LOOS)
            || (::llvm::ELF::DT_LOOS   < d_tag && d_tag < ::llvm::ELF::DT_HIOS)
            || (::llvm::ELF::DT_LOPROC < d_tag && d_tag < ::llvm::ELF::DT_HIPROC)) {
          // Special case for MIPS which breaks the regular rules between DT_LOPROC and DT_HIPROC
          if (elf_file.GetHeader().e_machine == ::llvm::ELF::EM_MIPS) {
            switch (d_tag) {
              case DT_MIPS_RLD_VERSION:
              case DT_MIPS_TIME_STAMP:
              case DT_MIPS_ICHECKSUM:
              case DT_MIPS_IVERSION:
              case DT_MIPS_FLAGS:
              case DT_MIPS_LOCAL_GOTNO:
              case DT_MIPS_CONFLICTNO:
              case DT_MIPS_LIBLISTNO:
              case DT_MIPS_SYMTABNO:
              case DT_MIPS_UNREFEXTNO:
              case DT_MIPS_GOTSYM:
              case DT_MIPS_HIPAGENO: {
                break;
              }
              case DT_MIPS_BASE_ADDRESS:
              case DT_MIPS_CONFLICT:
              case DT_MIPS_LIBLIST:
              case DT_MIPS_RLD_MAP: {
                elf_dyn_needs_fixup = true;
                break;
              }
              default: {
                LOG(FATAL) << "Unknown MIPS d_tag value 0x" << std::hex << d_tag;
                break;
              }
            }
          } else if ((elf_dyn.d_tag % 2) == 0) {
            elf_dyn_needs_fixup = true;
          }
        } else {
          LOG(FATAL) << "Unknown d_tag value 0x" << std::hex << d_tag;
        }
        break;
      }
    }
    if (elf_dyn_needs_fixup) {
      uint32_t d_ptr = elf_dyn.d_un.d_ptr;
      if (DEBUG_FIXUP) {
        LOG(INFO) << StringPrintf("In %s moving Elf32_Dyn[%d] from 0x%08x to 0x%08x",
                                  elf_file.GetFile().GetPath().c_str(), i,
                                  d_ptr, d_ptr + base_address);
      }
      d_ptr += base_address;
      elf_dyn.d_un.d_ptr = d_ptr;
    }
  }
  return true;
}

bool ElfFixup::FixupSectionHeaders(ElfFile& elf_file, uintptr_t base_address) {
  for (::llvm::ELF::Elf32_Word i = 0; i < elf_file.GetSectionHeaderNum(); i++) {
    ::llvm::ELF::Elf32_Shdr& sh = elf_file.GetSectionHeader(i);
    // 0 implies that the section will not exist in the memory of the process
    if (sh.sh_addr == 0) {
      continue;
    }
    if (DEBUG_FIXUP) {
      LOG(INFO) << StringPrintf("In %s moving Elf32_Shdr[%d] from 0x%08x to 0x%08x",
                                elf_file.GetFile().GetPath().c_str(), i,
                                sh.sh_addr, sh.sh_addr + base_address);
    }
    sh.sh_addr += base_address;
  }
  return true;
}

bool ElfFixup::FixupProgramHeaders(ElfFile& elf_file, uintptr_t base_address) {
  // TODO: ELFObjectFile doesn't have give to Elf32_Phdr, so we do that ourselves for now.
  for (::llvm::ELF::Elf32_Word i = 0; i < elf_file.GetProgramHeaderNum(); i++) {
    ::llvm::ELF::Elf32_Phdr& ph = elf_file.GetProgramHeader(i);
    CHECK_EQ(ph.p_vaddr, ph.p_paddr) << elf_file.GetFile().GetPath() << " i=" << i;
    CHECK((ph.p_align == 0) || (0 == ((ph.p_vaddr - ph.p_offset) & (ph.p_align - 1))))
            << elf_file.GetFile().GetPath() << " i=" << i;
    if (DEBUG_FIXUP) {
      LOG(INFO) << StringPrintf("In %s moving Elf32_Phdr[%d] from 0x%08x to 0x%08x",
                                elf_file.GetFile().GetPath().c_str(), i,
                                ph.p_vaddr, ph.p_vaddr + base_address);
    }
    ph.p_vaddr += base_address;
    ph.p_paddr += base_address;
    CHECK((ph.p_align == 0) || (0 == ((ph.p_vaddr - ph.p_offset) & (ph.p_align - 1))))
            << elf_file.GetFile().GetPath() << " i=" << i;
  }
  return true;
}

bool ElfFixup::FixupSymbols(ElfFile& elf_file, uintptr_t base_address, bool dynamic) {
  ::llvm::ELF::Elf32_Word section_type = dynamic ? ::llvm::ELF::SHT_DYNSYM : ::llvm::ELF::SHT_SYMTAB;
  // TODO: Unfortunate ELFObjectFile has protected symbol access, so use ElfFile
  ::llvm::ELF::Elf32_Shdr* symbol_section = elf_file.FindSectionByType(section_type);
  if (symbol_section == NULL) {
    // file is missing optional .symtab
    CHECK(!dynamic) << elf_file.GetFile().GetPath();
    return true;
  }
  for (uint32_t i = 0; i < elf_file.GetSymbolNum(*symbol_section); i++) {
    ::llvm::ELF::Elf32_Sym& symbol = elf_file.GetSymbol(section_type, i);
    if (symbol.st_value != 0) {
      if (DEBUG_FIXUP) {
        LOG(INFO) << StringPrintf("In %s moving Elf32_Sym[%d] from 0x%08x to 0x%08x",
                                  elf_file.GetFile().GetPath().c_str(), i,
                                  symbol.st_value, symbol.st_value + base_address);
      }
      symbol.st_value += base_address;
    }
  }
  return true;
}

bool ElfFixup::FixupRelocations(ElfFile& elf_file, uintptr_t base_address) {
  for (llvm::ELF::Elf32_Word i = 0; i < elf_file.GetSectionHeaderNum(); i++) {
    llvm::ELF::Elf32_Shdr& sh = elf_file.GetSectionHeader(i);
    if (sh.sh_type == llvm::ELF::SHT_REL) {
      for (uint32_t i = 0; i < elf_file.GetRelNum(sh); i++) {
        llvm::ELF::Elf32_Rel& rel = elf_file.GetRel(sh, i);
        if (DEBUG_FIXUP) {
          LOG(INFO) << StringPrintf("In %s moving Elf32_Rel[%d] from 0x%08x to 0x%08x",
                                    elf_file.GetFile().GetPath().c_str(), i,
                                    rel.r_offset, rel.r_offset + base_address);
        }
        rel.r_offset += base_address;
      }
    } else if (sh.sh_type == llvm::ELF::SHT_RELA) {
      for (uint32_t i = 0; i < elf_file.GetRelaNum(sh); i++) {
        llvm::ELF::Elf32_Rela& rela = elf_file.GetRela(sh, i);
        if (DEBUG_FIXUP) {
          LOG(INFO) << StringPrintf("In %s moving Elf32_Rela[%d] from 0x%08x to 0x%08x",
                                    elf_file.GetFile().GetPath().c_str(), i,
                                    rela.r_offset, rela.r_offset + base_address);
        }
        rela.r_offset += base_address;
      }
    }
  }
  return true;
}

}  // namespace art
