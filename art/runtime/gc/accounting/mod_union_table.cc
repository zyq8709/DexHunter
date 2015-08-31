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

#include "mod_union_table.h"

#include "base/stl_util.h"
#include "card_table-inl.h"
#include "heap_bitmap.h"
#include "gc/collector/mark_sweep-inl.h"
#include "gc/heap.h"
#include "gc/space/space.h"
#include "mirror/art_field-inl.h"
#include "mirror/object-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "space_bitmap-inl.h"
#include "thread.h"
#include "UniquePtr.h"

using ::art::mirror::Object;

namespace art {
namespace gc {
namespace accounting {

class ModUnionClearCardSetVisitor {
 public:
  explicit ModUnionClearCardSetVisitor(ModUnionTable::CardSet* const cleared_cards)
    : cleared_cards_(cleared_cards) {
  }

  inline void operator()(byte* card, byte expected_value, byte new_value) const {
    if (expected_value == CardTable::kCardDirty) {
      cleared_cards_->insert(card);
    }
  }

 private:
  ModUnionTable::CardSet* const cleared_cards_;
};

class ModUnionClearCardVisitor {
 public:
  explicit ModUnionClearCardVisitor(std::vector<byte*>* cleared_cards)
    : cleared_cards_(cleared_cards) {
  }

  void operator()(byte* card, byte expected_card, byte new_card) const {
    if (expected_card == CardTable::kCardDirty) {
      cleared_cards_->push_back(card);
    }
  }
 private:
  std::vector<byte*>* const cleared_cards_;
};

class ModUnionScanImageRootVisitor {
 public:
  explicit ModUnionScanImageRootVisitor(collector::MarkSweep* const mark_sweep)
      : mark_sweep_(mark_sweep) {}

  void operator()(const Object* root) const
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(root != NULL);
    mark_sweep_->ScanRoot(root);
  }

 private:
  collector::MarkSweep* const mark_sweep_;
};

void ModUnionTableReferenceCache::ClearCards(space::ContinuousSpace* space) {
  CardTable* card_table = GetHeap()->GetCardTable();
  ModUnionClearCardSetVisitor visitor(&cleared_cards_);
  // Clear dirty cards in the this space and update the corresponding mod-union bits.
  card_table->ModifyCardsAtomic(space->Begin(), space->End(), AgeCardVisitor(), visitor);
}

class AddToReferenceArrayVisitor {
 public:
  explicit AddToReferenceArrayVisitor(ModUnionTableReferenceCache* mod_union_table,
                                      std::vector<const Object*>* references)
    : mod_union_table_(mod_union_table),
      references_(references) {
  }

  // Extra parameters are required since we use this same visitor signature for checking objects.
  void operator()(const Object* obj, const Object* ref, const MemberOffset& /* offset */,
                  bool /* is_static */) const {
    // Only add the reference if it is non null and fits our criteria.
    if (ref != NULL && mod_union_table_->AddReference(obj, ref)) {
      references_->push_back(ref);
    }
  }

 private:
  ModUnionTableReferenceCache* const mod_union_table_;
  std::vector<const Object*>* const references_;
};

class ModUnionReferenceVisitor {
 public:
  explicit ModUnionReferenceVisitor(ModUnionTableReferenceCache* const mod_union_table,
                                    std::vector<const Object*>* references)
    : mod_union_table_(mod_union_table),
      references_(references) {
  }

  void operator()(const Object* obj) const
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_) {
    DCHECK(obj != NULL);
    // We don't have an early exit since we use the visitor pattern, an early
    // exit should significantly speed this up.
    AddToReferenceArrayVisitor visitor(mod_union_table_, references_);
    collector::MarkSweep::VisitObjectReferences(obj, visitor);
  }
 private:
  ModUnionTableReferenceCache* const mod_union_table_;
  std::vector<const Object*>* const references_;
};

class CheckReferenceVisitor {
 public:
  explicit CheckReferenceVisitor(ModUnionTableReferenceCache* mod_union_table,
                                 const std::set<const Object*>& references)
    : mod_union_table_(mod_union_table),
      references_(references) {
  }

  // Extra parameters are required since we use this same visitor signature for checking objects.
  // TODO: Fixme when anotatalysis works with visitors.
  void operator()(const Object* obj, const Object* ref, const MemberOffset& /* offset */,
                  bool /* is_static */) const
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_) {
    Heap* heap = mod_union_table_->GetHeap();
    if (ref != NULL && mod_union_table_->AddReference(obj, ref) &&
        references_.find(ref) == references_.end()) {
      space::ContinuousSpace* from_space = heap->FindContinuousSpaceFromObject(obj, false);
      space::ContinuousSpace* to_space = heap->FindContinuousSpaceFromObject(ref, false);
      LOG(INFO) << "Object " << reinterpret_cast<const void*>(obj) << "(" << PrettyTypeOf(obj) << ")"
                << "References " << reinterpret_cast<const void*>(ref)
                << "(" << PrettyTypeOf(ref) << ") without being in mod-union table";
      LOG(INFO) << "FromSpace " << from_space->GetName() << " type " << from_space->GetGcRetentionPolicy();
      LOG(INFO) << "ToSpace " << to_space->GetName() << " type " << to_space->GetGcRetentionPolicy();
      mod_union_table_->GetHeap()->DumpSpaces();
      LOG(FATAL) << "FATAL ERROR";
    }
  }

 private:
  ModUnionTableReferenceCache* const mod_union_table_;
  const std::set<const Object*>& references_;
};

class ModUnionCheckReferences {
 public:
  explicit ModUnionCheckReferences(ModUnionTableReferenceCache* mod_union_table,
                                   const std::set<const Object*>& references)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      : mod_union_table_(mod_union_table), references_(references) {
  }

  void operator()(const Object* obj) const NO_THREAD_SAFETY_ANALYSIS {
    Locks::heap_bitmap_lock_->AssertSharedHeld(Thread::Current());
    DCHECK(obj != NULL);
    CheckReferenceVisitor visitor(mod_union_table_, references_);
    collector::MarkSweep::VisitObjectReferences(obj, visitor);
  }

 private:
  ModUnionTableReferenceCache* const mod_union_table_;
  const std::set<const Object*>& references_;
};

void ModUnionTableReferenceCache::Verify() {
  // Start by checking that everything in the mod union table is marked.
  Heap* heap = GetHeap();
  for (const std::pair<const byte*, std::vector<const Object*> >& it : references_) {
    for (const Object* ref : it.second) {
      CHECK(heap->IsLiveObjectLocked(ref));
    }
  }

  // Check the references of each clean card which is also in the mod union table.
  CardTable* card_table = heap->GetCardTable();
  for (const std::pair<const byte*, std::vector<const Object*> > & it : references_) {
    const byte* card = it.first;
    if (*card == CardTable::kCardClean) {
      std::set<const Object*> reference_set(it.second.begin(), it.second.end());
      ModUnionCheckReferences visitor(this, reference_set);
      uintptr_t start = reinterpret_cast<uintptr_t>(card_table->AddrFromCard(card));
      uintptr_t end = start + CardTable::kCardSize;
      auto* space = heap->FindContinuousSpaceFromObject(reinterpret_cast<Object*>(start), false);
      DCHECK(space != nullptr);
      SpaceBitmap* live_bitmap = space->GetLiveBitmap();
      live_bitmap->VisitMarkedRange(start, end, visitor);
    }
  }
}

void ModUnionTableReferenceCache::Dump(std::ostream& os) {
  CardTable* card_table = heap_->GetCardTable();
  os << "ModUnionTable cleared cards: [";
  for (byte* card_addr : cleared_cards_) {
    uintptr_t start = reinterpret_cast<uintptr_t>(card_table->AddrFromCard(card_addr));
    uintptr_t end = start + CardTable::kCardSize;
    os << reinterpret_cast<void*>(start) << "-" << reinterpret_cast<void*>(end) << ",";
  }
  os << "]\nModUnionTable references: [";
  for (const std::pair<const byte*, std::vector<const Object*> >& it : references_) {
    const byte* card_addr = it.first;
    uintptr_t start = reinterpret_cast<uintptr_t>(card_table->AddrFromCard(card_addr));
    uintptr_t end = start + CardTable::kCardSize;
    os << reinterpret_cast<void*>(start) << "-" << reinterpret_cast<void*>(end) << "->{";
    for (const mirror::Object* ref : it.second) {
      os << reinterpret_cast<const void*>(ref) << ",";
    }
    os << "},";
  }
}

void ModUnionTableReferenceCache::Update() {
  Heap* heap = GetHeap();
  CardTable* card_table = heap->GetCardTable();

  std::vector<const Object*> cards_references;
  ModUnionReferenceVisitor visitor(this, &cards_references);

  for (const auto& card : cleared_cards_) {
    // Clear and re-compute alloc space references associated with this card.
    cards_references.clear();
    uintptr_t start = reinterpret_cast<uintptr_t>(card_table->AddrFromCard(card));
    uintptr_t end = start + CardTable::kCardSize;
    auto* space = heap->FindContinuousSpaceFromObject(reinterpret_cast<Object*>(start), false);
    DCHECK(space != nullptr);
    SpaceBitmap* live_bitmap = space->GetLiveBitmap();
    live_bitmap->VisitMarkedRange(start, end, visitor);

    // Update the corresponding references for the card.
    auto found = references_.find(card);
    if (found == references_.end()) {
      if (cards_references.empty()) {
        // No reason to add empty array.
        continue;
      }
      references_.Put(card, cards_references);
    } else {
      found->second = cards_references;
    }
  }
  cleared_cards_.clear();
}

void ModUnionTableReferenceCache::MarkReferences(collector::MarkSweep* mark_sweep) {
  size_t count = 0;

  for (const auto& ref : references_) {
    for (const auto& obj : ref.second) {
      mark_sweep->MarkRoot(obj);
      ++count;
    }
  }
  if (VLOG_IS_ON(heap)) {
    VLOG(gc) << "Marked " << count << " references in mod union table";
  }
}

void ModUnionTableCardCache::ClearCards(space::ContinuousSpace* space) {
  CardTable* card_table = GetHeap()->GetCardTable();
  ModUnionClearCardSetVisitor visitor(&cleared_cards_);
  // Clear dirty cards in the this space and update the corresponding mod-union bits.
  card_table->ModifyCardsAtomic(space->Begin(), space->End(), AgeCardVisitor(), visitor);
}

// Mark all references to the alloc space(s).
void ModUnionTableCardCache::MarkReferences(collector::MarkSweep* mark_sweep) {
  CardTable* card_table = heap_->GetCardTable();
  ModUnionScanImageRootVisitor visitor(mark_sweep);
  space::ContinuousSpace* space = nullptr;
  SpaceBitmap* bitmap = nullptr;
  for (const byte* card_addr : cleared_cards_) {
    auto start = reinterpret_cast<uintptr_t>(card_table->AddrFromCard(card_addr));
    auto end = start + CardTable::kCardSize;
    auto obj_start = reinterpret_cast<Object*>(start);
    if (UNLIKELY(space == nullptr || !space->Contains(obj_start))) {
      space = heap_->FindContinuousSpaceFromObject(obj_start, false);
      DCHECK(space != nullptr);
      bitmap = space->GetLiveBitmap();
      DCHECK(bitmap != nullptr);
    }
    bitmap->VisitMarkedRange(start, end, visitor);
  }
}

void ModUnionTableCardCache::Dump(std::ostream& os) {
  CardTable* card_table = heap_->GetCardTable();
  os << "ModUnionTable dirty cards: [";
  for (const byte* card_addr : cleared_cards_) {
    auto start = reinterpret_cast<uintptr_t>(card_table->AddrFromCard(card_addr));
    auto end = start + CardTable::kCardSize;
    os << reinterpret_cast<void*>(start) << "-" << reinterpret_cast<void*>(end) << ",";
  }
  os << "]";
}

}  // namespace accounting
}  // namespace gc
}  // namespace art
