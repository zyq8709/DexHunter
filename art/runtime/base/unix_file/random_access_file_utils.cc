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

#include <vector>
#include "base/unix_file/random_access_file_utils.h"
#include "base/unix_file/random_access_file.h"

namespace unix_file {

bool CopyFile(const RandomAccessFile& src, RandomAccessFile* dst) {
  // We don't call src->GetLength because some files (those in /proc, say)
  // don't know how long they are. We just read until there's nothing left.
  std::vector<char> buf(4096);
  int64_t offset = 0;
  int64_t n;
  while ((n = src.Read(&buf[0], buf.size(), offset)) > 0) {
    if (dst->Write(&buf[0], n, offset) != n) {
      return false;
    }
    offset += n;
  }
  return n >= 0;
}

}  // namespace unix_file
