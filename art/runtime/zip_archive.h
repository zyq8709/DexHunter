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

#ifndef ART_RUNTIME_ZIP_ARCHIVE_H_
#define ART_RUNTIME_ZIP_ARCHIVE_H_

#include <stdint.h>
#include <zlib.h>

#include "base/logging.h"
#include "base/stringpiece.h"
#include "base/unix_file/random_access_file.h"
#include "globals.h"
#include "mem_map.h"
#include "os.h"
#include "safe_map.h"
#include "UniquePtr.h"

namespace art {

class ZipArchive;
class MemMap;

class ZipEntry {
 public:
  bool ExtractToFile(File& file);
  bool ExtractToMemory(uint8_t* begin, size_t size);
  MemMap* ExtractToMemMap(const char* entry_filename);

  uint32_t GetUncompressedLength();
  uint32_t GetCrc32();

 private:
  ZipEntry(const ZipArchive* zip_archive, const byte* ptr) : zip_archive_(zip_archive), ptr_(ptr) {}

  // Zip compression methods
  enum {
    kCompressStored     = 0,        // no compression
    kCompressDeflated   = 8,        // standard deflate
  };

  // kCompressStored, kCompressDeflated, ...
  uint16_t GetCompressionMethod();

  uint32_t GetCompressedLength();

  // returns -1 on error
  off64_t GetDataOffset();

  const ZipArchive* zip_archive_;

  // pointer to zip entry within central directory
  const byte* ptr_;

  friend class ZipArchive;
  DISALLOW_COPY_AND_ASSIGN(ZipEntry);
};

class ZipArchive {
 public:
  // Zip file constants.
  static const uint32_t kEOCDSignature      = 0x06054b50;
  static const int32_t kEOCDLen             = 22;
  static const int32_t kEOCDDiskNumber      =  4;              // number of the current disk
  static const int32_t kEOCDDiskNumberForCD =  6;              // disk number with the Central Directory
  static const int32_t kEOCDNumEntries      =  8;              // offset to #of entries in file
  static const int32_t kEOCDTotalNumEntries = 10;              // offset to total #of entries in spanned archives
  static const int32_t kEOCDSize            = 12;              // size of the central directory
  static const int32_t kEOCDFileOffset      = 16;              // offset to central directory
  static const int32_t kEOCDCommentSize     = 20;              // offset to the length of the file comment

  static const int32_t kMaxCommentLen = 65535;  // longest possible in uint16_t
  static const int32_t kMaxEOCDSearch = (kMaxCommentLen + kEOCDLen);

  static const uint32_t kLFHSignature = 0x04034b50;
  static const int32_t kLFHLen        = 30;  // excluding variable-len fields
  static const int32_t kLFHGPBFlags   = 6;   // offset to GPB flags
  static const int32_t kLFHNameLen    = 26;  // offset to filename length
  static const int32_t kLFHExtraLen   = 28;  // offset to extra length

  static const uint32_t kCDESignature   = 0x02014b50;
  static const int32_t kCDELen          = 46;  // excluding variable-len fields
  static const int32_t kCDEGPBFlags     = 8;   // offset to GPB flags
  static const int32_t kCDEMethod       = 10;  // offset to compression method
  static const int32_t kCDEModWhen      = 12;  // offset to modification timestamp
  static const int32_t kCDECRC          = 16;  // offset to entry CRC
  static const int32_t kCDECompLen      = 20;  // offset to compressed length
  static const int32_t kCDEUncompLen    = 24;  // offset to uncompressed length
  static const int32_t kCDENameLen      = 28;  // offset to filename length
  static const int32_t kCDEExtraLen     = 30;  // offset to extra length
  static const int32_t kCDECommentLen   = 32;  // offset to comment length
  static const int32_t kCDELocalOffset  = 42;  // offset to local hdr

  // General Purpose Bit Flag
  static const int32_t kGPFEncryptedFlag   = (1 << 0);
  static const int32_t kGPFUnsupportedMask = (kGPFEncryptedFlag);

  // return new ZipArchive instance on success, NULL on error.
  static ZipArchive* Open(const std::string& filename);
  static ZipArchive* OpenFromFd(int fd);

  ZipEntry* Find(const char* name) const;

  ~ZipArchive() {
    Close();
  }

 private:
  explicit ZipArchive(int fd) : fd_(fd), num_entries_(0), dir_offset_(0) {}

  bool MapCentralDirectory();
  bool Parse();
  void Close();

  int fd_;
  uint16_t num_entries_;
  off64_t dir_offset_;
  UniquePtr<MemMap> dir_map_;
  typedef SafeMap<StringPiece, const byte*> DirEntries;
  DirEntries dir_entries_;

  friend class ZipEntry;

  DISALLOW_COPY_AND_ASSIGN(ZipArchive);
};

}  // namespace art

#endif  // ART_RUNTIME_ZIP_ARCHIVE_H_
