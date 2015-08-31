/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include "os.h"

#include <cstddef>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "base/logging.h"
#include "base/unix_file/fd_file.h"
#include "UniquePtr.h"

namespace art {

File* OS::OpenFileForReading(const char* name) {
  return OpenFileWithFlags(name, O_RDONLY);
}

File* OS::OpenFileReadWrite(const char* name) {
  return OpenFileWithFlags(name, O_RDWR);
}

File* OS::CreateEmptyFile(const char* name) {
  return OpenFileWithFlags(name, O_RDWR | O_CREAT | O_TRUNC);
}

File* OS::OpenFileWithFlags(const char* name, int flags) {
  CHECK(name != NULL);
  UniquePtr<File> file(new File);
  if (!file->Open(name, flags, 0666)) {
    return NULL;
  }
  return file.release();
}

bool OS::FileExists(const char* name) {
  struct stat st;
  if (stat(name, &st) == 0) {
    return S_ISREG(st.st_mode);  // TODO: Deal with symlinks?
  } else {
    return false;
  }
}

bool OS::DirectoryExists(const char* name) {
  struct stat st;
  if (stat(name, &st) == 0) {
    return S_ISDIR(st.st_mode);  // TODO: Deal with symlinks?
  } else {
    return false;
  }
}

}  // namespace art
