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

#include "compiler_internals.h"
#include "dex_file-inl.h"
#include "arena_allocator.h"
#include "base/logging.h"
#include "base/mutex.h"

namespace art {

// Memmap is a bit slower than malloc according to my measurements.
static constexpr bool kUseMemMap = false;
static constexpr bool kUseMemSet = true && kUseMemMap;

static const char* alloc_names[ArenaAllocator::kNumAllocKinds] = {
  "Misc       ",
  "BasicBlock ",
  "LIR        ",
  "MIR        ",
  "DataFlow   ",
  "GrowList   ",
  "GrowBitMap ",
  "Dalvik2SSA ",
  "DebugInfo  ",
  "Successor  ",
  "RegAlloc   ",
  "Data       ",
  "Preds      ",
};

Arena::Arena(size_t size)
    : bytes_allocated_(0),
      map_(nullptr),
      next_(nullptr) {
  if (kUseMemMap) {
    map_ = MemMap::MapAnonymous("dalvik-arena", NULL, size, PROT_READ | PROT_WRITE);
    memory_ = map_->Begin();
    size_ = map_->Size();
  } else {
    memory_ = reinterpret_cast<uint8_t*>(calloc(1, size));
    size_ = size;
  }
}

Arena::~Arena() {
  if (kUseMemMap) {
    delete map_;
  } else {
    free(reinterpret_cast<void*>(memory_));
  }
}

void Arena::Reset() {
  if (bytes_allocated_) {
    if (kUseMemSet || !kUseMemMap) {
      memset(Begin(), 0, bytes_allocated_);
    } else {
      madvise(Begin(), bytes_allocated_, MADV_DONTNEED);
    }
    bytes_allocated_ = 0;
  }
}

ArenaPool::ArenaPool()
    : lock_("Arena pool lock"),
      free_arenas_(nullptr) {
}

ArenaPool::~ArenaPool() {
  while (free_arenas_ != nullptr) {
    auto* arena = free_arenas_;
    free_arenas_ = free_arenas_->next_;
    delete arena;
  }
}

Arena* ArenaPool::AllocArena(size_t size) {
  Thread* self = Thread::Current();
  Arena* ret = nullptr;
  {
    MutexLock lock(self, lock_);
    if (free_arenas_ != nullptr && LIKELY(free_arenas_->Size() >= size)) {
      ret = free_arenas_;
      free_arenas_ = free_arenas_->next_;
    }
  }
  if (ret == nullptr) {
    ret = new Arena(size);
  }
  ret->Reset();
  return ret;
}

void ArenaPool::FreeArena(Arena* arena) {
  Thread* self = Thread::Current();
  {
    MutexLock lock(self, lock_);
    arena->next_ = free_arenas_;
    free_arenas_ = arena;
  }
}

size_t ArenaAllocator::BytesAllocated() const {
  size_t total = 0;
  for (int i = 0; i < kNumAllocKinds; i++) {
    total += alloc_stats_[i];
  }
  return total;
}

ArenaAllocator::ArenaAllocator(ArenaPool* pool)
  : pool_(pool),
    begin_(nullptr),
    end_(nullptr),
    ptr_(nullptr),
    arena_head_(nullptr),
    num_allocations_(0) {
  memset(&alloc_stats_[0], 0, sizeof(alloc_stats_));
}

void ArenaAllocator::UpdateBytesAllocated() {
  if (arena_head_ != nullptr) {
    // Update how many bytes we have allocated into the arena so that the arena pool knows how
    // much memory to zero out.
    arena_head_->bytes_allocated_ = ptr_ - begin_;
  }
}

ArenaAllocator::~ArenaAllocator() {
  // Reclaim all the arenas by giving them back to the thread pool.
  UpdateBytesAllocated();
  while (arena_head_ != nullptr) {
    Arena* arena = arena_head_;
    arena_head_ = arena_head_->next_;
    pool_->FreeArena(arena);
  }
}

void ArenaAllocator::ObtainNewArenaForAllocation(size_t allocation_size) {
  UpdateBytesAllocated();
  Arena* new_arena = pool_->AllocArena(std::max(Arena::kDefaultSize, allocation_size));
  new_arena->next_ = arena_head_;
  arena_head_ = new_arena;
  // Update our internal data structures.
  ptr_ = begin_ = new_arena->Begin();
  end_ = new_arena->End();
}

// Dump memory usage stats.
void ArenaAllocator::DumpMemStats(std::ostream& os) const {
  size_t malloc_bytes = 0;
  // Start out with how many lost bytes we have in the arena we are currently allocating into.
  size_t lost_bytes(end_ - ptr_);
  size_t num_arenas = 0;
  for (Arena* arena = arena_head_; arena != nullptr; arena = arena->next_) {
    malloc_bytes += arena->Size();
    if (arena != arena_head_) {
      lost_bytes += arena->RemainingSpace();
    }
    ++num_arenas;
  }
  const size_t bytes_allocated = BytesAllocated();
  os << " MEM: used: " << bytes_allocated << ", allocated: " << malloc_bytes
     << ", lost: " << lost_bytes << "\n";
  if (num_allocations_ != 0) {
    os << "Number of arenas allocated: " << num_arenas << ", Number of allocations: "
       << num_allocations_ << ", avg size: " << bytes_allocated / num_allocations_ << "\n";
  }
  os << "===== Allocation by kind\n";
  for (int i = 0; i < kNumAllocKinds; i++) {
      os << alloc_names[i] << std::setw(10) << alloc_stats_[i] << "\n";
  }
}

}  // namespace art
