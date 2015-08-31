/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include "indirect_reference_table.h"
#include "jni_internal.h"
#include "reference_table.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "thread.h"
#include "utils.h"

#include <cstdlib>

namespace art {

static void AbortMaybe() {
  // If -Xcheck:jni is on, it'll give a more detailed error before aborting.
  if (!Runtime::Current()->GetJavaVM()->check_jni) {
    // Otherwise, we want to abort rather than hand back a bad reference.
    LOG(FATAL) << "JNI ERROR (app bug): see above.";
  }
}

IndirectReferenceTable::IndirectReferenceTable(size_t initialCount,
                                               size_t maxCount, IndirectRefKind desiredKind) {
  CHECK_GT(initialCount, 0U);
  CHECK_LE(initialCount, maxCount);
  CHECK_NE(desiredKind, kSirtOrInvalid);

  table_ = reinterpret_cast<const mirror::Object**>(malloc(initialCount * sizeof(const mirror::Object*)));
  CHECK(table_ != NULL);
  memset(table_, 0xd1, initialCount * sizeof(const mirror::Object*));

  slot_data_ = reinterpret_cast<IndirectRefSlot*>(calloc(initialCount, sizeof(IndirectRefSlot)));
  CHECK(slot_data_ != NULL);

  segment_state_.all = IRT_FIRST_SEGMENT;
  alloc_entries_ = initialCount;
  max_entries_ = maxCount;
  kind_ = desiredKind;
}

IndirectReferenceTable::~IndirectReferenceTable() {
  free(table_);
  free(slot_data_);
  table_ = NULL;
  slot_data_ = NULL;
  alloc_entries_ = max_entries_ = -1;
}

// Make sure that the entry at "idx" is correctly paired with "iref".
bool IndirectReferenceTable::CheckEntry(const char* what, IndirectRef iref, int idx) const {
  const mirror::Object* obj = table_[idx];
  IndirectRef checkRef = ToIndirectRef(obj, idx);
  if (UNLIKELY(checkRef != iref)) {
    LOG(ERROR) << "JNI ERROR (app bug): attempt to " << what
               << " stale " << kind_ << " " << iref
               << " (should be " << checkRef << ")";
    AbortMaybe();
    return false;
  }
  return true;
}

IndirectRef IndirectReferenceTable::Add(uint32_t cookie, const mirror::Object* obj) {
  IRTSegmentState prevState;
  prevState.all = cookie;
  size_t topIndex = segment_state_.parts.topIndex;

  DCHECK(obj != NULL);
  // TODO: stronger sanity check on the object (such as in heap)
  DCHECK_ALIGNED(reinterpret_cast<uintptr_t>(obj), 8);
  DCHECK(table_ != NULL);
  DCHECK_LE(alloc_entries_, max_entries_);
  DCHECK_GE(segment_state_.parts.numHoles, prevState.parts.numHoles);

  if (topIndex == alloc_entries_) {
    // reached end of allocated space; did we hit buffer max?
    if (topIndex == max_entries_) {
      LOG(FATAL) << "JNI ERROR (app bug): " << kind_ << " table overflow "
                 << "(max=" << max_entries_ << ")\n"
                 << MutatorLockedDumpable<IndirectReferenceTable>(*this);
    }

    size_t newSize = alloc_entries_ * 2;
    if (newSize > max_entries_) {
      newSize = max_entries_;
    }
    DCHECK_GT(newSize, alloc_entries_);

    table_ = reinterpret_cast<const mirror::Object**>(realloc(table_, newSize * sizeof(const mirror::Object*)));
    slot_data_ = reinterpret_cast<IndirectRefSlot*>(realloc(slot_data_,
                                                            newSize * sizeof(IndirectRefSlot)));
    if (table_ == NULL || slot_data_ == NULL) {
      LOG(FATAL) << "JNI ERROR (app bug): unable to expand "
                 << kind_ << " table (from "
                 << alloc_entries_ << " to " << newSize
                 << ", max=" << max_entries_ << ")\n"
                 << MutatorLockedDumpable<IndirectReferenceTable>(*this);
    }

    // Clear the newly-allocated slot_data_ elements.
    memset(slot_data_ + alloc_entries_, 0, (newSize - alloc_entries_) * sizeof(IndirectRefSlot));

    alloc_entries_ = newSize;
  }

  // We know there's enough room in the table.  Now we just need to find
  // the right spot.  If there's a hole, find it and fill it; otherwise,
  // add to the end of the list.
  IndirectRef result;
  int numHoles = segment_state_.parts.numHoles - prevState.parts.numHoles;
  if (numHoles > 0) {
    DCHECK_GT(topIndex, 1U);
    // Find the first hole; likely to be near the end of the list.
    const mirror::Object** pScan = &table_[topIndex - 1];
    DCHECK(*pScan != NULL);
    while (*--pScan != NULL) {
      DCHECK_GE(pScan, table_ + prevState.parts.topIndex);
    }
    UpdateSlotAdd(obj, pScan - table_);
    result = ToIndirectRef(obj, pScan - table_);
    *pScan = obj;
    segment_state_.parts.numHoles--;
  } else {
    // Add to the end.
    UpdateSlotAdd(obj, topIndex);
    result = ToIndirectRef(obj, topIndex);
    table_[topIndex++] = obj;
    segment_state_.parts.topIndex = topIndex;
  }
  if (false) {
    LOG(INFO) << "+++ added at " << ExtractIndex(result) << " top=" << segment_state_.parts.topIndex
              << " holes=" << segment_state_.parts.numHoles;
  }

  DCHECK(result != NULL);
  return result;
}

void IndirectReferenceTable::AssertEmpty() {
  if (UNLIKELY(begin() != end())) {
    ScopedObjectAccess soa(Thread::Current());
    LOG(FATAL) << "Internal Error: non-empty local reference table\n"
               << MutatorLockedDumpable<IndirectReferenceTable>(*this);
  }
}

// Verifies that the indirect table lookup is valid.
// Returns "false" if something looks bad.
bool IndirectReferenceTable::GetChecked(IndirectRef iref) const {
  if (UNLIKELY(iref == NULL)) {
    LOG(WARNING) << "Attempt to look up NULL " << kind_;
    return false;
  }
  if (UNLIKELY(GetIndirectRefKind(iref) == kSirtOrInvalid)) {
    LOG(ERROR) << "JNI ERROR (app bug): invalid " << kind_ << " " << iref;
    AbortMaybe();
    return false;
  }

  int topIndex = segment_state_.parts.topIndex;
  int idx = ExtractIndex(iref);
  if (UNLIKELY(idx >= topIndex)) {
    LOG(ERROR) << "JNI ERROR (app bug): accessed stale " << kind_ << " "
               << iref << " (index " << idx << " in a table of size " << topIndex << ")";
    AbortMaybe();
    return false;
  }

  if (UNLIKELY(table_[idx] == NULL)) {
    LOG(ERROR) << "JNI ERROR (app bug): accessed deleted " << kind_ << " " << iref;
    AbortMaybe();
    return false;
  }

  if (UNLIKELY(!CheckEntry("use", iref, idx))) {
    return false;
  }

  return true;
}

static int Find(mirror::Object* direct_pointer, int bottomIndex, int topIndex, const mirror::Object** table) {
  for (int i = bottomIndex; i < topIndex; ++i) {
    if (table[i] == direct_pointer) {
      return i;
    }
  }
  return -1;
}

bool IndirectReferenceTable::ContainsDirectPointer(mirror::Object* direct_pointer) const {
  return Find(direct_pointer, 0, segment_state_.parts.topIndex, table_) != -1;
}

// Removes an object. We extract the table offset bits from "iref"
// and zap the corresponding entry, leaving a hole if it's not at the top.
// If the entry is not between the current top index and the bottom index
// specified by the cookie, we don't remove anything. This is the behavior
// required by JNI's DeleteLocalRef function.
// This method is not called when a local frame is popped; this is only used
// for explicit single removals.
// Returns "false" if nothing was removed.
bool IndirectReferenceTable::Remove(uint32_t cookie, IndirectRef iref) {
  IRTSegmentState prevState;
  prevState.all = cookie;
  int topIndex = segment_state_.parts.topIndex;
  int bottomIndex = prevState.parts.topIndex;

  DCHECK(table_ != NULL);
  DCHECK_LE(alloc_entries_, max_entries_);
  DCHECK_GE(segment_state_.parts.numHoles, prevState.parts.numHoles);

  int idx = ExtractIndex(iref);

  JavaVMExt* vm = Runtime::Current()->GetJavaVM();
  if (GetIndirectRefKind(iref) == kSirtOrInvalid &&
      Thread::Current()->SirtContains(reinterpret_cast<jobject>(iref))) {
    LOG(WARNING) << "Attempt to remove local SIRT entry from IRT, ignoring";
    return true;
  }
  if (GetIndirectRefKind(iref) == kSirtOrInvalid && vm->work_around_app_jni_bugs) {
    mirror::Object* direct_pointer = reinterpret_cast<mirror::Object*>(iref);
    idx = Find(direct_pointer, bottomIndex, topIndex, table_);
    if (idx == -1) {
      LOG(WARNING) << "Trying to work around app JNI bugs, but didn't find " << iref << " in table!";
      return false;
    }
  }

  if (idx < bottomIndex) {
    // Wrong segment.
    LOG(WARNING) << "Attempt to remove index outside index area (" << idx
                 << " vs " << bottomIndex << "-" << topIndex << ")";
    return false;
  }
  if (idx >= topIndex) {
    // Bad --- stale reference?
    LOG(WARNING) << "Attempt to remove invalid index " << idx
                 << " (bottom=" << bottomIndex << " top=" << topIndex << ")";
    return false;
  }

  if (idx == topIndex-1) {
    // Top-most entry.  Scan up and consume holes.

    if (!vm->work_around_app_jni_bugs && !CheckEntry("remove", iref, idx)) {
      return false;
    }

    table_[idx] = NULL;
    int numHoles = segment_state_.parts.numHoles - prevState.parts.numHoles;
    if (numHoles != 0) {
      while (--topIndex > bottomIndex && numHoles != 0) {
        if (false) {
          LOG(INFO) << "+++ checking for hole at " << topIndex-1
                    << " (cookie=" << cookie << ") val=" << table_[topIndex - 1];
        }
        if (table_[topIndex-1] != NULL) {
          break;
        }
        if (false) {
          LOG(INFO) << "+++ ate hole at " << (topIndex - 1);
        }
        numHoles--;
      }
      segment_state_.parts.numHoles = numHoles + prevState.parts.numHoles;
      segment_state_.parts.topIndex = topIndex;
    } else {
      segment_state_.parts.topIndex = topIndex-1;
      if (false) {
        LOG(INFO) << "+++ ate last entry " << topIndex - 1;
      }
    }
  } else {
    // Not the top-most entry.  This creates a hole.  We NULL out the
    // entry to prevent somebody from deleting it twice and screwing up
    // the hole count.
    if (table_[idx] == NULL) {
      LOG(INFO) << "--- WEIRD: removing null entry " << idx;
      return false;
    }
    if (!vm->work_around_app_jni_bugs && !CheckEntry("remove", iref, idx)) {
      return false;
    }

    table_[idx] = NULL;
    segment_state_.parts.numHoles++;
    if (false) {
      LOG(INFO) << "+++ left hole at " << idx << ", holes=" << segment_state_.parts.numHoles;
    }
  }

  return true;
}

void IndirectReferenceTable::VisitRoots(RootVisitor* visitor, void* arg) {
  for (auto ref : *this) {
    visitor(*ref, arg);
  }
}

void IndirectReferenceTable::Dump(std::ostream& os) const {
  os << kind_ << " table dump:\n";
  std::vector<const mirror::Object*> entries(table_, table_ + Capacity());
  // Remove NULLs.
  for (int i = entries.size() - 1; i >= 0; --i) {
    if (entries[i] == NULL) {
      entries.erase(entries.begin() + i);
    }
  }
  ReferenceTable::Dump(os, entries);
}

}  // namespace art
