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

#include "intern_table.h"

#include "gc/space/image_space.h"
#include "mirror/dex_cache.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "thread.h"
#include "UniquePtr.h"
#include "utf.h"

namespace art {

InternTable::InternTable()
    : intern_table_lock_("InternTable lock"), is_dirty_(false), allow_new_interns_(true),
      new_intern_condition_("New intern condition", intern_table_lock_) {
}

size_t InternTable::Size() const {
  MutexLock mu(Thread::Current(), intern_table_lock_);
  return strong_interns_.size() + weak_interns_.size();
}

void InternTable::DumpForSigQuit(std::ostream& os) const {
  MutexLock mu(Thread::Current(), intern_table_lock_);
  os << "Intern table: " << strong_interns_.size() << " strong; "
     << weak_interns_.size() << " weak\n";
}

void InternTable::VisitRoots(RootVisitor* visitor, void* arg,
                             bool only_dirty, bool clean_dirty) {
  MutexLock mu(Thread::Current(), intern_table_lock_);
  if (!only_dirty || is_dirty_) {
    for (const auto& strong_intern : strong_interns_) {
      visitor(strong_intern.second, arg);
    }
    if (clean_dirty) {
      is_dirty_ = false;
    }
  }
  // Note: we deliberately don't visit the weak_interns_ table and the immutable
  // image roots.
}

mirror::String* InternTable::Lookup(Table& table, mirror::String* s,
                                    uint32_t hash_code) {
  intern_table_lock_.AssertHeld(Thread::Current());
  for (auto it = table.find(hash_code), end = table.end(); it != end; ++it) {
    mirror::String* existing_string = it->second;
    if (existing_string->Equals(s)) {
      return existing_string;
    }
  }
  return NULL;
}

mirror::String* InternTable::Insert(Table& table, mirror::String* s,
                                    uint32_t hash_code) {
  intern_table_lock_.AssertHeld(Thread::Current());
  table.insert(std::make_pair(hash_code, s));
  return s;
}

void InternTable::Remove(Table& table, const mirror::String* s,
                         uint32_t hash_code) {
  intern_table_lock_.AssertHeld(Thread::Current());
  for (auto it = table.find(hash_code), end = table.end(); it != end; ++it) {
    if (it->second == s) {
      table.erase(it);
      return;
    }
  }
}

static mirror::String* LookupStringFromImage(mirror::String* s)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  gc::space::ImageSpace* image = Runtime::Current()->GetHeap()->GetImageSpace();
  if (image == NULL) {
    return NULL;  // No image present.
  }
  mirror::Object* root = image->GetImageHeader().GetImageRoot(ImageHeader::kDexCaches);
  mirror::ObjectArray<mirror::DexCache>* dex_caches = root->AsObjectArray<mirror::DexCache>();
  const std::string utf8 = s->ToModifiedUtf8();
  for (int32_t i = 0; i < dex_caches->GetLength(); ++i) {
    mirror::DexCache* dex_cache = dex_caches->Get(i);
    const DexFile* dex_file = dex_cache->GetDexFile();
    // Binary search the dex file for the string index.
    const DexFile::StringId* string_id = dex_file->FindStringId(utf8.c_str());
    if (string_id != NULL) {
      uint32_t string_idx = dex_file->GetIndexForStringId(*string_id);
      mirror::String* image = dex_cache->GetResolvedString(string_idx);
      if (image != NULL) {
        return image;
      }
    }
  }
  return NULL;
}

void InternTable::AllowNewInterns() {
  Thread* self = Thread::Current();
  MutexLock mu(self, intern_table_lock_);
  allow_new_interns_ = true;
  new_intern_condition_.Broadcast(self);
}

void InternTable::DisallowNewInterns() {
  Thread* self = Thread::Current();
  MutexLock mu(self, intern_table_lock_);
  allow_new_interns_ = false;
}

mirror::String* InternTable::Insert(mirror::String* s, bool is_strong) {
  Thread* self = Thread::Current();
  MutexLock mu(self, intern_table_lock_);

  DCHECK(s != NULL);
  uint32_t hash_code = s->GetHashCode();

  while (UNLIKELY(!allow_new_interns_)) {
    new_intern_condition_.WaitHoldingLocks(self);
  }

  if (is_strong) {
    // Check the strong table for a match.
    mirror::String* strong = Lookup(strong_interns_, s, hash_code);
    if (strong != NULL) {
      return strong;
    }

    // Mark as dirty so that we rescan the roots.
    is_dirty_ = true;

    // Check the image for a match.
    mirror::String* image = LookupStringFromImage(s);
    if (image != NULL) {
      return Insert(strong_interns_, image, hash_code);
    }

    // There is no match in the strong table, check the weak table.
    mirror::String* weak = Lookup(weak_interns_, s, hash_code);
    if (weak != NULL) {
      // A match was found in the weak table. Promote to the strong table.
      Remove(weak_interns_, weak, hash_code);
      return Insert(strong_interns_, weak, hash_code);
    }

    // No match in the strong table or the weak table. Insert into the strong
    // table.
    return Insert(strong_interns_, s, hash_code);
  }

  // Check the strong table for a match.
  mirror::String* strong = Lookup(strong_interns_, s, hash_code);
  if (strong != NULL) {
    return strong;
  }
  // Check the image for a match.
  mirror::String* image = LookupStringFromImage(s);
  if (image != NULL) {
    return Insert(weak_interns_, image, hash_code);
  }
  // Check the weak table for a match.
  mirror::String* weak = Lookup(weak_interns_, s, hash_code);
  if (weak != NULL) {
    return weak;
  }
  // Insert into the weak table.
  return Insert(weak_interns_, s, hash_code);
}

mirror::String* InternTable::InternStrong(int32_t utf16_length,
                                          const char* utf8_data) {
  return InternStrong(mirror::String::AllocFromModifiedUtf8(
      Thread::Current(), utf16_length, utf8_data));
}

mirror::String* InternTable::InternStrong(const char* utf8_data) {
  return InternStrong(
      mirror::String::AllocFromModifiedUtf8(Thread::Current(), utf8_data));
}

mirror::String* InternTable::InternStrong(mirror::String* s) {
  if (s == NULL) {
    return NULL;
  }
  return Insert(s, true);
}

mirror::String* InternTable::InternWeak(mirror::String* s) {
  if (s == NULL) {
    return NULL;
  }
  return Insert(s, false);
}

bool InternTable::ContainsWeak(mirror::String* s) {
  MutexLock mu(Thread::Current(), intern_table_lock_);
  const mirror::String* found = Lookup(weak_interns_, s, s->GetHashCode());
  return found == s;
}

void InternTable::SweepInternTableWeaks(IsMarkedTester is_marked, void* arg) {
  MutexLock mu(Thread::Current(), intern_table_lock_);
  // TODO: std::remove_if + lambda.
  for (auto it = weak_interns_.begin(), end = weak_interns_.end(); it != end;) {
    mirror::Object* object = it->second;
    if (!is_marked(object, arg)) {
      weak_interns_.erase(it++);
    } else {
      ++it;
    }
  }
}

}  // namespace art
