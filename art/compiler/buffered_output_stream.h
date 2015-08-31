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

#ifndef ART_COMPILER_BUFFERED_OUTPUT_STREAM_H_
#define ART_COMPILER_BUFFERED_OUTPUT_STREAM_H_

#include "output_stream.h"

#include "globals.h"

namespace art {

class BufferedOutputStream : public OutputStream {
 public:
  explicit BufferedOutputStream(OutputStream* out);

  virtual ~BufferedOutputStream() {
    delete out_;
  }

  virtual bool WriteFully(const void* buffer, int64_t byte_count);

  virtual off_t Seek(off_t offset, Whence whence);

 private:
  static const size_t kBufferSize = 8 * KB;

  bool Flush();

  OutputStream* const out_;

  uint8_t buffer_[kBufferSize];

  size_t used_;

  DISALLOW_COPY_AND_ASSIGN(BufferedOutputStream);
};

}  // namespace art

#endif  // ART_COMPILER_BUFFERED_OUTPUT_STREAM_H_
