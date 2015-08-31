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

#include "file_output_stream.h"

#include <sys/types.h>
#include <unistd.h>

#include "base/unix_file/fd_file.h"

namespace art {

FileOutputStream::FileOutputStream(File* file) : OutputStream(file->GetPath()), file_(file) {}

bool FileOutputStream::WriteFully(const void* buffer, int64_t byte_count) {
  return file_->WriteFully(buffer, byte_count);
}

off_t FileOutputStream::Seek(off_t offset, Whence whence) {
  return lseek(file_->Fd(), offset, static_cast<int>(whence));
}

}  // namespace art
