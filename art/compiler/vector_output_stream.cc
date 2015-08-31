/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "vector_output_stream.h"

#include "base/logging.h"

namespace art {

VectorOutputStream::VectorOutputStream(const std::string& location, std::vector<uint8_t>& vector)
  : OutputStream(location), offset_(vector.size()), vector_(vector) {}

off_t VectorOutputStream::Seek(off_t offset, Whence whence) {
  CHECK(whence == kSeekSet || whence == kSeekCurrent || whence == kSeekEnd) << whence;
  off_t new_offset = 0;
  switch (whence) {
    case kSeekSet: {
      new_offset = offset;
      break;
    }
    case kSeekCurrent: {
      new_offset = offset_ + offset;
      break;
    }
    case kSeekEnd: {
      new_offset = vector_.size() + offset;
      break;
    }
  }
  EnsureCapacity(new_offset);
  offset_ = new_offset;
  return offset_;
}

}  // namespace art
