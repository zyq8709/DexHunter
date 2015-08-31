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

#include "buffered_output_stream.h"

#include <string.h>

namespace art {

BufferedOutputStream::BufferedOutputStream(OutputStream* out)
    : OutputStream(out->GetLocation()), out_(out), used_(0) {}

bool BufferedOutputStream::WriteFully(const void* buffer, int64_t byte_count) {
  if (byte_count > kBufferSize) {
    Flush();
    return out_->WriteFully(buffer, byte_count);
  }
  if (used_ + byte_count > kBufferSize) {
    bool success = Flush();
    if (!success) {
      return false;
    }
  }
  const uint8_t* src = reinterpret_cast<const uint8_t*>(buffer);
  memcpy(&buffer_[used_], src, byte_count);
  used_ += byte_count;
  return true;
}

bool BufferedOutputStream::Flush() {
  bool success = true;
  if (used_ > 0) {
    success = out_->WriteFully(&buffer_[0], used_);
    used_ = 0;
  }
  return success;
}

off_t BufferedOutputStream::Seek(off_t offset, Whence whence) {
  if (!Flush()) {
    return -1;
  }
  return out_->Seek(offset, whence);
}

}  // namespace art
