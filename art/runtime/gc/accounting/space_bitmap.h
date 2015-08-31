/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_H_
#define ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_H_

#include "locks.h"
#include "gc_allocator.h"
#include "globals.h"
#include "mem_map.h"
#include "UniquePtr.h"

#include <limits.h>
#include <set>
#include <stdint.h>
#include <vector>

namespace art {

namespace mirror {
  class Object;
}  // namespace mirror

namespace gc {
namespace accounting {

class SpaceBitmap {
 public:
  // Alignment of objects within spaces.
  static const size_t kAlignment = 8;

  typedef void Callback(mirror::Object* obj, void* arg);

  typedef void ScanCallback(mirror::Object* obj, void* finger, void* arg);

  typedef void SweepCallback(size_t ptr_count, mirror::Object** ptrs, void* arg);

  // Initialize a space bitmap so that it points to a bitmap large enough to cover a heap at
  // heap_begin of heap_capacity bytes, where objects are guaranteed to be kAlignment-aligned.
  static SpaceBitmap* Create(const std::string& name, byte* heap_begin, size_t heap_capacity);

  // Initialize a space bitmap using the provided mem_map as the live bits. Takes ownership of the
  // mem map. The address range covered starts at heap_begin and is of size equal to heap_capacity.
  // Objects are kAlignement-aligned.
  static SpaceBitmap* CreateFromMemMap(const std::string& name, MemMap* mem_map,
                                       byte* heap_begin, size_t heap_capacity);

  ~SpaceBitmap();

  // <offset> is the difference from .base to a pointer address.
  // <index> is the index of .bits that contains the bit representing
  //         <offset>.
  static size_t OffsetToIndex(size_t offset) {
    return offset / kAlignment / kBitsPerWord;
  }

  static uintptr_t IndexToOffset(size_t index) {
    return static_cast<uintptr_t>(index * kAlignment * kBitsPerWord);
  }

  // Pack the bits in backwards so they come out in address order when using CLZ.
  static word OffsetToMask(uintptr_t offset_) {
    return static_cast<uintptr_t>(kWordHighBitMask) >> ((offset_ / kAlignment) % kBitsPerWord);
  }

  inline bool Set(const mirror::Object* obj) {
    return Modify(obj, true);
  }

  inline bool Clear(const mirror::Object* obj) {
    return Modify(obj, false);
  }

  // Returns true if the object was previously marked.
  bool AtomicTestAndSet(const mirror::Object* obj);

  // Fill the bitmap with zeroes.  Returns the bitmap's memory to the system as a side-effect.
  void Clear();

  bool Test(const mirror::Object* obj) const;

  // Return true iff <obj> is within the range of pointers that this bitmap could potentially cover,
  // even if a bit has not been set for it.
  bool HasAddress(const void* obj) const {
    // If obj < heap_begin_ then offset underflows to some very large value past the end of the
    // bitmap.
    const uintptr_t offset = reinterpret_cast<uintptr_t>(obj) - heap_begin_;
    const size_t index = OffsetToIndex(offset);
    return index < bitmap_size_ / kWordSize;
  }

  void VisitRange(uintptr_t base, uintptr_t max, Callback* visitor, void* arg) const;

  class ClearVisitor {
   public:
    explicit ClearVisitor(SpaceBitmap* const bitmap)
        : bitmap_(bitmap) {
    }

    void operator()(mirror::Object* obj) const {
      bitmap_->Clear(obj);
    }
   private:
    SpaceBitmap* const bitmap_;
  };

  template <typename Visitor>
  void VisitRange(uintptr_t visit_begin, uintptr_t visit_end, const Visitor& visitor) const {
    for (; visit_begin < visit_end; visit_begin += kAlignment) {
      visitor(reinterpret_cast<mirror::Object*>(visit_begin));
    }
  }

  template <typename Visitor>
  void VisitMarkedRange(uintptr_t visit_begin, uintptr_t visit_end, const Visitor& visitor) const
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Walk(Callback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  void InOrderWalk(Callback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  static void SweepWalk(const SpaceBitmap& live, const SpaceBitmap& mark, uintptr_t base,
                        uintptr_t max, SweepCallback* thunk, void* arg);

  void CopyFrom(SpaceBitmap* source_bitmap);

  // Starting address of our internal storage.
  word* Begin() {
    return bitmap_begin_;
  }

  // Size of our internal storage
  size_t Size() const {
    return bitmap_size_;
  }

  // Size in bytes of the memory that the bitmaps spans.
  size_t HeapSize() const {
    return IndexToOffset(Size() / kWordSize);
  }

  uintptr_t HeapBegin() const {
    return heap_begin_;
  }

  // The maximum address which the bitmap can span. (HeapBegin() <= object < HeapLimit()).
  uintptr_t HeapLimit() const {
    return HeapBegin() + static_cast<uintptr_t>(HeapSize());
  }

  // Set the max address which can covered by the bitmap.
  void SetHeapLimit(uintptr_t new_end);

  std::string GetName() const;
  void SetName(const std::string& name);

  std::string Dump() const;

  const void* GetObjectWordAddress(const mirror::Object* obj) const {
    uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
    const uintptr_t offset = addr - heap_begin_;
    const size_t index = OffsetToIndex(offset);
    return &bitmap_begin_[index];
  }

 private:
  // TODO: heap_end_ is initialized so that the heap bitmap is empty, this doesn't require the -1,
  // however, we document that this is expected on heap_end_
  SpaceBitmap(const std::string& name, MemMap* mem_map, word* bitmap_begin, size_t bitmap_size,
              const void* heap_begin)
      : mem_map_(mem_map), bitmap_begin_(bitmap_begin), bitmap_size_(bitmap_size),
        heap_begin_(reinterpret_cast<uintptr_t>(heap_begin)),
        name_(name) {}

  bool Modify(const mirror::Object* obj, bool do_set);

  // Backing storage for bitmap.
  UniquePtr<MemMap> mem_map_;

  // This bitmap itself, word sized for efficiency in scanning.
  word* const bitmap_begin_;

  // Size of this bitmap.
  size_t bitmap_size_;

  // The base address of the heap, which corresponds to the word containing the first bit in the
  // bitmap.
  const uintptr_t heap_begin_;

  // Name of this bitmap.
  std::string name_;
};

// Like a bitmap except it keeps track of objects using sets.
class SpaceSetMap {
 public:
  typedef std::set<
      const mirror::Object*, std::less<const mirror::Object*>,
      GCAllocator<const mirror::Object*> > Objects;

  bool IsEmpty() const {
    return contained_.empty();
  }

  inline void Set(const mirror::Object* obj) {
    contained_.insert(obj);
  }

  inline void Clear(const mirror::Object* obj) {
    Objects::iterator found = contained_.find(obj);
    if (found != contained_.end()) {
      contained_.erase(found);
    }
  }

  void Clear() {
    contained_.clear();
  }

  inline bool Test(const mirror::Object* obj) const {
    return contained_.find(obj) != contained_.end();
  }

  std::string GetName() const;
  void SetName(const std::string& name);

  void Walk(SpaceBitmap::Callback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(GlobalSynchronization::heap_bitmap_lock_);

  void CopyFrom(const SpaceSetMap& space_set);

  template <typename Visitor>
  void Visit(const Visitor& visitor) NO_THREAD_SAFETY_ANALYSIS {
    for (Objects::iterator it = contained_.begin(); it != contained_.end(); ++it) {
      visitor(*it);
    }
  }

  explicit SpaceSetMap(const std::string& name) : name_(name) {}
  ~SpaceSetMap() {}

  Objects& GetObjects() {
    return contained_;
  }

 private:
  std::string name_;
  Objects contained_;
};

std::ostream& operator << (std::ostream& stream, const SpaceBitmap& bitmap);

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_H_
