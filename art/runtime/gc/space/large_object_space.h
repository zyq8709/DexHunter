/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_SPACE_LARGE_OBJECT_SPACE_H_
#define ART_RUNTIME_GC_SPACE_LARGE_OBJECT_SPACE_H_

#include "gc/accounting/gc_allocator.h"
#include "dlmalloc_space.h"
#include "safe_map.h"
#include "space.h"

#include <set>
#include <vector>

namespace art {
namespace gc {
namespace space {

// Abstraction implemented by all large object spaces.
class LargeObjectSpace : public DiscontinuousSpace, public AllocSpace {
 public:
  virtual SpaceType GetType() const {
    return kSpaceTypeLargeObjectSpace;
  }

  virtual void SwapBitmaps();
  virtual void CopyLiveToMarked();
  virtual void Walk(DlMallocSpace::WalkCallback, void* arg) = 0;
  virtual ~LargeObjectSpace() {}

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

  size_t FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs);

 protected:
  explicit LargeObjectSpace(const std::string& name);

  // Approximate number of bytes which have been allocated into the space.
  size_t num_bytes_allocated_;
  size_t num_objects_allocated_;
  size_t total_bytes_allocated_;
  size_t total_objects_allocated_;

  friend class Space;

 private:
  DISALLOW_COPY_AND_ASSIGN(LargeObjectSpace);
};

// A discontinuous large object space implemented by individual mmap/munmap calls.
class LargeObjectMapSpace : public LargeObjectSpace {
 public:
  // Creates a large object space. Allocations into the large object space use memory maps instead
  // of malloc.
  static LargeObjectMapSpace* Create(const std::string& name);

  // Return the storage space required by obj.
  size_t AllocationSize(const mirror::Object* obj);
  mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated);
  size_t Free(Thread* self, mirror::Object* ptr);
  void Walk(DlMallocSpace::WalkCallback, void* arg) LOCKS_EXCLUDED(lock_);
  // TODO: disabling thread safety analysis as this may be called when we already hold lock_.
  bool Contains(const mirror::Object* obj) const NO_THREAD_SAFETY_ANALYSIS;

 private:
  explicit LargeObjectMapSpace(const std::string& name);
  virtual ~LargeObjectMapSpace() {}

  // Used to ensure mutual exclusion when the allocation spaces data structures are being modified.
  mutable Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::vector<mirror::Object*,
      accounting::GCAllocator<mirror::Object*> > large_objects_ GUARDED_BY(lock_);
  typedef SafeMap<mirror::Object*, MemMap*, std::less<mirror::Object*>,
      accounting::GCAllocator<std::pair<const mirror::Object*, MemMap*> > > MemMaps;
  MemMaps mem_maps_ GUARDED_BY(lock_);
};

// A continuous large object space with a free-list to handle holes.
class FreeListSpace : public LargeObjectSpace {
 public:
  virtual ~FreeListSpace();
  static FreeListSpace* Create(const std::string& name, byte* requested_begin, size_t capacity);

  size_t AllocationSize(const mirror::Object* obj) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated);
  size_t Free(Thread* self, mirror::Object* obj);
  bool Contains(const mirror::Object* obj) const;
  void Walk(DlMallocSpace::WalkCallback callback, void* arg) LOCKS_EXCLUDED(lock_);

  // Address at which the space begins.
  byte* Begin() const {
    return begin_;
  }

  // Address at which the space ends, which may vary as the space is filled.
  byte* End() const {
    return end_;
  }

  // Current size of space
  size_t Size() const {
    return End() - Begin();
  }

  void Dump(std::ostream& os) const;

 private:
  static const size_t kAlignment = kPageSize;

  class AllocationHeader {
   public:
    // Returns the allocation size, includes the header.
    size_t AllocationSize() const {
      return alloc_size_;
    }

    // Updates the allocation size in the header, the allocation size includes the header itself.
    void SetAllocationSize(size_t size) {
      DCHECK(IsAligned<kPageSize>(size));
      alloc_size_ = size;
    }

    bool IsFree() const {
      return AllocationSize() == 0;
    }

    // Returns the previous free allocation header by using the prev_free_ member to figure out
    // where it is. If prev free is 0 then we just return ourself.
    AllocationHeader* GetPrevFreeAllocationHeader() {
      return reinterpret_cast<AllocationHeader*>(reinterpret_cast<uintptr_t>(this) - prev_free_);
    }

    // Returns the address of the object associated with this allocation header.
    mirror::Object* GetObjectAddress() {
      return reinterpret_cast<mirror::Object*>(reinterpret_cast<uintptr_t>(this) + sizeof(*this));
    }

    // Returns the next allocation header after the object associated with this allocation header.
    AllocationHeader* GetNextAllocationHeader() {
      DCHECK_NE(alloc_size_, 0U);
      return reinterpret_cast<AllocationHeader*>(reinterpret_cast<uintptr_t>(this) + alloc_size_);
    }

    // Returns how many free bytes there is before the block.
    size_t GetPrevFree() const {
      return prev_free_;
    }

    // Update the size of the free block prior to the allocation.
    void SetPrevFree(size_t prev_free) {
      DCHECK(IsAligned<kPageSize>(prev_free));
      prev_free_ = prev_free;
    }

    // Finds and returns the next non free allocation header after ourself.
    // TODO: Optimize, currently O(n) for n free following pages.
    AllocationHeader* GetNextNonFree();

    // Used to implement best fit object allocation. Each allocation has an AllocationHeader which
    // contains the size of the previous free block preceding it. Implemented in such a way that we
    // can also find the iterator for any allocation header pointer.
    class SortByPrevFree {
     public:
      bool operator()(const AllocationHeader* a, const AllocationHeader* b) const {
        if (a->GetPrevFree() < b->GetPrevFree()) return true;
        if (a->GetPrevFree() > b->GetPrevFree()) return false;
        if (a->AllocationSize() < b->AllocationSize()) return true;
        if (a->AllocationSize() > b->AllocationSize()) return false;
        return reinterpret_cast<uintptr_t>(a) < reinterpret_cast<uintptr_t>(b);
      }
    };

   private:
    // Contains the size of the previous free block, if 0 then the memory preceding us is an
    // allocation.
    size_t prev_free_;

    // Allocation size of this object, 0 means that the allocation header is free memory.
    size_t alloc_size_;

    friend class FreeListSpace;
  };

  FreeListSpace(const std::string& name, MemMap* mem_map, byte* begin, byte* end);

  // Removes header from the free blocks set by finding the corresponding iterator and erasing it.
  void RemoveFreePrev(AllocationHeader* header) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Finds the allocation header corresponding to obj.
  AllocationHeader* GetAllocationHeader(const mirror::Object* obj);

  typedef std::set<AllocationHeader*, AllocationHeader::SortByPrevFree,
                   accounting::GCAllocator<AllocationHeader*> > FreeBlocks;

  byte* const begin_;
  byte* const end_;

  // There is not footer for any allocations at the end of the space, so we keep track of how much
  // free space there is at the end manually.
  UniquePtr<MemMap> mem_map_;
  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  size_t free_end_ GUARDED_BY(lock_);
  FreeBlocks free_blocks_ GUARDED_BY(lock_);
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_LARGE_OBJECT_SPACE_H_
