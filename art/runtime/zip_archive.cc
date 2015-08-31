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

#include "zip_archive.h"

#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/unix_file/fd_file.h"
#include "UniquePtr.h"

namespace art {

static const size_t kBufSize = 32 * KB;

// Get 2 little-endian bytes.
static uint32_t Le16ToHost(const byte* src) {
  return ((src[0] <<  0) |
          (src[1] <<  8));
}

// Get 4 little-endian bytes.
static uint32_t Le32ToHost(const byte* src) {
  return ((src[0] <<  0) |
          (src[1] <<  8) |
          (src[2] << 16) |
          (src[3] << 24));
}

uint16_t ZipEntry::GetCompressionMethod() {
  return Le16ToHost(ptr_ + ZipArchive::kCDEMethod);
}

uint32_t ZipEntry::GetCompressedLength() {
  return Le32ToHost(ptr_ + ZipArchive::kCDECompLen);
}

uint32_t ZipEntry::GetUncompressedLength() {
  return Le32ToHost(ptr_ + ZipArchive::kCDEUncompLen);
}

uint32_t ZipEntry::GetCrc32() {
  return Le32ToHost(ptr_ + ZipArchive::kCDECRC);
}

off64_t ZipEntry::GetDataOffset() {
  // All we have is the offset to the Local File Header, which is
  // variable size, so we have to read the contents of the struct to
  // figure out where the actual data starts.

  // We also need to make sure that the lengths are not so large that
  // somebody trying to map the compressed or uncompressed data runs
  // off the end of the mapped region.

  off64_t dir_offset = zip_archive_->dir_offset_;
  int64_t lfh_offset = Le32ToHost(ptr_ + ZipArchive::kCDELocalOffset);
  if (lfh_offset + ZipArchive::kLFHLen >= dir_offset) {
    LOG(WARNING) << "Zip: bad LFH offset in zip";
    return -1;
  }

  if (lseek64(zip_archive_->fd_, lfh_offset, SEEK_SET) != lfh_offset) {
    PLOG(WARNING) << "Zip: failed seeking to LFH at offset " << lfh_offset;
    return -1;
  }

  uint8_t lfh_buf[ZipArchive::kLFHLen];
  ssize_t actual = TEMP_FAILURE_RETRY(read(zip_archive_->fd_, lfh_buf, sizeof(lfh_buf)));
  if (actual != sizeof(lfh_buf)) {
    LOG(WARNING) << "Zip: failed reading LFH from offset " << lfh_offset;
    return -1;
  }

  if (Le32ToHost(lfh_buf) != ZipArchive::kLFHSignature) {
    LOG(WARNING) << "Zip: didn't find signature at start of LFH, offset " << lfh_offset;
    return -1;
  }

  uint32_t gpbf = Le16ToHost(lfh_buf + ZipArchive::kLFHGPBFlags);
  if ((gpbf & ZipArchive::kGPFUnsupportedMask) != 0) {
    LOG(WARNING) << "Invalid General Purpose Bit Flag: " << gpbf;
    return -1;
  }

  off64_t data_offset = (lfh_offset + ZipArchive::kLFHLen
                       + Le16ToHost(lfh_buf + ZipArchive::kLFHNameLen)
                       + Le16ToHost(lfh_buf + ZipArchive::kLFHExtraLen));
  if (data_offset >= dir_offset) {
    LOG(WARNING) << "Zip: bad data offset " << data_offset << " in zip";
    return -1;
  }

  // check lengths

  if (static_cast<off64_t>(data_offset + GetCompressedLength()) > dir_offset) {
    LOG(WARNING) << "Zip: bad compressed length in zip "
                 << "(" << data_offset << " + " << GetCompressedLength()
                 << " > " << dir_offset << ")";
    return -1;
  }

  if (GetCompressionMethod() == kCompressStored
      && static_cast<off64_t>(data_offset + GetUncompressedLength()) > dir_offset) {
    LOG(WARNING) << "Zip: bad uncompressed length in zip "
                 << "(" << data_offset << " + " << GetUncompressedLength()
                 << " > " << dir_offset << ")";
    return -1;
  }

  return data_offset;
}

static bool CopyFdToMemory(uint8_t* begin, size_t size, int in, size_t count) {
  uint8_t* dst = begin;
  std::vector<uint8_t> buf(kBufSize);
  while (count != 0) {
    size_t bytes_to_read = (count > kBufSize) ? kBufSize : count;
    ssize_t actual = TEMP_FAILURE_RETRY(read(in, &buf[0], bytes_to_read));
    if (actual != static_cast<ssize_t>(bytes_to_read)) {
      PLOG(WARNING) << "Zip: short read";
      return false;
    }
    memcpy(dst, &buf[0], bytes_to_read);
    dst += bytes_to_read;
    count -= bytes_to_read;
  }
  DCHECK_EQ(dst, begin + size);
  return true;
}

class ZStream {
 public:
  ZStream(byte* write_buf, size_t write_buf_size) {
    // Initialize the zlib stream struct.
    memset(&zstream_, 0, sizeof(zstream_));
    zstream_.zalloc = Z_NULL;
    zstream_.zfree = Z_NULL;
    zstream_.opaque = Z_NULL;
    zstream_.next_in = NULL;
    zstream_.avail_in = 0;
    zstream_.next_out = reinterpret_cast<Bytef*>(write_buf);
    zstream_.avail_out = write_buf_size;
    zstream_.data_type = Z_UNKNOWN;
  }

  z_stream& Get() {
    return zstream_;
  }

  ~ZStream() {
    inflateEnd(&zstream_);
  }
 private:
  z_stream zstream_;
};

static bool InflateToMemory(uint8_t* begin, size_t size,
                            int in, size_t uncompressed_length, size_t compressed_length) {
  uint8_t* dst = begin;
  UniquePtr<uint8_t[]> read_buf(new uint8_t[kBufSize]);
  UniquePtr<uint8_t[]> write_buf(new uint8_t[kBufSize]);
  if (read_buf.get() == NULL || write_buf.get() == NULL) {
    LOG(WARNING) << "Zip: failed to allocate buffer to inflate";
    return false;
  }

  UniquePtr<ZStream> zstream(new ZStream(write_buf.get(), kBufSize));

  // Use the undocumented "negative window bits" feature to tell zlib
  // that there's no zlib header waiting for it.
  int zerr = inflateInit2(&zstream->Get(), -MAX_WBITS);
  if (zerr != Z_OK) {
    if (zerr == Z_VERSION_ERROR) {
      LOG(ERROR) << "Installed zlib is not compatible with linked version (" << ZLIB_VERSION << ")";
    } else {
      LOG(WARNING) << "Call to inflateInit2 failed (zerr=" << zerr << ")";
    }
    return false;
  }

  size_t remaining = compressed_length;
  do {
    // read as much as we can
    if (zstream->Get().avail_in == 0) {
      size_t bytes_to_read = (remaining > kBufSize) ? kBufSize : remaining;

        ssize_t actual = TEMP_FAILURE_RETRY(read(in, read_buf.get(), bytes_to_read));
        if (actual != static_cast<ssize_t>(bytes_to_read)) {
          LOG(WARNING) << "Zip: inflate read failed (" << actual << " vs " << bytes_to_read << ")";
          return false;
        }
        remaining -= bytes_to_read;
        zstream->Get().next_in = read_buf.get();
        zstream->Get().avail_in = bytes_to_read;
    }

    // uncompress the data
    zerr = inflate(&zstream->Get(), Z_NO_FLUSH);
    if (zerr != Z_OK && zerr != Z_STREAM_END) {
      LOG(WARNING) << "Zip: inflate zerr=" << zerr
                   << " (next_in=" << zstream->Get().next_in
                   << " avail_in=" << zstream->Get().avail_in
                   << " next_out=" << zstream->Get().next_out
                   << " avail_out=" << zstream->Get().avail_out
                   << ")";
      return false;
    }

    // write when we're full or when we're done
    if (zstream->Get().avail_out == 0 ||
        (zerr == Z_STREAM_END && zstream->Get().avail_out != kBufSize)) {
      size_t bytes_to_write = zstream->Get().next_out - write_buf.get();
      memcpy(dst, write_buf.get(), bytes_to_write);
      dst += bytes_to_write;
      zstream->Get().next_out = write_buf.get();
      zstream->Get().avail_out = kBufSize;
    }
  } while (zerr == Z_OK);

  DCHECK_EQ(zerr, Z_STREAM_END);  // other errors should've been caught

  // paranoia
  if (zstream->Get().total_out != uncompressed_length) {
    LOG(WARNING) << "Zip: size mismatch on inflated file ("
                 << zstream->Get().total_out << " vs " << uncompressed_length << ")";
    return false;
  }

  DCHECK_EQ(dst, begin + size);
  return true;
}

bool ZipEntry::ExtractToFile(File& file) {
  uint32_t length = GetUncompressedLength();
  int result = TEMP_FAILURE_RETRY(ftruncate(file.Fd(), length));
  if (result == -1) {
    PLOG(WARNING) << "Zip: failed to ftruncate " << file.GetPath() << " to length " << length;
    return false;
  }

  UniquePtr<MemMap> map(MemMap::MapFile(length, PROT_READ | PROT_WRITE, MAP_SHARED, file.Fd(), 0));
  if (map.get() == NULL) {
    LOG(WARNING) << "Zip: failed to mmap space for " << file.GetPath();
    return false;
  }

  return ExtractToMemory(map->Begin(), map->Size());
}

bool ZipEntry::ExtractToMemory(uint8_t* begin, size_t size) {
  // If size is zero, data offset will be meaningless, so bail out early.
  if (size == 0) {
    return true;
  }
  off64_t data_offset = GetDataOffset();
  if (data_offset == -1) {
    LOG(WARNING) << "Zip: data_offset=" << data_offset;
    return false;
  }
  if (lseek64(zip_archive_->fd_, data_offset, SEEK_SET) != data_offset) {
    PLOG(WARNING) << "Zip: lseek to data at " << data_offset << " failed";
    return false;
  }

  // TODO: this doesn't verify the data's CRC, but probably should (especially
  // for uncompressed data).
  switch (GetCompressionMethod()) {
    case kCompressStored:
      return CopyFdToMemory(begin, size, zip_archive_->fd_, GetUncompressedLength());
    case kCompressDeflated:
      return InflateToMemory(begin, size, zip_archive_->fd_,
                             GetUncompressedLength(), GetCompressedLength());
    default:
      LOG(WARNING) << "Zip: unknown compression method " << std::hex << GetCompressionMethod();
      return false;
  }
}

MemMap* ZipEntry::ExtractToMemMap(const char* entry_filename) {
  std::string name(entry_filename);
  name += " extracted in memory from ";
  name += entry_filename;
  UniquePtr<MemMap> map(MemMap::MapAnonymous(name.c_str(),
                                             NULL,
                                             GetUncompressedLength(),
                                             PROT_READ | PROT_WRITE));
  if (map.get() == NULL) {
    LOG(ERROR) << "Zip: mmap for '" << entry_filename << "' failed";
    return NULL;
  }

  bool success = ExtractToMemory(map->Begin(), map->Size());
  if (!success) {
    LOG(ERROR) << "Zip: Failed to extract '" << entry_filename << "' to memory";
    return NULL;
  }

  return map.release();
}

static void SetCloseOnExec(int fd) {
  // This dance is more portable than Linux's O_CLOEXEC open(2) flag.
  int flags = fcntl(fd, F_GETFD);
  if (flags == -1) {
    PLOG(WARNING) << "fcntl(" << fd << ", F_GETFD) failed";
    return;
  }
  int rc = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
  if (rc == -1) {
    PLOG(WARNING) << "fcntl(" << fd << ", F_SETFD, " << flags << ") failed";
    return;
  }
}

ZipArchive* ZipArchive::Open(const std::string& filename) {
  DCHECK(!filename.empty());
  int fd = open(filename.c_str(), O_RDONLY, 0);
  if (fd == -1) {
    PLOG(WARNING) << "Unable to open '" << filename << "'";
    return NULL;
  }
  return OpenFromFd(fd);
}

ZipArchive* ZipArchive::OpenFromFd(int fd) {
  SetCloseOnExec(fd);
  UniquePtr<ZipArchive> zip_archive(new ZipArchive(fd));
  if (zip_archive.get() == NULL) {
      return NULL;
  }
  if (!zip_archive->MapCentralDirectory()) {
      zip_archive->Close();
      return NULL;
  }
  if (!zip_archive->Parse()) {
      zip_archive->Close();
      return NULL;
  }
  return zip_archive.release();
}

ZipEntry* ZipArchive::Find(const char* name) const {
  DCHECK(name != NULL);
  DirEntries::const_iterator it = dir_entries_.find(name);
  if (it == dir_entries_.end()) {
    return NULL;
  }
  return new ZipEntry(this, (*it).second);
}

void ZipArchive::Close() {
  if (fd_ != -1) {
    close(fd_);
  }
  fd_ = -1;
  num_entries_ = 0;
  dir_offset_ = 0;
}

// Find the zip Central Directory and memory-map it.
//
// On success, returns true after populating fields from the EOCD area:
//   num_entries_
//   dir_offset_
//   dir_map_
bool ZipArchive::MapCentralDirectory() {
  /*
   * Get and test file length.
   */
  off64_t file_length = lseek64(fd_, 0, SEEK_END);
  if (file_length < kEOCDLen) {
    LOG(WARNING) << "Zip: length " << file_length << " is too small to be zip";
    return false;
  }

  size_t read_amount = kMaxEOCDSearch;
  if (file_length < off64_t(read_amount)) {
    read_amount = file_length;
  }

  UniquePtr<uint8_t[]> scan_buf(new uint8_t[read_amount]);
  if (scan_buf.get() == NULL) {
    return false;
  }

  /*
   * Make sure this is a Zip archive.
   */
  if (lseek64(fd_, 0, SEEK_SET) != 0) {
    PLOG(WARNING) << "seek to start failed: ";
    return false;
  }

  ssize_t actual = TEMP_FAILURE_RETRY(read(fd_, scan_buf.get(), sizeof(int32_t)));
  if (actual != static_cast<ssize_t>(sizeof(int32_t))) {
    PLOG(INFO) << "couldn't read first signature from zip archive: ";
    return false;
  }

  unsigned int header = Le32ToHost(scan_buf.get());
  if (header != kLFHSignature) {
    LOG(VERBOSE) << "Not a Zip archive (found " << std::hex << header << ")";
    return false;
  }

  // Perform the traditional EOCD snipe hunt.
  //
  // We're searching for the End of Central Directory magic number,
  // which appears at the start of the EOCD block.  It's followed by
  // 18 bytes of EOCD stuff and up to 64KB of archive comment.  We
  // need to read the last part of the file into a buffer, dig through
  // it to find the magic number, parse some values out, and use those
  // to determine the extent of the CD.
  //
  // We start by pulling in the last part of the file.
  off64_t search_start = file_length - read_amount;

  if (lseek64(fd_, search_start, SEEK_SET) != search_start) {
    PLOG(WARNING) << "Zip: seek " << search_start << " failed";
    return false;
  }
  actual = TEMP_FAILURE_RETRY(read(fd_, scan_buf.get(), read_amount));
  if (actual != static_cast<ssize_t>(read_amount)) {
    PLOG(WARNING) << "Zip: read " << actual << ", expected " << read_amount << ". failed";
    return false;
  }


  // Scan backward for the EOCD magic.  In an archive without a trailing
  // comment, we'll find it on the first try.  (We may want to consider
  // doing an initial minimal read; if we don't find it, retry with a
  // second read as above.)
  int i;
  for (i = read_amount - kEOCDLen; i >= 0; i--) {
    if (scan_buf.get()[i] == 0x50 && Le32ToHost(&(scan_buf.get())[i]) == kEOCDSignature) {
      break;
    }
  }
  if (i < 0) {
    LOG(WARNING) << "Zip: EOCD not found, not a zip file";
    return false;
  }

  off64_t eocd_offset = search_start + i;
  const byte* eocd_ptr = scan_buf.get() + i;

  DCHECK(eocd_offset < file_length);

  // Grab the CD offset and size, and the number of entries in the
  // archive.  Verify that they look reasonable.
  uint16_t disk_number = Le16ToHost(eocd_ptr + kEOCDDiskNumber);
  uint16_t disk_with_central_dir = Le16ToHost(eocd_ptr + kEOCDDiskNumberForCD);
  uint16_t num_entries = Le16ToHost(eocd_ptr + kEOCDNumEntries);
  uint16_t total_num_entries = Le16ToHost(eocd_ptr + kEOCDTotalNumEntries);
  uint32_t dir_size = Le32ToHost(eocd_ptr + kEOCDSize);
  uint32_t dir_offset = Le32ToHost(eocd_ptr + kEOCDFileOffset);
  uint16_t comment_size = Le16ToHost(eocd_ptr + kEOCDCommentSize);

  if ((uint64_t) dir_offset + (uint64_t) dir_size > (uint64_t) eocd_offset) {
    LOG(WARNING) << "Zip: bad offsets ("
                 << "dir=" << dir_offset << ", "
                 << "size=" << dir_size  << ", "
                 << "eocd=" << eocd_offset << ")";
    return false;
  }
  if (num_entries == 0) {
    LOG(WARNING) << "Zip: empty archive?";
    return false;
  } else if (num_entries != total_num_entries || disk_number != 0 || disk_with_central_dir != 0) {
    LOG(WARNING) << "spanned archives not supported";
    return false;
  }

  // Check to see if comment is a sane size
  if ((comment_size > (file_length - kEOCDLen))
      || (eocd_offset > (file_length - kEOCDLen) - comment_size)) {
    LOG(WARNING) << "comment size runs off end of file";
    return false;
  }

  // It all looks good.  Create a mapping for the CD.
  dir_map_.reset(MemMap::MapFile(dir_size, PROT_READ, MAP_SHARED, fd_, dir_offset));
  if (dir_map_.get() == NULL) {
    return false;
  }

  num_entries_ = num_entries;
  dir_offset_ = dir_offset;
  return true;
}

bool ZipArchive::Parse() {
  const byte* cd_ptr = dir_map_->Begin();
  size_t cd_length = dir_map_->Size();

  // Walk through the central directory, adding entries to the hash
  // table and verifying values.
  const byte* ptr = cd_ptr;
  for (int i = 0; i < num_entries_; i++) {
    if (Le32ToHost(ptr) != kCDESignature) {
      LOG(WARNING) << "Zip: missed a central dir sig (at " << i << ")";
      return false;
    }
    if (ptr + kCDELen > cd_ptr + cd_length) {
      LOG(WARNING) << "Zip: ran off the end (at " << i << ")";
      return false;
    }

    int64_t local_hdr_offset = Le32ToHost(ptr + kCDELocalOffset);
    if (local_hdr_offset >= dir_offset_) {
      LOG(WARNING) << "Zip: bad LFH offset " << local_hdr_offset << " at entry " << i;
      return false;
    }

    uint16_t gpbf = Le16ToHost(ptr + kCDEGPBFlags);
    if ((gpbf & kGPFUnsupportedMask) != 0) {
      LOG(WARNING) << "Invalid General Purpose Bit Flag: " << gpbf;
      return false;
    }

    uint16_t name_len = Le16ToHost(ptr + kCDENameLen);
    uint16_t extra_len = Le16ToHost(ptr + kCDEExtraLen);
    uint16_t comment_len = Le16ToHost(ptr + kCDECommentLen);

    // add the CDE filename to the hash table
    const char* name = reinterpret_cast<const char*>(ptr + kCDELen);

    // Check name for NULL characters
    if (memchr(name, 0, name_len) != NULL) {
      LOG(WARNING) << "Filename contains NUL byte";
      return false;
    }

    dir_entries_.Put(StringPiece(name, name_len), ptr);
    ptr += kCDELen + name_len + extra_len + comment_len;
    if (ptr > cd_ptr + cd_length) {
      LOG(WARNING) << "Zip: bad CD advance "
                   << "(" << ptr << " vs " << (cd_ptr + cd_length) << ") "
                   << "at entry " << i;
      return false;
    }
  }
  return true;
}

}  // namespace art
