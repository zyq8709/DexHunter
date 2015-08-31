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

#ifndef ART_COMPILER_FILE_OUTPUT_STREAM_H_
#define ART_COMPILER_FILE_OUTPUT_STREAM_H_

#include "output_stream.h"

#include "os.h"

namespace art {

class FileOutputStream : public OutputStream {
 public:
  explicit FileOutputStream(File* file);

  virtual ~FileOutputStream() {}

  virtual bool WriteFully(const void* buffer, int64_t byte_count);

  virtual off_t Seek(off_t offset, Whence whence);

 private:
  File* const file_;

  DISALLOW_COPY_AND_ASSIGN(FileOutputStream);
};

}  // namespace art

#endif  // ART_COMPILER_FILE_OUTPUT_STREAM_H_
