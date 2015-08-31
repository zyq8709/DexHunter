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

#ifndef ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_H_
#define ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_H_

#include "gc_allocator.h"
#include "globals.h"
#include "safe_map.h"

#include <set>
#include <vector>

namespace art {
namespace mirror {
  class Object;
}  // namespace mirror

namespace gc {

namespace collector {
  class MarkSweep;
}  // namespace collector
namespace space {
  class ContinuousSpace;
  class Space;
}  // namespace space

class Heap;

namespace accounting {

class SpaceBitmap;
class HeapBitmap;

// The mod-union table is the union of modified cards. It is used to allow the card table to be
// cleared between GC phases, reducing the number of dirty cards that need to be scanned.
class ModUnionTable {
 public:
  typedef std::set<byte*, std::less<byte*>, GCAllocator<byte*> > CardSet;

  explicit ModUnionTable(Heap* heap) : heap_(heap) {}

  virtual ~ModUnionTable() {}

  // Clear cards which map to a memory range of a space. This doesn't immediately update the
  // mod-union table, as updating the mod-union table may have an associated cost, such as
  // determining references to track.
  virtual void ClearCards(space::ContinuousSpace* space) = 0;

  // Update the mod-union table using data stored by ClearCards. There may be multiple ClearCards
  // before a call to update, for example, back-to-back sticky GCs.
  virtual void Update() = 0;

  // Mark the bitmaps for all references which are stored in the mod-union table.
  virtual void MarkReferences(collector::MarkSweep* mark_sweep) = 0;

  // Verification, sanity checks that we don't have clean cards which conflict with out cached data
  // for said cards. Exclusive lock is required since verify sometimes uses
  // SpaceBitmap::VisitMarkedRange and VisitMarkedRange can't know if the callback will modify the
  // bitmap or not.
  virtual void Verify() EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_) = 0;

  virtual void Dump(std::ostream& os) = 0;

  Heap* GetHeap() const {
    return heap_;
  }

 protected:
  Heap* const heap_;
};

// Reference caching implementation. Caches references pointing to alloc space(s) for each card.
class ModUnionTableReferenceCache : public ModUnionTable {
 public:
  explicit ModUnionTableReferenceCache(Heap* heap) : ModUnionTable(heap) {}
  virtual ~ModUnionTableReferenceCache() {}

  // Clear and store cards for a space.
  void ClearCards(space::ContinuousSpace* space);

  // Update table based on cleared cards.
  void Update()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Mark all references to the alloc space(s).
  void MarkReferences(collector::MarkSweep* mark_sweep)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Exclusive lock is required since verify uses SpaceBitmap::VisitMarkedRange and
  // VisitMarkedRange can't know if the callback will modify the bitmap or not.
  void Verify() EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Function that tells whether or not to add a reference to the table.
  virtual bool AddReference(const mirror::Object* obj, const mirror::Object* ref) = 0;

  void Dump(std::ostream& os);

 protected:
  // Cleared card array, used to update the mod-union table.
  ModUnionTable::CardSet cleared_cards_;

  // Maps from dirty cards to their corresponding alloc space references.
  SafeMap<const byte*, std::vector<const mirror::Object*>, std::less<const byte*>,
    GCAllocator<std::pair<const byte*, std::vector<const mirror::Object*> > > > references_;
};

// Card caching implementation. Keeps track of which cards we cleared and only this information.
class ModUnionTableCardCache : public ModUnionTable {
 public:
  explicit ModUnionTableCardCache(Heap* heap) : ModUnionTable(heap) {}
  virtual ~ModUnionTableCardCache() {}

  // Clear and store cards for a space.
  void ClearCards(space::ContinuousSpace* space);

  // Nothing to update as all dirty cards were placed into cleared cards during clearing.
  void Update() {}

  // Mark all references to the alloc space(s).
  void MarkReferences(collector::MarkSweep* mark_sweep)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Nothing to verify.
  void Verify() {}

  void Dump(std::ostream& os);

 protected:
  // Cleared card array, used to update the mod-union table.
  CardSet cleared_cards_;
};

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_H_
