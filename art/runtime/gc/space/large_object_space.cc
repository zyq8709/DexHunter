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

#include "large_object_space.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "UniquePtr.h"
#include "image.h"
#include "os.h"
#include "thread.h"
#include "utils.h"

namespace art {
namespace gc {
namespace space {

void LargeObjectSpace::SwapBitmaps() {
  live_objects_.swap(mark_objects_);
  // Swap names to get more descriptive diagnostics.
  std::string temp_name = live_objects_->GetName();
  live_objects_->SetName(mark_objects_->GetName());
  mark_objects_->SetName(temp_name);
}

LargeObjectSpace::LargeObjectSpace(const std::string& name)
    : DiscontinuousSpace(name, kGcRetentionPolicyAlwaysCollect),
      num_bytes_allocated_(0), num_objects_allocated_(0), total_bytes_allocated_(0),
      total_objects_allocated_(0) {
}


void LargeObjectSpace::CopyLiveToMarked() {
  mark_objects_->CopyFrom(*live_objects_.get());
}

LargeObjectMapSpace::LargeObjectMapSpace(const std::string& name)
    : LargeObjectSpace(name),
      lock_("large object map space lock", kAllocSpaceLock) {}

LargeObjectMapSpace* LargeObjectMapSpace::Create(const std::string& name) {
  return new LargeObjectMapSpace(name);
}

mirror::Object* LargeObjectMapSpace::Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated) {
  MemMap* mem_map = MemMap::MapAnonymous("large object space allocation", NULL, num_bytes,
                                         PROT_READ | PROT_WRITE);
  if (mem_map == NULL) {
    return NULL;
  }
  MutexLock mu(self, lock_);
  mirror::Object* obj = reinterpret_cast<mirror::Object*>(mem_map->Begin());
  large_objects_.push_back(obj);
  mem_maps_.Put(obj, mem_map);
  size_t allocation_size = mem_map->Size();
  DCHECK(bytes_allocated != NULL);
  *bytes_allocated = allocation_size;
  num_bytes_allocated_ += allocation_size;
  total_bytes_allocated_ += allocation_size;
  ++num_objects_allocated_;
  ++total_objects_allocated_;
  return obj;
}

size_t LargeObjectMapSpace::Free(Thread* self, mirror::Object* ptr) {
  MutexLock mu(self, lock_);
  MemMaps::iterator found = mem_maps_.find(ptr);
  CHECK(found != mem_maps_.end()) << "Attempted to free large object which was not live";
  DCHECK_GE(num_bytes_allocated_, found->second->Size());
  size_t allocation_size = found->second->Size();
  num_bytes_allocated_ -= allocation_size;
  --num_objects_allocated_;
  delete found->second;
  mem_maps_.erase(found);
  return allocation_size;
}

size_t LargeObjectMapSpace::AllocationSize(const mirror::Object* obj) {
  MutexLock mu(Thread::Current(), lock_);
  MemMaps::iterator found = mem_maps_.find(const_cast<mirror::Object*>(obj));
  CHECK(found != mem_maps_.end()) << "Attempted to get size of a large object which is not live";
  return found->second->Size();
}

size_t LargeObjectSpace::FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs) {
  size_t total = 0;
  for (size_t i = 0; i < num_ptrs; ++i) {
    if (kDebugSpaces) {
      CHECK(Contains(ptrs[i]));
    }
    total += Free(self, ptrs[i]);
  }
  return total;
}

void LargeObjectMapSpace::Walk(DlMallocSpace::WalkCallback callback, void* arg) {
  MutexLock mu(Thread::Current(), lock_);
  for (MemMaps::iterator it = mem_maps_.begin(); it != mem_maps_.end(); ++it) {
    MemMap* mem_map = it->second;
    callback(mem_map->Begin(), mem_map->End(), mem_map->Size(), arg);
    callback(NULL, NULL, 0, arg);
  }
}

bool LargeObjectMapSpace::Contains(const mirror::Object* obj) const {
  Thread* self = Thread::Current();
  if (lock_.IsExclusiveHeld(self)) {
    // We hold lock_ so do the check.
    return mem_maps_.find(const_cast<mirror::Object*>(obj)) != mem_maps_.end();
  } else {
    MutexLock mu(self, lock_);
    return mem_maps_.find(const_cast<mirror::Object*>(obj)) != mem_maps_.end();
  }
}

FreeListSpace* FreeListSpace::Create(const std::string& name, byte* requested_begin, size_t size) {
  CHECK_EQ(size % kAlignment, 0U);
  MemMap* mem_map = MemMap::MapAnonymous(name.c_str(), requested_begin, size,
                                         PROT_READ | PROT_WRITE);
  CHECK(mem_map != NULL) << "Failed to allocate large object space mem map";
  return new FreeListSpace(name, mem_map, mem_map->Begin(), mem_map->End());
}

FreeListSpace::FreeListSpace(const std::string& name, MemMap* mem_map, byte* begin, byte* end)
    : LargeObjectSpace(name),
      begin_(begin),
      end_(end),
      mem_map_(mem_map),
      lock_("free list space lock", kAllocSpaceLock) {
  free_end_ = end - begin;
}

FreeListSpace::~FreeListSpace() {}

void FreeListSpace::Walk(DlMallocSpace::WalkCallback callback, void* arg) {
  MutexLock mu(Thread::Current(), lock_);
  uintptr_t free_end_start = reinterpret_cast<uintptr_t>(end_) - free_end_;
  AllocationHeader* cur_header = reinterpret_cast<AllocationHeader*>(Begin());
  while (reinterpret_cast<uintptr_t>(cur_header) < free_end_start) {
    cur_header = cur_header->GetNextNonFree();
    size_t alloc_size = cur_header->AllocationSize();
    byte* byte_start = reinterpret_cast<byte*>(cur_header->GetObjectAddress());
    byte* byte_end = byte_start + alloc_size - sizeof(AllocationHeader);
    callback(byte_start, byte_end, alloc_size, arg);
    callback(NULL, NULL, 0, arg);
    cur_header = reinterpret_cast<AllocationHeader*>(byte_end);
  }
}

void FreeListSpace::RemoveFreePrev(AllocationHeader* header) {
  CHECK(!header->IsFree());
  CHECK_GT(header->GetPrevFree(), size_t(0));
  FreeBlocks::iterator found = free_blocks_.lower_bound(header);
  CHECK(found != free_blocks_.end());
  CHECK_EQ(*found, header);
  free_blocks_.erase(found);
}

FreeListSpace::AllocationHeader* FreeListSpace::GetAllocationHeader(const mirror::Object* obj) {
  DCHECK(Contains(obj));
  return reinterpret_cast<AllocationHeader*>(reinterpret_cast<uintptr_t>(obj) -
      sizeof(AllocationHeader));
}

FreeListSpace::AllocationHeader* FreeListSpace::AllocationHeader::GetNextNonFree() {
  // We know that there has to be at least one object after us or else we would have
  // coalesced with the free end region. May be worth investigating a better way to do this
  // as it may be expensive for large allocations.
  for (uintptr_t pos = reinterpret_cast<uintptr_t>(this);; pos += kAlignment) {
    AllocationHeader* cur = reinterpret_cast<AllocationHeader*>(pos);
    if (!cur->IsFree()) return cur;
  }
}

size_t FreeListSpace::Free(Thread* self, mirror::Object* obj) {
  MutexLock mu(self, lock_);
  DCHECK(Contains(obj));
  AllocationHeader* header = GetAllocationHeader(obj);
  CHECK(IsAligned<kAlignment>(header));
  size_t allocation_size = header->AllocationSize();
  DCHECK_GT(allocation_size, size_t(0));
  DCHECK(IsAligned<kAlignment>(allocation_size));
  // Look at the next chunk.
  AllocationHeader* next_header = header->GetNextAllocationHeader();
  // Calculate the start of the end free block.
  uintptr_t free_end_start = reinterpret_cast<uintptr_t>(end_) - free_end_;
  size_t header_prev_free = header->GetPrevFree();
  size_t new_free_size = allocation_size;
  if (header_prev_free) {
    new_free_size += header_prev_free;
    RemoveFreePrev(header);
  }
  if (reinterpret_cast<uintptr_t>(next_header) >= free_end_start) {
    // Easy case, the next chunk is the end free region.
    CHECK_EQ(reinterpret_cast<uintptr_t>(next_header), free_end_start);
    free_end_ += new_free_size;
  } else {
    AllocationHeader* new_free_header;
    DCHECK(IsAligned<kAlignment>(next_header));
    if (next_header->IsFree()) {
      // Find the next chunk by reading each page until we hit one with non-zero chunk.
      AllocationHeader* next_next_header = next_header->GetNextNonFree();
      DCHECK(IsAligned<kAlignment>(next_next_header));
      DCHECK(IsAligned<kAlignment>(next_next_header->AllocationSize()));
      RemoveFreePrev(next_next_header);
      new_free_header = next_next_header;
      new_free_size += next_next_header->GetPrevFree();
    } else {
      new_free_header = next_header;
    }
    new_free_header->prev_free_ = new_free_size;
    free_blocks_.insert(new_free_header);
  }
  --num_objects_allocated_;
  DCHECK_LE(allocation_size, num_bytes_allocated_);
  num_bytes_allocated_ -= allocation_size;
  madvise(header, allocation_size, MADV_DONTNEED);
  if (kIsDebugBuild) {
    // Can't disallow reads since we use them to find next chunks during coalescing.
    mprotect(header, allocation_size, PROT_READ);
  }
  return allocation_size;
}

bool FreeListSpace::Contains(const mirror::Object* obj) const {
  return mem_map_->HasAddress(obj);
}

size_t FreeListSpace::AllocationSize(const mirror::Object* obj) {
  AllocationHeader* header = GetAllocationHeader(obj);
  DCHECK(Contains(obj));
  DCHECK(!header->IsFree());
  return header->AllocationSize();
}

mirror::Object* FreeListSpace::Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated) {
  MutexLock mu(self, lock_);
  size_t allocation_size = RoundUp(num_bytes + sizeof(AllocationHeader), kAlignment);
  AllocationHeader temp;
  temp.SetPrevFree(allocation_size);
  temp.SetAllocationSize(0);
  AllocationHeader* new_header;
  // Find the smallest chunk at least num_bytes in size.
  FreeBlocks::iterator found = free_blocks_.lower_bound(&temp);
  if (found != free_blocks_.end()) {
    AllocationHeader* header = *found;
    free_blocks_.erase(found);

    // Fit our object in the previous free header space.
    new_header = header->GetPrevFreeAllocationHeader();

    // Remove the newly allocated block from the header and update the prev_free_.
    header->prev_free_ -= allocation_size;
    if (header->prev_free_ > 0) {
      // If there is remaining space, insert back into the free set.
      free_blocks_.insert(header);
    }
  } else {
    // Try to steal some memory from the free space at the end of the space.
    if (LIKELY(free_end_ >= allocation_size)) {
      // Fit our object at the start of the end free block.
      new_header = reinterpret_cast<AllocationHeader*>(end_ - free_end_);
      free_end_ -= allocation_size;
    } else {
      return NULL;
    }
  }

  DCHECK(bytes_allocated != NULL);
  *bytes_allocated = allocation_size;

  // Need to do these inside of the lock.
  ++num_objects_allocated_;
  ++total_objects_allocated_;
  num_bytes_allocated_ += allocation_size;
  total_bytes_allocated_ += allocation_size;

  // We always put our object at the start of the free block, there can not be another free block
  // before it.
  if (kIsDebugBuild) {
    mprotect(new_header, allocation_size, PROT_READ | PROT_WRITE);
  }
  new_header->SetPrevFree(0);
  new_header->SetAllocationSize(allocation_size);
  return new_header->GetObjectAddress();
}

void FreeListSpace::Dump(std::ostream& os) const {
  MutexLock mu(Thread::Current(), const_cast<Mutex&>(lock_));
  os << GetName() << " -"
     << " begin: " << reinterpret_cast<void*>(Begin())
     << " end: " << reinterpret_cast<void*>(End()) << "\n";
  uintptr_t free_end_start = reinterpret_cast<uintptr_t>(end_) - free_end_;
  AllocationHeader* cur_header = reinterpret_cast<AllocationHeader*>(Begin());
  while (reinterpret_cast<uintptr_t>(cur_header) < free_end_start) {
    byte* free_start = reinterpret_cast<byte*>(cur_header);
    cur_header = cur_header->GetNextNonFree();
    byte* free_end = reinterpret_cast<byte*>(cur_header);
    if (free_start != free_end) {
      os << "Free block at address: " << reinterpret_cast<const void*>(free_start)
         << " of length " << free_end - free_start << " bytes\n";
    }
    size_t alloc_size = cur_header->AllocationSize();
    byte* byte_start = reinterpret_cast<byte*>(cur_header->GetObjectAddress());
    byte* byte_end = byte_start + alloc_size - sizeof(AllocationHeader);
    os << "Large object at address: " << reinterpret_cast<const void*>(free_start)
       << " of length " << byte_end - byte_start << " bytes\n";
    cur_header = reinterpret_cast<AllocationHeader*>(byte_end);
  }
  if (free_end_) {
    os << "Free block at address: " << reinterpret_cast<const void*>(free_end_start)
       << " of length " << free_end_ << " bytes\n";
  }
}

}  // namespace space
}  // namespace gc
}  // namespace art
