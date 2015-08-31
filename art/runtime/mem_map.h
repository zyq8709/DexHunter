/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_RUNTIME_MEM_MAP_H_
#define ART_RUNTIME_MEM_MAP_H_

#include <string>

#include <stddef.h>
#include <sys/mman.h>  // For the PROT_* and MAP_* constants.
#include <sys/types.h>

#include "globals.h"

namespace art {

// Used to keep track of mmap segments.
class MemMap {
 public:
  // Request an anonymous region of length 'byte_count' and a requested base address.
  // Use NULL as the requested base address if you don't care.
  //
  // The word "anonymous" in this context means "not backed by a file". The supplied
  // 'ashmem_name' will be used -- on systems that support it -- to give the mapping
  // a name.
  //
  // On success, returns returns a MemMap instance.  On failure, returns a NULL;
  static MemMap* MapAnonymous(const char* ashmem_name, byte* addr, size_t byte_count, int prot);

  // Map part of a file, taking care of non-page aligned offsets.  The
  // "start" offset is absolute, not relative.
  //
  // On success, returns returns a MemMap instance.  On failure, returns a NULL;
  static MemMap* MapFile(size_t byte_count, int prot, int flags, int fd, off_t start) {
    return MapFileAtAddress(NULL, byte_count, prot, flags, fd, start, false);
  }

  // Map part of a file, taking care of non-page aligned offsets.  The
  // "start" offset is absolute, not relative. This version allows
  // requesting a specific address for the base of the mapping.
  //
  // On success, returns returns a MemMap instance.  On failure, returns a NULL;
  static MemMap* MapFileAtAddress(
      byte* addr, size_t byte_count, int prot, int flags, int fd, off_t start, bool reuse);

  // Releases the memory mapping
  ~MemMap();

  bool Protect(int prot);

  int GetProtect() const {
    return prot_;
  }

  byte* Begin() const {
    return begin_;
  }

  size_t Size() const {
    return size_;
  }

  byte* End() const {
    return Begin() + Size();
  }

  bool HasAddress(const void* addr) const {
    return Begin() <= addr && addr < End();
  }

  // Trim by unmapping pages at the end of the map.
  void UnMapAtEnd(byte* new_end);

 private:
  MemMap(const std::string& name, byte* begin, size_t size, void* base_begin, size_t base_size,
         int prot);

  std::string name_;
  byte* const begin_;  // Start of data.
  size_t size_;  // Length of data.

  void* const base_begin_;  // Page-aligned base address.
  const size_t base_size_;  // Length of mapping.
  int prot_;  // Protection of the map.
};

}  // namespace art

#endif  // ART_RUNTIME_MEM_MAP_H_
