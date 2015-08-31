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

#ifndef ART_COMPILER_OUTPUT_STREAM_H_
#define ART_COMPILER_OUTPUT_STREAM_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"

namespace art {

enum Whence {
  kSeekSet = SEEK_SET,
  kSeekCurrent = SEEK_CUR,
  kSeekEnd = SEEK_END,
};

class OutputStream {
 public:
  explicit OutputStream(const std::string& location) : location_(location) {}

  virtual ~OutputStream() {}

  const std::string& GetLocation() const {
    return location_;
  }

  virtual bool WriteFully(const void* buffer, int64_t byte_count) = 0;

  virtual off_t Seek(off_t offset, Whence whence) = 0;

 private:
  const std::string location_;

  DISALLOW_COPY_AND_ASSIGN(OutputStream);
};

}  // namespace art

#endif  // ART_COMPILER_OUTPUT_STREAM_H_
