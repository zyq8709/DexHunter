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

#include "base/logging.h"
#include "base/unix_file/mapped_file.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <string>

namespace unix_file {

MappedFile::~MappedFile() {
}

int MappedFile::Close() {
  if (IsMapped()) {
    Unmap();
  }
  return FdFile::Close();
}

bool MappedFile::MapReadOnly() {
  CHECK(IsOpened());
  CHECK(!IsMapped());
  struct stat st;
  int result = TEMP_FAILURE_RETRY(fstat(Fd(), &st));
  if (result == -1) {
    PLOG(WARNING) << "Failed to stat file '" << GetPath() << "'";
    return false;
  }
  file_size_ = st.st_size;
  do {
    mapped_file_ = mmap(NULL, file_size_, PROT_READ, MAP_PRIVATE, Fd(), 0);
  } while (mapped_file_ == MAP_FAILED && errno == EINTR);
  if (mapped_file_ == MAP_FAILED) {
    PLOG(WARNING) << "Failed to mmap file '" << GetPath() << "' of size "
                  << file_size_ << " bytes to memory";
    return false;
  }
  map_mode_ = kMapReadOnly;
  return true;
}

bool MappedFile::MapReadWrite(int64_t file_size) {
  CHECK(IsOpened());
  CHECK(!IsMapped());
  int result = TEMP_FAILURE_RETRY(ftruncate64(Fd(), file_size));
  if (result == -1) {
    PLOG(ERROR) << "Failed to truncate file '" << GetPath()
                << "' to size " << file_size;
    return false;
  }
  file_size_ = file_size;
  do {
    mapped_file_ =
        mmap(NULL, file_size_, PROT_READ | PROT_WRITE, MAP_SHARED, Fd(), 0);
  } while (mapped_file_ == MAP_FAILED && errno == EINTR);
  if (mapped_file_ == MAP_FAILED) {
    PLOG(WARNING) << "Failed to mmap file '" << GetPath() << "' of size "
                  << file_size_ << " bytes to memory";
    return false;
  }
  map_mode_ = kMapReadWrite;
  return true;
}

bool MappedFile::Unmap() {
  CHECK(IsMapped());
  int result = TEMP_FAILURE_RETRY(munmap(mapped_file_, file_size_));
  if (result == -1) {
    PLOG(WARNING) << "Failed unmap file '" << GetPath() << "' of size "
                  << file_size_;
    return false;
  } else {
    mapped_file_ = NULL;
    file_size_ = -1;
    return true;
  }
}

int64_t MappedFile::Read(char* buf, int64_t byte_count, int64_t offset) const {
  if (IsMapped()) {
    if (offset < 0) {
      errno = EINVAL;
      return -errno;
    }
    int64_t read_size = std::max(0LL, std::min(byte_count, file_size_ - offset));
    if (read_size > 0) {
      memcpy(buf, data() + offset, read_size);
    }
    return read_size;
  } else {
    return FdFile::Read(buf, byte_count, offset);
  }
}

int MappedFile::SetLength(int64_t new_length) {
  CHECK(!IsMapped());
  return FdFile::SetLength(new_length);
}

int64_t MappedFile::GetLength() const {
  if (IsMapped()) {
    return file_size_;
  } else {
    return FdFile::GetLength();
  }
}

int MappedFile::Flush() {
  int rc = IsMapped() ? TEMP_FAILURE_RETRY(msync(mapped_file_, file_size_, 0)) : FdFile::Flush();
  return rc == -1 ? -errno : 0;
}

int64_t MappedFile::Write(const char* buf, int64_t byte_count, int64_t offset) {
  if (IsMapped()) {
    CHECK_EQ(kMapReadWrite, map_mode_);
    if (offset < 0) {
      errno = EINVAL;
      return -errno;
    }
    int64_t write_size = std::max(0LL, std::min(byte_count, file_size_ - offset));
    if (write_size > 0) {
      memcpy(data() + offset, buf, write_size);
    }
    return write_size;
  } else {
    return FdFile::Write(buf, byte_count, offset);
  }
}

int64_t MappedFile::size() const {
  return GetLength();
}

bool MappedFile::IsMapped() const {
  return mapped_file_ != NULL && mapped_file_ != MAP_FAILED;
}

char* MappedFile::data() const {
  CHECK(IsMapped());
  return static_cast<char*>(mapped_file_);
}

}  // namespace unix_file
