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

#include "oat_file.h"

#include <dlfcn.h>

#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "elf_file.h"
#include "oat.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "os.h"
#include "utils.h"
#include "vmap_table.h"

namespace art {

std::string OatFile::DexFilenameToOdexFilename(const std::string& location) {
  CHECK_GE(location.size(), 4U) << location;  // must be at least .123
  size_t dot_index = location.size() - 3 - 1;  // 3=dex or zip or apk
  CHECK_EQ('.', location[dot_index]) << location;
  std::string odex_location(location);
  odex_location.resize(dot_index + 1);
  CHECK_EQ('.', odex_location[odex_location.size()-1]) << location << " " << odex_location;
  odex_location += "odex";
  return odex_location;
}

void OatFile::CheckLocation(const std::string& location) {
  CHECK(!location.empty());
}

OatFile* OatFile::OpenMemory(std::vector<uint8_t>& oat_contents,
                             const std::string& location) {
  CHECK(!oat_contents.empty()) << location;
  CheckLocation(location);
  UniquePtr<OatFile> oat_file(new OatFile(location));
  oat_file->begin_ = &oat_contents[0];
  oat_file->end_ = &oat_contents[oat_contents.size()];
  return oat_file->Setup() ? oat_file.release() : NULL;
}

OatFile* OatFile::Open(const std::string& filename,
                       const std::string& location,
                       byte* requested_base,
                       bool executable) {
  CHECK(!filename.empty()) << location;
  CheckLocation(filename);
#ifdef ART_USE_PORTABLE_COMPILER
  // If we are using PORTABLE, use dlopen to deal with relocations.
  //
  // We use our own ELF loader for Quick to deal with legacy apps that
  // open a generated dex file by name, remove the file, then open
  // another generated dex file with the same name. http://b/10614658
  if (executable) {
    return OpenDlopen(filename, location, requested_base);
  }
#endif
  // If we aren't trying to execute, we just use our own ElfFile loader for a couple reasons:
  //
  // On target, dlopen may fail when compiling due to selinux restrictions on installd.
  //
  // On host, dlopen is expected to fail when cross compiling, so fall back to OpenElfFile.
  // This won't work for portable runtime execution because it doesn't process relocations.
  UniquePtr<File> file(OS::OpenFileForReading(filename.c_str()));
  if (file.get() == NULL) {
    return NULL;
  }
  return OpenElfFile(file.get(), location, requested_base, false, executable);
}

OatFile* OatFile::OpenWritable(File* file, const std::string& location) {
  CheckLocation(location);
  return OpenElfFile(file, location, NULL, true, false);
}

OatFile* OatFile::OpenDlopen(const std::string& elf_filename,
                             const std::string& location,
                             byte* requested_base) {
  UniquePtr<OatFile> oat_file(new OatFile(location));
  bool success = oat_file->Dlopen(elf_filename, requested_base);
  if (!success) {
    return NULL;
  }
  return oat_file.release();
}

OatFile* OatFile::OpenElfFile(File* file,
                              const std::string& location,
                              byte* requested_base,
                              bool writable,
                              bool executable) {
  UniquePtr<OatFile> oat_file(new OatFile(location));
  bool success = oat_file->ElfFileOpen(file, requested_base, writable, executable);
  if (!success) {
    return NULL;
  }
  return oat_file.release();
}

OatFile::OatFile(const std::string& location)
    : location_(location), begin_(NULL), end_(NULL), dlopen_handle_(NULL) {
  CHECK(!location_.empty());
}

OatFile::~OatFile() {
  STLDeleteValues(&oat_dex_files_);
  if (dlopen_handle_ != NULL) {
    dlclose(dlopen_handle_);
  }
}

bool OatFile::Dlopen(const std::string& elf_filename, byte* requested_base) {
  char* absolute_path = realpath(elf_filename.c_str(), NULL);
  if (absolute_path == NULL) {
    VLOG(class_linker) << "Failed to find absolute path for " << elf_filename;
    return false;
  }
  dlopen_handle_ = dlopen(absolute_path, RTLD_NOW);
  free(absolute_path);
  if (dlopen_handle_ == NULL) {
    VLOG(class_linker) << "Failed to dlopen " << elf_filename << ": " << dlerror();
    return false;
  }
  begin_ = reinterpret_cast<byte*>(dlsym(dlopen_handle_, "oatdata"));
  if (begin_ == NULL) {
    LOG(WARNING) << "Failed to find oatdata symbol in " << elf_filename << ": " << dlerror();
    return false;
  }
  if (requested_base != NULL && begin_ != requested_base) {
    std::string maps;
    ReadFileToString("/proc/self/maps", &maps);
    LOG(WARNING) << "Failed to find oatdata symbol at expected address: oatdata="
                 << reinterpret_cast<const void*>(begin_) << " != expected="
                 << reinterpret_cast<const void*>(requested_base)
                 << " /proc/self/maps:\n" << maps;
    return false;
  }
  end_ = reinterpret_cast<byte*>(dlsym(dlopen_handle_, "oatlastword"));
  if (end_ == NULL) {
    LOG(WARNING) << "Failed to find oatlastword symbol in " << elf_filename << ": " << dlerror();
    return false;
  }
  // Readjust to be non-inclusive upper bound.
  end_ += sizeof(uint32_t);
  return Setup();
}

bool OatFile::ElfFileOpen(File* file, byte* requested_base, bool writable, bool executable) {
  elf_file_.reset(ElfFile::Open(file, writable, true));
  if (elf_file_.get() == NULL) {
    if (writable) {
      PLOG(WARNING) << "Failed to open ELF file for " << file->GetPath();
    }
    return false;
  }
  bool loaded = elf_file_->Load(executable);
  if (!loaded) {
    LOG(WARNING) << "Failed to load ELF file " << file->GetPath();
    return false;
  }
  begin_ = elf_file_->FindDynamicSymbolAddress("oatdata");
  if (begin_ == NULL) {
    LOG(WARNING) << "Failed to find oatdata symbol in " << file->GetPath();
    return false;
  }
  if (requested_base != NULL && begin_ != requested_base) {
    std::string maps;
    ReadFileToString("/proc/self/maps", &maps);
    LOG(WARNING) << "Failed to find oatdata symbol at expected address: oatdata="
                 << reinterpret_cast<const void*>(begin_) << " != expected="
                 << reinterpret_cast<const void*>(requested_base)
                 << " /proc/self/maps:\n" << maps;
    return false;
  }
  end_ = elf_file_->FindDynamicSymbolAddress("oatlastword");
  if (end_ == NULL) {
    LOG(WARNING) << "Failed to find oatlastword symbol in " << file->GetPath();
    return false;
  }
  // Readjust to be non-inclusive upper bound.
  end_ += sizeof(uint32_t);
  return Setup();
}

bool OatFile::Setup() {
  if (!GetOatHeader().IsValid()) {
    LOG(WARNING) << "Invalid oat magic for " << GetLocation();
    return false;
  }
  const byte* oat = Begin();
  oat += sizeof(OatHeader);
  if (oat > End()) {
    LOG(ERROR) << "In oat file " << GetLocation() << " found truncated OatHeader";
    return false;
  }

  oat += GetOatHeader().GetImageFileLocationSize();
  if (oat > End()) {
    LOG(ERROR) << "In oat file " << GetLocation() << " found truncated image file location: "
               << reinterpret_cast<const void*>(Begin())
               << "+" << sizeof(OatHeader)
               << "+" << GetOatHeader().GetImageFileLocationSize()
               << "<=" << reinterpret_cast<const void*>(End());
    return false;
  }

  for (size_t i = 0; i < GetOatHeader().GetDexFileCount(); i++) {
    size_t dex_file_location_size = *reinterpret_cast<const uint32_t*>(oat);
    if (dex_file_location_size == 0U) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " with empty location name";
      return false;
    }
    oat += sizeof(dex_file_location_size);
    if (oat > End()) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " truncated after dex file location size";
      return false;
    }

    const char* dex_file_location_data = reinterpret_cast<const char*>(oat);
    oat += dex_file_location_size;
    if (oat > End()) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " with truncated dex file location";
      return false;
    }

    std::string dex_file_location(dex_file_location_data, dex_file_location_size);

    uint32_t dex_file_checksum = *reinterpret_cast<const uint32_t*>(oat);
    oat += sizeof(dex_file_checksum);
    if (oat > End()) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " for "<< dex_file_location
                 << " truncated after dex file checksum";
      return false;
    }

    uint32_t dex_file_offset = *reinterpret_cast<const uint32_t*>(oat);
    if (dex_file_offset == 0U) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " for "<< dex_file_location
                 << " with zero dex file offset";
      return false;
    }
    if (dex_file_offset > Size()) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " for "<< dex_file_location
                 << " with dex file offset" << dex_file_offset << " > " << Size();
      return false;
    }
    oat += sizeof(dex_file_offset);
    if (oat > End()) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " for "<< dex_file_location
                 << " truncated after dex file offset";
      return false;
    }

    const uint8_t* dex_file_pointer = Begin() + dex_file_offset;
    if (!DexFile::IsMagicValid(dex_file_pointer)) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " for "<< dex_file_location
                 << " with invalid dex file magic: " << dex_file_pointer;
      return false;
    }
    if (!DexFile::IsVersionValid(dex_file_pointer)) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " for "<< dex_file_location
                 << " with invalid dex file version: " << dex_file_pointer;
      return false;
    }
    const DexFile::Header* header = reinterpret_cast<const DexFile::Header*>(dex_file_pointer);
    const uint32_t* methods_offsets_pointer = reinterpret_cast<const uint32_t*>(oat);

    oat += (sizeof(*methods_offsets_pointer) * header->class_defs_size_);
    if (oat > End()) {
      LOG(ERROR) << "In oat file " << GetLocation() << " found OatDexFile # " << i
                 << " for "<< dex_file_location
                 << " with truncated method offsets";
      return false;
    }

    oat_dex_files_.Put(dex_file_location, new OatDexFile(this,
                                                         dex_file_location,
                                                         dex_file_checksum,
                                                         dex_file_pointer,
                                                         methods_offsets_pointer));
  }
  return true;
}

const OatHeader& OatFile::GetOatHeader() const {
  return *reinterpret_cast<const OatHeader*>(Begin());
}

const byte* OatFile::Begin() const {
  CHECK(begin_ != NULL);
  return begin_;
}

const byte* OatFile::End() const {
  CHECK(end_ != NULL);
  return end_;
}

const OatFile::OatDexFile* OatFile::GetOatDexFile(const std::string& dex_location,
                                                  const uint32_t* const dex_location_checksum,
                                                  bool warn_if_not_found) const {
  Table::const_iterator it = oat_dex_files_.find(dex_location);
  if (it != oat_dex_files_.end()) {
    const OatFile::OatDexFile* oat_dex_file = it->second;
    if (dex_location_checksum == NULL ||
        oat_dex_file->GetDexFileLocationChecksum() == *dex_location_checksum) {
      return oat_dex_file;
    }
  }

  if (warn_if_not_found) {
    LOG(WARNING) << "Failed to find OatDexFile for DexFile " << dex_location
                 << " in OatFile " << GetLocation();
    if (kIsDebugBuild) {
      for (Table::const_iterator it = oat_dex_files_.begin(); it != oat_dex_files_.end(); ++it) {
        LOG(WARNING) << "OatFile " << GetLocation()
                     << " contains OatDexFile " << it->second->GetDexFileLocation();
      }
    }
  }
  return NULL;
}

std::vector<const OatFile::OatDexFile*> OatFile::GetOatDexFiles() const {
  std::vector<const OatFile::OatDexFile*> result;
  for (Table::const_iterator it = oat_dex_files_.begin(); it != oat_dex_files_.end(); ++it) {
    result.push_back(it->second);
  }
  return result;
}

OatFile::OatDexFile::OatDexFile(const OatFile* oat_file,
                                const std::string& dex_file_location,
                                uint32_t dex_file_location_checksum,
                                const byte* dex_file_pointer,
                                const uint32_t* oat_class_offsets_pointer)
    : oat_file_(oat_file),
      dex_file_location_(dex_file_location),
      dex_file_location_checksum_(dex_file_location_checksum),
      dex_file_pointer_(dex_file_pointer),
      oat_class_offsets_pointer_(oat_class_offsets_pointer) {}

OatFile::OatDexFile::~OatDexFile() {}

size_t OatFile::OatDexFile::FileSize() const {
  return reinterpret_cast<const DexFile::Header*>(dex_file_pointer_)->file_size_;
}

const DexFile* OatFile::OatDexFile::OpenDexFile() const {
  return DexFile::Open(dex_file_pointer_, FileSize(), dex_file_location_,
                       dex_file_location_checksum_);
}

const OatFile::OatClass* OatFile::OatDexFile::GetOatClass(uint16_t class_def_index) const {
  uint32_t oat_class_offset = oat_class_offsets_pointer_[class_def_index];

  const byte* oat_class_pointer = oat_file_->Begin() + oat_class_offset;
  CHECK_LT(oat_class_pointer, oat_file_->End()) << oat_file_->GetLocation();
  mirror::Class::Status status = *reinterpret_cast<const mirror::Class::Status*>(oat_class_pointer);

  const byte* methods_pointer = oat_class_pointer + sizeof(status);
  CHECK_LT(methods_pointer, oat_file_->End()) << oat_file_->GetLocation();

  return new OatClass(oat_file_,
                      status,
                      reinterpret_cast<const OatMethodOffsets*>(methods_pointer));
}

OatFile::OatClass::OatClass(const OatFile* oat_file,
                            mirror::Class::Status status,
                            const OatMethodOffsets* methods_pointer)
    : oat_file_(oat_file), status_(status), methods_pointer_(methods_pointer) {}

OatFile::OatClass::~OatClass() {}

mirror::Class::Status OatFile::OatClass::GetStatus() const {
  return status_;
}

const OatFile::OatMethod OatFile::OatClass::GetOatMethod(uint32_t method_index) const {
  const OatMethodOffsets& oat_method_offsets = methods_pointer_[method_index];
  return OatMethod(
      oat_file_->Begin(),
      oat_method_offsets.code_offset_,
      oat_method_offsets.frame_size_in_bytes_,
      oat_method_offsets.core_spill_mask_,
      oat_method_offsets.fp_spill_mask_,
      oat_method_offsets.mapping_table_offset_,
      oat_method_offsets.vmap_table_offset_,
      oat_method_offsets.gc_map_offset_);
}

OatFile::OatMethod::OatMethod(const byte* base,
                              const uint32_t code_offset,
                              const size_t frame_size_in_bytes,
                              const uint32_t core_spill_mask,
                              const uint32_t fp_spill_mask,
                              const uint32_t mapping_table_offset,
                              const uint32_t vmap_table_offset,
                              const uint32_t gc_map_offset)
  : begin_(base),
    code_offset_(code_offset),
    frame_size_in_bytes_(frame_size_in_bytes),
    core_spill_mask_(core_spill_mask),
    fp_spill_mask_(fp_spill_mask),
    mapping_table_offset_(mapping_table_offset),
    vmap_table_offset_(vmap_table_offset),
    native_gc_map_offset_(gc_map_offset) {
#ifndef NDEBUG
  if (mapping_table_offset_ != 0) {  // implies non-native, non-stub code
    if (vmap_table_offset_ == 0) {
      DCHECK_EQ(0U, static_cast<uint32_t>(__builtin_popcount(core_spill_mask_) +
                                          __builtin_popcount(fp_spill_mask_)));
    } else {
      VmapTable vmap_table(reinterpret_cast<const uint8_t*>(begin_ + vmap_table_offset_));

      DCHECK_EQ(vmap_table.Size(), static_cast<uint32_t>(__builtin_popcount(core_spill_mask_) +
                                                         __builtin_popcount(fp_spill_mask_)));
    }
  } else {
    DCHECK_EQ(vmap_table_offset_, 0U);
  }
#endif
}

OatFile::OatMethod::~OatMethod() {}

const void* OatFile::OatMethod::GetCode() const {
  return GetOatPointer<const void*>(code_offset_);
}

uint32_t OatFile::OatMethod::GetCodeSize() const {
#if defined(ART_USE_PORTABLE_COMPILER)
  // TODO: With Quick, we store the size before the code. With
  // Portable, the code is in a .o file we don't manage ourselves. ELF
  // symbols do have a concept of size, so we could capture that and
  // store it somewhere, such as the OatMethod.
  return 0;
#else
  uintptr_t code = reinterpret_cast<uint32_t>(GetCode());

  if (code == 0) {
    return 0;
  }
  // TODO: make this Thumb2 specific
  code &= ~0x1;
  return reinterpret_cast<uint32_t*>(code)[-1];
#endif
}

void OatFile::OatMethod::LinkMethod(mirror::ArtMethod* method) const {
  CHECK(method != NULL);
  method->SetEntryPointFromCompiledCode(GetCode());
  method->SetFrameSizeInBytes(frame_size_in_bytes_);
  method->SetCoreSpillMask(core_spill_mask_);
  method->SetFpSpillMask(fp_spill_mask_);
  method->SetMappingTable(GetMappingTable());
  method->SetVmapTable(GetVmapTable());
  method->SetNativeGcMap(GetNativeGcMap());  // Used by native methods in work around JNI mode.
}

}  // namespace art
