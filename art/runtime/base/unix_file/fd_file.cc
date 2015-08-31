/*
 * Copyright (C) 2009 The Android Open Source Project
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
#include "base/unix_file/fd_file.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace unix_file {

FdFile::FdFile() : fd_(-1), auto_close_(true) {
}

FdFile::FdFile(int fd) : fd_(fd), auto_close_(true) {
}

FdFile::FdFile(int fd, const std::string& path) : fd_(fd), file_path_(path), auto_close_(true) {
  CHECK_NE(0U, path.size());
}

FdFile::~FdFile() {
  if (auto_close_ && fd_ != -1) {
    Close();
  }
}

void FdFile::DisableAutoClose() {
  auto_close_ = false;
}

bool FdFile::Open(const std::string& path, int flags) {
  return Open(path, flags, 0640);
}

bool FdFile::Open(const std::string& path, int flags, mode_t mode) {
  CHECK_EQ(fd_, -1) << path;
  fd_ = TEMP_FAILURE_RETRY(open(path.c_str(), flags, mode));
  if (fd_ == -1) {
    return false;
  }
  file_path_ = path;
  return true;
}

int FdFile::Close() {
  int result = TEMP_FAILURE_RETRY(close(fd_));
  if (result == -1) {
    return -errno;
  } else {
    fd_ = -1;
    file_path_ = "";
    return 0;
  }
}

int FdFile::Flush() {
  int rc = TEMP_FAILURE_RETRY(fdatasync(fd_));
  return (rc == -1) ? -errno : rc;
}

int64_t FdFile::Read(char* buf, int64_t byte_count, int64_t offset) const {
  int rc = TEMP_FAILURE_RETRY(pread64(fd_, buf, byte_count, offset));
  return (rc == -1) ? -errno : rc;
}

int FdFile::SetLength(int64_t new_length) {
  int rc = TEMP_FAILURE_RETRY(ftruncate64(fd_, new_length));
  return (rc == -1) ? -errno : rc;
}

int64_t FdFile::GetLength() const {
  struct stat s;
  int rc = TEMP_FAILURE_RETRY(fstat(fd_, &s));
  return (rc == -1) ? -errno : s.st_size;
}

int64_t FdFile::Write(const char* buf, int64_t byte_count, int64_t offset) {
  int rc = TEMP_FAILURE_RETRY(pwrite64(fd_, buf, byte_count, offset));
  return (rc == -1) ? -errno : rc;
}

int FdFile::Fd() const {
  return fd_;
}

bool FdFile::IsOpened() const {
  return fd_ >= 0;
}

std::string FdFile::GetPath() const {
  return file_path_;
}

bool FdFile::ReadFully(void* buffer, int64_t byte_count) {
  char* ptr = static_cast<char*>(buffer);
  while (byte_count > 0) {
    int bytes_read = TEMP_FAILURE_RETRY(read(fd_, ptr, byte_count));
    if (bytes_read <= 0) {
      return false;
    }
    byte_count -= bytes_read;  // Reduce the number of remaining bytes.
    ptr += bytes_read;  // Move the buffer forward.
  }
  return true;
}

bool FdFile::WriteFully(const void* buffer, int64_t byte_count) {
  const char* ptr = static_cast<const char*>(buffer);
  while (byte_count > 0) {
    int bytes_read = TEMP_FAILURE_RETRY(write(fd_, ptr, byte_count));
    if (bytes_read < 0) {
      return false;
    }
    byte_count -= bytes_read;  // Reduce the number of remaining bytes.
    ptr += bytes_read;  // Move the buffer forward.
  }
  return true;
}

}  // namespace unix_file
