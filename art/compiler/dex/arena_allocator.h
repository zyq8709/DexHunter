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

#ifndef ART_COMPILER_DEX_ARENA_ALLOCATOR_H_
#define ART_COMPILER_DEX_ARENA_ALLOCATOR_H_

#include <stdint.h>
#include <stddef.h>

#include "base/mutex.h"
#include "compiler_enums.h"
#include "mem_map.h"

namespace art {

class Arena;
class ArenaPool;
class ArenaAllocator;

class Arena {
 public:
  static constexpr size_t kDefaultSize = 128 * KB;
  explicit Arena(size_t size = kDefaultSize);
  ~Arena();
  void Reset();
  uint8_t* Begin() {
    return memory_;
  }

  uint8_t* End() {
    return memory_ + size_;
  }

  size_t Size() const {
    return size_;
  }

  size_t RemainingSpace() const {
    return Size() - bytes_allocated_;
  }

 private:
  size_t bytes_allocated_;
  uint8_t* memory_;
  size_t size_;
  MemMap* map_;
  Arena* next_;
  friend class ArenaPool;
  friend class ArenaAllocator;
  DISALLOW_COPY_AND_ASSIGN(Arena);
};

class ArenaPool {
 public:
  ArenaPool();
  ~ArenaPool();
  Arena* AllocArena(size_t size);
  void FreeArena(Arena* arena);

 private:
  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  Arena* free_arenas_ GUARDED_BY(lock_);
  DISALLOW_COPY_AND_ASSIGN(ArenaPool);
};

class ArenaAllocator {
 public:
  // Type of allocation for memory tuning.
  enum ArenaAllocKind {
    kAllocMisc,
    kAllocBB,
    kAllocLIR,
    kAllocMIR,
    kAllocDFInfo,
    kAllocGrowableArray,
    kAllocGrowableBitMap,
    kAllocDalvikToSSAMap,
    kAllocDebugInfo,
    kAllocSuccessor,
    kAllocRegAlloc,
    kAllocData,
    kAllocPredecessors,
    kNumAllocKinds
  };

  static constexpr bool kCountAllocations = false;

  explicit ArenaAllocator(ArenaPool* pool);
  ~ArenaAllocator();

  // Returns zeroed memory.
  void* Alloc(size_t bytes, ArenaAllocKind kind) ALWAYS_INLINE {
    bytes = (bytes + 3) & ~3;
    if (UNLIKELY(ptr_ + bytes > end_)) {
      // Obtain a new block.
      ObtainNewArenaForAllocation(bytes);
      if (UNLIKELY(ptr_ == nullptr)) {
        return nullptr;
      }
    }
    if (kCountAllocations) {
      alloc_stats_[kind] += bytes;
      ++num_allocations_;
    }
    uint8_t* ret = ptr_;
    ptr_ += bytes;
    return ret;
  }

  void ObtainNewArenaForAllocation(size_t allocation_size);
  size_t BytesAllocated() const;
  void DumpMemStats(std::ostream& os) const;

 private:
  void UpdateBytesAllocated();

  ArenaPool* pool_;
  uint8_t* begin_;
  uint8_t* end_;
  uint8_t* ptr_;
  Arena* arena_head_;

  // Statistics.
  size_t num_allocations_;
  size_t alloc_stats_[kNumAllocKinds];   // Bytes used by various allocation kinds.

  DISALLOW_COPY_AND_ASSIGN(ArenaAllocator);
};  // ArenaAllocator

struct MemStats {
   public:
     void Dump(std::ostream& os) const {
       arena_.DumpMemStats(os);
     }
     explicit MemStats(const ArenaAllocator &arena) : arena_(arena) {}
  private:
    const ArenaAllocator &arena_;
};  // MemStats

}  // namespace art

#endif  // ART_COMPILER_DEX_ARENA_ALLOCATOR_H_
