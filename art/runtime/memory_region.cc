/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "memory_region.h"

#include <stdint.h>
#include <string.h>

#include "base/logging.h"
#include "globals.h"

namespace art {

void MemoryRegion::CopyFrom(size_t offset, const MemoryRegion& from) const {
  CHECK(from.pointer() != NULL);
  CHECK_GT(from.size(), 0U);
  CHECK_GE(this->size(), from.size());
  CHECK_LE(offset, this->size() - from.size());
  memmove(reinterpret_cast<void*>(start() + offset),
          from.pointer(), from.size());
}

}  // namespace art
