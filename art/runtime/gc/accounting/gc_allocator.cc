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

#include "gc_allocator.h"
#include "gc/allocator/dlmalloc.h"
#include "gc/heap.h"
#include "runtime.h"

namespace art {
namespace gc {
namespace accounting {
  void* RegisterGCAllocation(size_t bytes) {
    Runtime::Current()->GetHeap()->RegisterGCAllocation(bytes);
    return malloc(bytes);
  }

  void RegisterGCDeAllocation(void* p, size_t bytes) {
    Runtime::Current()->GetHeap()->RegisterGCDeAllocation(bytes);
    free(p);
  }
}  // namespace accounting
}  // namespace gc
}  // namespace art
