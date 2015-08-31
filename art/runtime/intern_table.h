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

#ifndef ART_RUNTIME_INTERN_TABLE_H_
#define ART_RUNTIME_INTERN_TABLE_H_

#include "base/mutex.h"
#include "root_visitor.h"

#include <map>

namespace art {
namespace mirror {
class String;
}  // namespace mirror

/**
 * Used to intern strings.
 *
 * There are actually two tables: one that holds strong references to its strings, and one that
 * holds weak references. The former is used for string literals, for which there is an effective
 * reference from the constant pool. The latter is used for strings interned at runtime via
 * String.intern. Some code (XML parsers being a prime example) relies on being able to intern
 * arbitrarily many strings for the duration of a parse without permanently increasing the memory
 * footprint.
 */
class InternTable {
 public:
  InternTable();

  // Interns a potentially new string in the 'strong' table. (See above.)
  mirror::String* InternStrong(int32_t utf16_length, const char* utf8_data)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Interns a potentially new string in the 'strong' table. (See above.)
  mirror::String* InternStrong(const char* utf8_data)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Interns a potentially new string in the 'strong' table. (See above.)
  mirror::String* InternStrong(mirror::String* s) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Interns a potentially new string in the 'weak' table. (See above.)
  mirror::String* InternWeak(mirror::String* s) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SweepInternTableWeaks(IsMarkedTester is_marked, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  bool ContainsWeak(mirror::String* s) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t Size() const;

  void VisitRoots(RootVisitor* visitor, void* arg, bool only_dirty, bool clean_dirty);

  void DumpForSigQuit(std::ostream& os) const;

  void DisallowNewInterns() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  void AllowNewInterns() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  typedef std::multimap<int32_t, mirror::String*> Table;

  mirror::String* Insert(mirror::String* s, bool is_strong)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::String* Lookup(Table& table, mirror::String* s, uint32_t hash_code)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::String* Insert(Table& table, mirror::String* s, uint32_t hash_code);
  void Remove(Table& table, const mirror::String* s, uint32_t hash_code);

  mutable Mutex intern_table_lock_;
  bool is_dirty_ GUARDED_BY(intern_table_lock_);
  bool allow_new_interns_ GUARDED_BY(intern_table_lock_);
  ConditionVariable new_intern_condition_ GUARDED_BY(intern_table_lock_);
  Table strong_interns_ GUARDED_BY(intern_table_lock_);
  Table weak_interns_ GUARDED_BY(intern_table_lock_);
};

}  // namespace art

#endif  // ART_RUNTIME_INTERN_TABLE_H_
