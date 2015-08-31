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
#include "dlmalloc_space.h"
#include "dlmalloc_space-inl.h"
#include "gc/accounting/card_table.h"
#include "gc/heap.h"
#include "mirror/object-inl.h"
#include "runtime.h"
#include "thread.h"
#include "utils.h"

#include <valgrind.h>
#include <../memcheck/memcheck.h>

namespace art {
namespace gc {
namespace space {

// TODO: Remove define macro
#define CHECK_MEMORY_CALL(call, args, what) \
  do { \
    int rc = call args; \
    if (UNLIKELY(rc != 0)) { \
      errno = rc; \
      PLOG(FATAL) << # call << " failed for " << what; \
    } \
  } while (false)

static const bool kPrefetchDuringDlMallocFreeList = true;

// Number of bytes to use as a red zone (rdz). A red zone of this size will be placed before and
// after each allocation. 8 bytes provides long/double alignment.
const size_t kValgrindRedZoneBytes = 8;

// A specialization of DlMallocSpace that provides information to valgrind wrt allocations.
class ValgrindDlMallocSpace : public DlMallocSpace {
 public:
  virtual mirror::Object* AllocWithGrowth(Thread* self, size_t num_bytes, size_t* bytes_allocated) {
    void* obj_with_rdz = DlMallocSpace::AllocWithGrowth(self, num_bytes + 2 * kValgrindRedZoneBytes,
                                                        bytes_allocated);
    if (obj_with_rdz == NULL) {
      return NULL;
    }
    mirror::Object* result = reinterpret_cast<mirror::Object*>(
        reinterpret_cast<byte*>(obj_with_rdz) + kValgrindRedZoneBytes);
    // Make redzones as no access.
    VALGRIND_MAKE_MEM_NOACCESS(obj_with_rdz, kValgrindRedZoneBytes);
    VALGRIND_MAKE_MEM_NOACCESS(reinterpret_cast<byte*>(result) + num_bytes, kValgrindRedZoneBytes);
    return result;
  }

  virtual mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated) {
    void* obj_with_rdz = DlMallocSpace::Alloc(self, num_bytes + 2 * kValgrindRedZoneBytes,
                                              bytes_allocated);
    if (obj_with_rdz == NULL) {
     return NULL;
    }
    mirror::Object* result = reinterpret_cast<mirror::Object*>(
        reinterpret_cast<byte*>(obj_with_rdz) + kValgrindRedZoneBytes);
    // Make redzones as no access.
    VALGRIND_MAKE_MEM_NOACCESS(obj_with_rdz, kValgrindRedZoneBytes);
    VALGRIND_MAKE_MEM_NOACCESS(reinterpret_cast<byte*>(result) + num_bytes, kValgrindRedZoneBytes);
    return result;
  }

  virtual size_t AllocationSize(const mirror::Object* obj) {
    size_t result = DlMallocSpace::AllocationSize(reinterpret_cast<const mirror::Object*>(
        reinterpret_cast<const byte*>(obj) - kValgrindRedZoneBytes));
    return result - 2 * kValgrindRedZoneBytes;
  }

  virtual size_t Free(Thread* self, mirror::Object* ptr) {
    void* obj_after_rdz = reinterpret_cast<void*>(ptr);
    void* obj_with_rdz = reinterpret_cast<byte*>(obj_after_rdz) - kValgrindRedZoneBytes;
    // Make redzones undefined.
    size_t allocation_size = DlMallocSpace::AllocationSize(
        reinterpret_cast<mirror::Object*>(obj_with_rdz));
    VALGRIND_MAKE_MEM_UNDEFINED(obj_with_rdz, allocation_size);
    size_t freed = DlMallocSpace::Free(self, reinterpret_cast<mirror::Object*>(obj_with_rdz));
    return freed - 2 * kValgrindRedZoneBytes;
  }

  virtual size_t FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs) {
    size_t freed = 0;
    for (size_t i = 0; i < num_ptrs; i++) {
      freed += Free(self, ptrs[i]);
    }
    return freed;
  }

  ValgrindDlMallocSpace(const std::string& name, MemMap* mem_map, void* mspace, byte* begin,
                        byte* end, size_t growth_limit, size_t initial_size) :
      DlMallocSpace(name, mem_map, mspace, begin, end, growth_limit) {
    VALGRIND_MAKE_MEM_UNDEFINED(mem_map->Begin() + initial_size, mem_map->Size() - initial_size);
  }

  virtual ~ValgrindDlMallocSpace() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ValgrindDlMallocSpace);
};

size_t DlMallocSpace::bitmap_index_ = 0;

DlMallocSpace::DlMallocSpace(const std::string& name, MemMap* mem_map, void* mspace, byte* begin,
                       byte* end, size_t growth_limit)
    : MemMapSpace(name, mem_map, end - begin, kGcRetentionPolicyAlwaysCollect),
      recent_free_pos_(0), num_bytes_allocated_(0), num_objects_allocated_(0),
      total_bytes_allocated_(0), total_objects_allocated_(0),
      lock_("allocation space lock", kAllocSpaceLock), mspace_(mspace),
      growth_limit_(growth_limit) {
  CHECK(mspace != NULL);

  size_t bitmap_index = bitmap_index_++;

  static const uintptr_t kGcCardSize = static_cast<uintptr_t>(accounting::CardTable::kCardSize);
  CHECK(IsAligned<kGcCardSize>(reinterpret_cast<uintptr_t>(mem_map->Begin())));
  CHECK(IsAligned<kGcCardSize>(reinterpret_cast<uintptr_t>(mem_map->End())));
  live_bitmap_.reset(accounting::SpaceBitmap::Create(
      StringPrintf("allocspace %s live-bitmap %d", name.c_str(), static_cast<int>(bitmap_index)),
      Begin(), Capacity()));
  DCHECK(live_bitmap_.get() != NULL) << "could not create allocspace live bitmap #" << bitmap_index;

  mark_bitmap_.reset(accounting::SpaceBitmap::Create(
      StringPrintf("allocspace %s mark-bitmap %d", name.c_str(), static_cast<int>(bitmap_index)),
      Begin(), Capacity()));
  DCHECK(live_bitmap_.get() != NULL) << "could not create allocspace mark bitmap #" << bitmap_index;

  for (auto& freed : recent_freed_objects_) {
    freed.first = nullptr;
    freed.second = nullptr;
  }
}

DlMallocSpace* DlMallocSpace::Create(const std::string& name, size_t initial_size, size_t
                                     growth_limit, size_t capacity, byte* requested_begin) {
  // Memory we promise to dlmalloc before it asks for morecore.
  // Note: making this value large means that large allocations are unlikely to succeed as dlmalloc
  // will ask for this memory from sys_alloc which will fail as the footprint (this value plus the
  // size of the large allocation) will be greater than the footprint limit.
  size_t starting_size = kPageSize;
  uint64_t start_time = 0;
  if (VLOG_IS_ON(heap) || VLOG_IS_ON(startup)) {
    start_time = NanoTime();
    VLOG(startup) << "Space::CreateAllocSpace entering " << name
                  << " initial_size=" << PrettySize(initial_size)
                  << " growth_limit=" << PrettySize(growth_limit)
                  << " capacity=" << PrettySize(capacity)
                  << " requested_begin=" << reinterpret_cast<void*>(requested_begin);
  }

  // Sanity check arguments
  if (starting_size > initial_size) {
    initial_size = starting_size;
  }
  if (initial_size > growth_limit) {
    LOG(ERROR) << "Failed to create alloc space (" << name << ") where the initial size ("
        << PrettySize(initial_size) << ") is larger than its capacity ("
        << PrettySize(growth_limit) << ")";
    return NULL;
  }
  if (growth_limit > capacity) {
    LOG(ERROR) << "Failed to create alloc space (" << name << ") where the growth limit capacity ("
        << PrettySize(growth_limit) << ") is larger than the capacity ("
        << PrettySize(capacity) << ")";
    return NULL;
  }

  // Page align growth limit and capacity which will be used to manage mmapped storage
  growth_limit = RoundUp(growth_limit, kPageSize);
  capacity = RoundUp(capacity, kPageSize);

  UniquePtr<MemMap> mem_map(MemMap::MapAnonymous(name.c_str(), requested_begin, capacity,
                                                 PROT_READ | PROT_WRITE));
  if (mem_map.get() == NULL) {
    LOG(ERROR) << "Failed to allocate pages for alloc space (" << name << ") of size "
        << PrettySize(capacity);
    return NULL;
  }

  void* mspace = CreateMallocSpace(mem_map->Begin(), starting_size, initial_size);
  if (mspace == NULL) {
    LOG(ERROR) << "Failed to initialize mspace for alloc space (" << name << ")";
    return NULL;
  }

  // Protect memory beyond the initial size.
  byte* end = mem_map->Begin() + starting_size;
  if (capacity - initial_size > 0) {
    CHECK_MEMORY_CALL(mprotect, (end, capacity - initial_size, PROT_NONE), name);
  }

  // Everything is set so record in immutable structure and leave
  MemMap* mem_map_ptr = mem_map.release();
  DlMallocSpace* space;
  if (RUNNING_ON_VALGRIND > 0) {
    space = new ValgrindDlMallocSpace(name, mem_map_ptr, mspace, mem_map_ptr->Begin(), end,
                                      growth_limit, initial_size);
  } else {
    space = new DlMallocSpace(name, mem_map_ptr, mspace, mem_map_ptr->Begin(), end, growth_limit);
  }
  if (VLOG_IS_ON(heap) || VLOG_IS_ON(startup)) {
    LOG(INFO) << "Space::CreateAllocSpace exiting (" << PrettyDuration(NanoTime() - start_time)
        << " ) " << *space;
  }
  return space;
}

void* DlMallocSpace::CreateMallocSpace(void* begin, size_t morecore_start, size_t initial_size) {
  // clear errno to allow PLOG on error
  errno = 0;
  // create mspace using our backing storage starting at begin and with a footprint of
  // morecore_start. Don't use an internal dlmalloc lock (as we already hold heap lock). When
  // morecore_start bytes of memory is exhaused morecore will be called.
  void* msp = create_mspace_with_base(begin, morecore_start, false /*locked*/);
  if (msp != NULL) {
    // Do not allow morecore requests to succeed beyond the initial size of the heap
    mspace_set_footprint_limit(msp, initial_size);
  } else {
    PLOG(ERROR) << "create_mspace_with_base failed";
  }
  return msp;
}

void DlMallocSpace::SwapBitmaps() {
  live_bitmap_.swap(mark_bitmap_);
  // Swap names to get more descriptive diagnostics.
  std::string temp_name(live_bitmap_->GetName());
  live_bitmap_->SetName(mark_bitmap_->GetName());
  mark_bitmap_->SetName(temp_name);
}

mirror::Object* DlMallocSpace::Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated) {
  return AllocNonvirtual(self, num_bytes, bytes_allocated);
}

mirror::Object* DlMallocSpace::AllocWithGrowth(Thread* self, size_t num_bytes, size_t* bytes_allocated) {
  mirror::Object* result;
  {
    MutexLock mu(self, lock_);
    // Grow as much as possible within the mspace.
    size_t max_allowed = Capacity();
    mspace_set_footprint_limit(mspace_, max_allowed);
    // Try the allocation.
    result = AllocWithoutGrowthLocked(num_bytes, bytes_allocated);
    // Shrink back down as small as possible.
    size_t footprint = mspace_footprint(mspace_);
    mspace_set_footprint_limit(mspace_, footprint);
  }
  if (result != NULL) {
    // Zero freshly allocated memory, done while not holding the space's lock.
    memset(result, 0, num_bytes);
  }
  // Return the new allocation or NULL.
  CHECK(!kDebugSpaces || result == NULL || Contains(result));
  return result;
}

void DlMallocSpace::SetGrowthLimit(size_t growth_limit) {
  growth_limit = RoundUp(growth_limit, kPageSize);
  growth_limit_ = growth_limit;
  if (Size() > growth_limit_) {
    end_ = begin_ + growth_limit;
  }
}

DlMallocSpace* DlMallocSpace::CreateZygoteSpace(const char* alloc_space_name) {
  end_ = reinterpret_cast<byte*>(RoundUp(reinterpret_cast<uintptr_t>(end_), kPageSize));
  DCHECK(IsAligned<accounting::CardTable::kCardSize>(begin_));
  DCHECK(IsAligned<accounting::CardTable::kCardSize>(end_));
  DCHECK(IsAligned<kPageSize>(begin_));
  DCHECK(IsAligned<kPageSize>(end_));
  size_t size = RoundUp(Size(), kPageSize);
  // Trim the heap so that we minimize the size of the Zygote space.
  Trim();
  // Trim our mem-map to free unused pages.
  GetMemMap()->UnMapAtEnd(end_);
  // TODO: Not hardcode these in?
  const size_t starting_size = kPageSize;
  const size_t initial_size = 2 * MB;
  // Remaining size is for the new alloc space.
  const size_t growth_limit = growth_limit_ - size;
  const size_t capacity = Capacity() - size;
  VLOG(heap) << "Begin " << reinterpret_cast<const void*>(begin_) << "\n"
             << "End " << reinterpret_cast<const void*>(end_) << "\n"
             << "Size " << size << "\n"
             << "GrowthLimit " << growth_limit_ << "\n"
             << "Capacity " << Capacity();
  SetGrowthLimit(RoundUp(size, kPageSize));
  SetFootprintLimit(RoundUp(size, kPageSize));
  // FIXME: Do we need reference counted pointers here?
  // Make the two spaces share the same mark bitmaps since the bitmaps span both of the spaces.
  VLOG(heap) << "Creating new AllocSpace: ";
  VLOG(heap) << "Size " << GetMemMap()->Size();
  VLOG(heap) << "GrowthLimit " << PrettySize(growth_limit);
  VLOG(heap) << "Capacity " << PrettySize(capacity);
  UniquePtr<MemMap> mem_map(MemMap::MapAnonymous(alloc_space_name, End(), capacity, PROT_READ | PROT_WRITE));
  void* mspace = CreateMallocSpace(end_, starting_size, initial_size);
  // Protect memory beyond the initial size.
  byte* end = mem_map->Begin() + starting_size;
  if (capacity - initial_size > 0) {
    CHECK_MEMORY_CALL(mprotect, (end, capacity - initial_size, PROT_NONE), alloc_space_name);
  }
  DlMallocSpace* alloc_space =
      new DlMallocSpace(alloc_space_name, mem_map.release(), mspace, end_, end, growth_limit);
  live_bitmap_->SetHeapLimit(reinterpret_cast<uintptr_t>(End()));
  CHECK_EQ(live_bitmap_->HeapLimit(), reinterpret_cast<uintptr_t>(End()));
  mark_bitmap_->SetHeapLimit(reinterpret_cast<uintptr_t>(End()));
  CHECK_EQ(mark_bitmap_->HeapLimit(), reinterpret_cast<uintptr_t>(End()));
  VLOG(heap) << "zygote space creation done";
  return alloc_space;
}

mirror::Class* DlMallocSpace::FindRecentFreedObject(const mirror::Object* obj) {
  size_t pos = recent_free_pos_;
  // Start at the most recently freed object and work our way back since there may be duplicates
  // caused by dlmalloc reusing memory.
  if (kRecentFreeCount > 0) {
    for (size_t i = 0; i + 1 < kRecentFreeCount + 1; ++i) {
      pos = pos != 0 ? pos - 1 : kRecentFreeMask;
      if (recent_freed_objects_[pos].first == obj) {
        return recent_freed_objects_[pos].second;
      }
    }
  }
  return nullptr;
}

void DlMallocSpace::RegisterRecentFree(mirror::Object* ptr) {
  recent_freed_objects_[recent_free_pos_].first = ptr;
  recent_freed_objects_[recent_free_pos_].second = ptr->GetClass();
  recent_free_pos_ = (recent_free_pos_ + 1) & kRecentFreeMask;
}

size_t DlMallocSpace::Free(Thread* self, mirror::Object* ptr) {
  MutexLock mu(self, lock_);
  if (kDebugSpaces) {
    CHECK(ptr != NULL);
    CHECK(Contains(ptr)) << "Free (" << ptr << ") not in bounds of heap " << *this;
  }
  const size_t bytes_freed = InternalAllocationSize(ptr);
  num_bytes_allocated_ -= bytes_freed;
  --num_objects_allocated_;
  if (kRecentFreeCount > 0) {
    RegisterRecentFree(ptr);
  }
  mspace_free(mspace_, ptr);
  return bytes_freed;
}

size_t DlMallocSpace::FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs) {
  DCHECK(ptrs != NULL);

  // Don't need the lock to calculate the size of the freed pointers.
  size_t bytes_freed = 0;
  for (size_t i = 0; i < num_ptrs; i++) {
    mirror::Object* ptr = ptrs[i];
    const size_t look_ahead = 8;
    if (kPrefetchDuringDlMallocFreeList && i + look_ahead < num_ptrs) {
      // The head of chunk for the allocation is sizeof(size_t) behind the allocation.
      __builtin_prefetch(reinterpret_cast<char*>(ptrs[i + look_ahead]) - sizeof(size_t));
    }
    bytes_freed += InternalAllocationSize(ptr);
  }

  if (kRecentFreeCount > 0) {
    MutexLock mu(self, lock_);
    for (size_t i = 0; i < num_ptrs; i++) {
      RegisterRecentFree(ptrs[i]);
    }
  }

  if (kDebugSpaces) {
    size_t num_broken_ptrs = 0;
    for (size_t i = 0; i < num_ptrs; i++) {
      if (!Contains(ptrs[i])) {
        num_broken_ptrs++;
        LOG(ERROR) << "FreeList[" << i << "] (" << ptrs[i] << ") not in bounds of heap " << *this;
      } else {
        size_t size = mspace_usable_size(ptrs[i]);
        memset(ptrs[i], 0xEF, size);
      }
    }
    CHECK_EQ(num_broken_ptrs, 0u);
  }

  {
    MutexLock mu(self, lock_);
    num_bytes_allocated_ -= bytes_freed;
    num_objects_allocated_ -= num_ptrs;
    mspace_bulk_free(mspace_, reinterpret_cast<void**>(ptrs), num_ptrs);
    return bytes_freed;
  }
}

// Callback from dlmalloc when it needs to increase the footprint
extern "C" void* art_heap_morecore(void* mspace, intptr_t increment) {
  Heap* heap = Runtime::Current()->GetHeap();
  DCHECK_EQ(heap->GetAllocSpace()->GetMspace(), mspace);
  return heap->GetAllocSpace()->MoreCore(increment);
}

void* DlMallocSpace::MoreCore(intptr_t increment) {
  lock_.AssertHeld(Thread::Current());
  byte* original_end = end_;
  if (increment != 0) {
    VLOG(heap) << "DlMallocSpace::MoreCore " << PrettySize(increment);
    byte* new_end = original_end + increment;
    if (increment > 0) {
      // Should never be asked to increase the allocation beyond the capacity of the space. Enforced
      // by mspace_set_footprint_limit.
      CHECK_LE(new_end, Begin() + Capacity());
      CHECK_MEMORY_CALL(mprotect, (original_end, increment, PROT_READ | PROT_WRITE), GetName());
    } else {
      // Should never be asked for negative footprint (ie before begin)
      CHECK_GT(original_end + increment, Begin());
      // Advise we don't need the pages and protect them
      // TODO: by removing permissions to the pages we may be causing TLB shoot-down which can be
      // expensive (note the same isn't true for giving permissions to a page as the protected
      // page shouldn't be in a TLB). We should investigate performance impact of just
      // removing ignoring the memory protection change here and in Space::CreateAllocSpace. It's
      // likely just a useful debug feature.
      size_t size = -increment;
      CHECK_MEMORY_CALL(madvise, (new_end, size, MADV_DONTNEED), GetName());
      CHECK_MEMORY_CALL(mprotect, (new_end, size, PROT_NONE), GetName());
    }
    // Update end_
    end_ = new_end;
  }
  return original_end;
}

// Virtual functions can't get inlined.
inline size_t DlMallocSpace::InternalAllocationSize(const mirror::Object* obj) {
  return AllocationSizeNonvirtual(obj);
}

size_t DlMallocSpace::AllocationSize(const mirror::Object* obj) {
  return InternalAllocationSize(obj);
}

size_t DlMallocSpace::Trim() {
  MutexLock mu(Thread::Current(), lock_);
  // Trim to release memory at the end of the space.
  mspace_trim(mspace_, 0);
  // Visit space looking for page-sized holes to advise the kernel we don't need.
  size_t reclaimed = 0;
  mspace_inspect_all(mspace_, DlmallocMadviseCallback, &reclaimed);
  return reclaimed;
}

void DlMallocSpace::Walk(void(*callback)(void *start, void *end, size_t num_bytes, void* callback_arg),
                      void* arg) {
  MutexLock mu(Thread::Current(), lock_);
  mspace_inspect_all(mspace_, callback, arg);
  callback(NULL, NULL, 0, arg);  // Indicate end of a space.
}

size_t DlMallocSpace::GetFootprint() {
  MutexLock mu(Thread::Current(), lock_);
  return mspace_footprint(mspace_);
}

size_t DlMallocSpace::GetFootprintLimit() {
  MutexLock mu(Thread::Current(), lock_);
  return mspace_footprint_limit(mspace_);
}

void DlMallocSpace::SetFootprintLimit(size_t new_size) {
  MutexLock mu(Thread::Current(), lock_);
  VLOG(heap) << "DLMallocSpace::SetFootprintLimit " << PrettySize(new_size);
  // Compare against the actual footprint, rather than the Size(), because the heap may not have
  // grown all the way to the allowed size yet.
  size_t current_space_size = mspace_footprint(mspace_);
  if (new_size < current_space_size) {
    // Don't let the space grow any more.
    new_size = current_space_size;
  }
  mspace_set_footprint_limit(mspace_, new_size);
}

void DlMallocSpace::Dump(std::ostream& os) const {
  os << GetType()
      << " begin=" << reinterpret_cast<void*>(Begin())
      << ",end=" << reinterpret_cast<void*>(End())
      << ",size=" << PrettySize(Size()) << ",capacity=" << PrettySize(Capacity())
      << ",name=\"" << GetName() << "\"]";
}

}  // namespace space
}  // namespace gc
}  // namespace art
