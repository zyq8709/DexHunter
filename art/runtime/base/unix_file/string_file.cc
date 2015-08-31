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

#include "base/unix_file/string_file.h"
#include <errno.h>
#include <algorithm>
#include "base/logging.h"

namespace unix_file {

StringFile::StringFile() {
}

StringFile::~StringFile() {
}

int StringFile::Close() {
  return 0;
}

int StringFile::Flush() {
  return 0;
}

int64_t StringFile::Read(char *buf, int64_t byte_count, int64_t offset) const {
  CHECK(buf);
  CHECK_GE(byte_count, 0);

  if (offset < 0) {
    return -EINVAL;
  }

  const int64_t available_bytes = std::min(byte_count, GetLength() - offset);
  if (available_bytes < 0) {
    return 0;  // Not an error, but nothing for us to do, either.
  }
  memcpy(buf, data_.data() + offset, available_bytes);
  return available_bytes;
}

int StringFile::SetLength(int64_t new_length) {
  if (new_length < 0) {
    return -EINVAL;
  }
  data_.resize(new_length);
  return 0;
}

int64_t StringFile::GetLength() const {
  return data_.size();
}

int64_t StringFile::Write(const char *buf, int64_t byte_count, int64_t offset) {
  CHECK(buf);
  CHECK_GE(byte_count, 0);

  if (offset < 0) {
    return -EINVAL;
  }

  if (byte_count == 0) {
    return 0;
  }

  // FUSE seems happy to allow writes past the end. (I'd guess it doesn't
  // synthesize a write of zero bytes so that we're free to implement sparse
  // files.) GNU as(1) seems to require such writes. Those files are small.
  const int64_t bytes_past_end = offset - GetLength();
  if (bytes_past_end > 0) {
    data_.append(bytes_past_end, '\0');
  }

  data_.replace(offset, byte_count, buf, byte_count);
  return byte_count;
}

void StringFile::Assign(const art::StringPiece &new_data) {
  data_.assign(new_data.data(), new_data.size());
}

const art::StringPiece StringFile::ToStringPiece() const {
  return data_;
}

}  // namespace unix_file
