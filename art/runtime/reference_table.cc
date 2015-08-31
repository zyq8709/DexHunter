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

#include "reference_table.h"

#include "base/mutex.h"
#include "indirect_reference_table.h"
#include "mirror/array.h"
#include "mirror/array-inl.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "thread.h"
#include "utils.h"

namespace art {

ReferenceTable::ReferenceTable(const char* name, size_t initial_size, size_t max_size)
    : name_(name), max_size_(max_size) {
  CHECK_LE(initial_size, max_size);
  entries_.reserve(initial_size);
}

ReferenceTable::~ReferenceTable() {
}

void ReferenceTable::Add(const mirror::Object* obj) {
  DCHECK(obj != NULL);
  if (entries_.size() == max_size_) {
    LOG(FATAL) << "ReferenceTable '" << name_ << "' "
               << "overflowed (" << max_size_ << " entries)";
  }
  entries_.push_back(obj);
}

void ReferenceTable::Remove(const mirror::Object* obj) {
  // We iterate backwards on the assumption that references are LIFO.
  for (int i = entries_.size() - 1; i >= 0; --i) {
    if (entries_[i] == obj) {
      entries_.erase(entries_.begin() + i);
      return;
    }
  }
}

// If "obj" is an array, return the number of elements in the array.
// Otherwise, return zero.
static size_t GetElementCount(const mirror::Object* obj) {
  if (obj == NULL || obj == kClearedJniWeakGlobal || !obj->IsArrayInstance()) {
    return 0;
  }
  return obj->AsArray()->GetLength();
}

struct ObjectComparator {
  bool operator()(const mirror::Object* obj1, const mirror::Object* obj2)
    // TODO: enable analysis when analysis can work with the STL.
      NO_THREAD_SAFETY_ANALYSIS {
    Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
    // Ensure null references and cleared jweaks appear at the end.
    if (obj1 == NULL) {
      return true;
    } else if (obj2 == NULL) {
      return false;
    }
    if (obj1 == kClearedJniWeakGlobal) {
      return true;
    } else if (obj2 == kClearedJniWeakGlobal) {
      return false;
    }

    // Sort by class...
    if (obj1->GetClass() != obj2->GetClass()) {
      return obj1->GetClass()->IdentityHashCode() < obj2->IdentityHashCode();
    } else {
      // ...then by size...
      size_t count1 = obj1->SizeOf();
      size_t count2 = obj2->SizeOf();
      if (count1 != count2) {
        return count1 < count2;
      } else {
        // ...and finally by identity hash code.
        return obj1->IdentityHashCode() < obj2->IdentityHashCode();
      }
    }
  }
};

// Log an object with some additional info.
//
// Pass in the number of elements in the array (or 0 if this is not an
// array object), and the number of additional objects that are identical
// or equivalent to the original.
static void DumpSummaryLine(std::ostream& os, const mirror::Object* obj, size_t element_count,
                            int identical, int equiv)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (obj == NULL) {
    os << "    NULL reference (count=" << equiv << ")\n";
    return;
  }
  if (obj == kClearedJniWeakGlobal) {
    os << "    cleared jweak (count=" << equiv << ")\n";
    return;
  }

  std::string className(PrettyTypeOf(obj));
  if (obj->IsClass()) {
    // We're summarizing multiple instances, so using the exemplar
    // Class' type parameter here would be misleading.
    className = "java.lang.Class";
  }
  if (element_count != 0) {
    StringAppendF(&className, " (%zd elements)", element_count);
  }

  size_t total = identical + equiv + 1;
  std::string msg(StringPrintf("%5zd of %s", total, className.c_str()));
  if (identical + equiv != 0) {
    StringAppendF(&msg, " (%d unique instances)", equiv + 1);
  }
  os << "    " << msg << "\n";
}

size_t ReferenceTable::Size() const {
  return entries_.size();
}

void ReferenceTable::Dump(std::ostream& os) const {
  os << name_ << " reference table dump:\n";
  Dump(os, entries_);
}

void ReferenceTable::Dump(std::ostream& os, const Table& entries) {
  if (entries.empty()) {
    os << "  (empty)\n";
    return;
  }

  // Dump the most recent N entries.
  const size_t kLast = 10;
  size_t count = entries.size();
  int first = count - kLast;
  if (first < 0) {
    first = 0;
  }
  os << "  Last " << (count - first) << " entries (of " << count << "):\n";
  for (int idx = count - 1; idx >= first; --idx) {
    const mirror::Object* ref = entries[idx];
    if (ref == NULL) {
      continue;
    }
    if (ref == kClearedJniWeakGlobal) {
      os << StringPrintf("    %5d: cleared jweak\n", idx);
      continue;
    }
    if (ref->GetClass() == NULL) {
      // should only be possible right after a plain dvmMalloc().
      size_t size = ref->SizeOf();
      os << StringPrintf("    %5d: %p (raw) (%zd bytes)\n", idx, ref, size);
      continue;
    }

    std::string className(PrettyTypeOf(ref));

    std::string extras;
    size_t element_count = GetElementCount(ref);
    if (element_count != 0) {
      StringAppendF(&extras, " (%zd elements)", element_count);
    } else if (ref->GetClass()->IsStringClass()) {
      mirror::String* s = const_cast<mirror::Object*>(ref)->AsString();
      std::string utf8(s->ToModifiedUtf8());
      if (s->GetLength() <= 16) {
        StringAppendF(&extras, " \"%s\"", utf8.c_str());
      } else {
        StringAppendF(&extras, " \"%.16s... (%d chars)", utf8.c_str(), s->GetLength());
      }
    }
    os << StringPrintf("    %5d: ", idx) << ref << " " << className << extras << "\n";
  }

  // Make a copy of the table and sort it.
  Table sorted_entries(entries.begin(), entries.end());
  std::sort(sorted_entries.begin(), sorted_entries.end(), ObjectComparator());

  // Remove any uninteresting stuff from the list. The sort moved them all to the end.
  while (!sorted_entries.empty() && sorted_entries.back() == NULL) {
    sorted_entries.pop_back();
  }
  while (!sorted_entries.empty() && sorted_entries.back() == kClearedJniWeakGlobal) {
    sorted_entries.pop_back();
  }
  if (sorted_entries.empty()) {
    return;
  }

  // Dump a summary of the whole table.
  os << "  Summary:\n";
  size_t equiv = 0;
  size_t identical = 0;
  for (size_t idx = 1; idx < count; idx++) {
    const mirror::Object* prev = sorted_entries[idx-1];
    const mirror::Object* current = sorted_entries[idx];
    size_t element_count = GetElementCount(prev);
    if (current == prev) {
      // Same reference, added more than once.
      identical++;
    } else if (current->GetClass() == prev->GetClass() && GetElementCount(current) == element_count) {
      // Same class / element count, different object.
      equiv++;
    } else {
      // Different class.
      DumpSummaryLine(os, prev, element_count, identical, equiv);
      equiv = identical = 0;
    }
  }
  // Handle the last entry.
  DumpSummaryLine(os, sorted_entries.back(), GetElementCount(sorted_entries.back()), identical, equiv);
}

void ReferenceTable::VisitRoots(RootVisitor* visitor, void* arg) {
  for (const auto& ref : entries_) {
    visitor(ref, arg);
  }
}

}  // namespace art
