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

#ifndef ART_RUNTIME_GC_SPACE_DLMALLOC_SPACE_H_
#define ART_RUNTIME_GC_SPACE_DLMALLOC_SPACE_H_

#include "gc/allocator/dlmalloc.h"
#include "space.h"

namespace art {
namespace gc {

namespace collector {
  class MarkSweep;
}  // namespace collector

namespace space {

// An alloc space is a space where objects may be allocated and garbage collected.
class DlMallocSpace : public MemMapSpace, public AllocSpace {
 public:
  typedef void(*WalkCallback)(void *start, void *end, size_t num_bytes, void* callback_arg);

  SpaceType GetType() const {
    if (GetGcRetentionPolicy() == kGcRetentionPolicyFullCollect) {
      return kSpaceTypeZygoteSpace;
    } else {
      return kSpaceTypeAllocSpace;
    }
  }

  // Create a AllocSpace with the requested sizes. The requested
  // base address is not guaranteed to be granted, if it is required,
  // the caller should call Begin on the returned space to confirm
  // the request was granted.
  static DlMallocSpace* Create(const std::string& name, size_t initial_size, size_t growth_limit,
                               size_t capacity, byte* requested_begin);

  // Allocate num_bytes without allowing the underlying mspace to grow.
  virtual mirror::Object* AllocWithGrowth(Thread* self, size_t num_bytes,
                                          size_t* bytes_allocated) LOCKS_EXCLUDED(lock_);

  // Allocate num_bytes allowing the underlying mspace to grow.
  virtual mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated);

  // Return the storage space required by obj.
  virtual size_t AllocationSize(const mirror::Object* obj);
  virtual size_t Free(Thread* self, mirror::Object* ptr);
  virtual size_t FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs);

  mirror::Object* AllocNonvirtual(Thread* self, size_t num_bytes, size_t* bytes_allocated);

  size_t AllocationSizeNonvirtual(const mirror::Object* obj) {
    return mspace_usable_size(const_cast<void*>(reinterpret_cast<const void*>(obj))) +
        kChunkOverhead;
  }

  void* MoreCore(intptr_t increment);

  void* GetMspace() const {
    return mspace_;
  }

  // Hands unused pages back to the system.
  size_t Trim();

  // Perform a mspace_inspect_all which calls back for each allocation chunk. The chunk may not be
  // in use, indicated by num_bytes equaling zero.
  void Walk(WalkCallback callback, void* arg) LOCKS_EXCLUDED(lock_);

  // Returns the number of bytes that the space has currently obtained from the system. This is
  // greater or equal to the amount of live data in the space.
  size_t GetFootprint();

  // Returns the number of bytes that the heap is allowed to obtain from the system via MoreCore.
  size_t GetFootprintLimit();

  // Set the maximum number of bytes that the heap is allowed to obtain from the system via
  // MoreCore. Note this is used to stop the mspace growing beyond the limit to Capacity. When
  // allocations fail we GC before increasing the footprint limit and allowing the mspace to grow.
  void SetFootprintLimit(size_t limit);

  // Removes the fork time growth limit on capacity, allowing the application to allocate up to the
  // maximum reserved size of the heap.
  void ClearGrowthLimit() {
    growth_limit_ = NonGrowthLimitCapacity();
  }

  // Override capacity so that we only return the possibly limited capacity
  size_t Capacity() const {
    return growth_limit_;
  }

  // The total amount of memory reserved for the alloc space.
  size_t NonGrowthLimitCapacity() const {
    return GetMemMap()->Size();
  }

  accounting::SpaceBitmap* GetLiveBitmap() const {
    return live_bitmap_.get();
  }

  accounting::SpaceBitmap* GetMarkBitmap() const {
    return mark_bitmap_.get();
  }

  void Dump(std::ostream& os) const;

  void SetGrowthLimit(size_t growth_limit);

  // Swap the live and mark bitmaps of this space. This is used by the GC for concurrent sweeping.
  void SwapBitmaps();

  // Turn ourself into a zygote space and return a new alloc space which has our unused memory.
  DlMallocSpace* CreateZygoteSpace(const char* alloc_space_name);

  uint64_t GetBytesAllocated() const {
    return num_bytes_allocated_;
  }

  uint64_t GetObjectsAllocated() const {
    return num_objects_allocated_;
  }

  uint64_t GetTotalBytesAllocated() const {
    return total_bytes_allocated_;
  }

  uint64_t GetTotalObjectsAllocated() const {
    return total_objects_allocated_;
  }

  // Returns the class of a recently freed object.
  mirror::Class* FindRecentFreedObject(const mirror::Object* obj);

 protected:
  DlMallocSpace(const std::string& name, MemMap* mem_map, void* mspace, byte* begin, byte* end,
                size_t growth_limit);

 private:
  size_t InternalAllocationSize(const mirror::Object* obj);
  mirror::Object* AllocWithoutGrowthLocked(size_t num_bytes, size_t* bytes_allocated)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool Init(size_t initial_size, size_t maximum_size, size_t growth_size, byte* requested_base);
  void RegisterRecentFree(mirror::Object* ptr);
  static void* CreateMallocSpace(void* base, size_t morecore_start, size_t initial_size);

  UniquePtr<accounting::SpaceBitmap> live_bitmap_;
  UniquePtr<accounting::SpaceBitmap> mark_bitmap_;
  UniquePtr<accounting::SpaceBitmap> temp_bitmap_;

  // Recent allocation buffer.
  static constexpr size_t kRecentFreeCount = kDebugSpaces ? (1 << 16) : 0;
  static constexpr size_t kRecentFreeMask = kRecentFreeCount - 1;
  std::pair<const mirror::Object*, mirror::Class*> recent_freed_objects_[kRecentFreeCount];
  size_t recent_free_pos_;

  // Approximate number of bytes which have been allocated into the space.
  size_t num_bytes_allocated_;
  size_t num_objects_allocated_;
  size_t total_bytes_allocated_;
  size_t total_objects_allocated_;

  static size_t bitmap_index_;

  // The boundary tag overhead.
  static const size_t kChunkOverhead = kWordSize;

  // Used to ensure mutual exclusion when the allocation spaces data structures are being modified.
  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  // Underlying malloc space
  void* const mspace_;

  // The capacity of the alloc space until such time that ClearGrowthLimit is called.
  // The underlying mem_map_ controls the maximum size we allow the heap to grow to. The growth
  // limit is a value <= to the mem_map_ capacity used for ergonomic reasons because of the zygote.
  // Prior to forking the zygote the heap will have a maximally sized mem_map_ but the growth_limit_
  // will be set to a lower value. The growth_limit_ is used as the capacity of the alloc_space_,
  // however, capacity normally can't vary. In the case of the growth_limit_ it can be cleared
  // one time by a call to ClearGrowthLimit.
  size_t growth_limit_;

  friend class collector::MarkSweep;

  DISALLOW_COPY_AND_ASSIGN(DlMallocSpace);
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_DLMALLOC_SPACE_H_
