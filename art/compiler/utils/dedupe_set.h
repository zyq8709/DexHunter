/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_DEDUPE_SET_H_
#define ART_COMPILER_UTILS_DEDUPE_SET_H_

#include <set>

#include "base/mutex.h"
#include "base/stl_util.h"

namespace art {

// A simple data structure to handle hashed deduplication. Add is thread safe.
template <typename Key, typename HashType, typename HashFunc>
class DedupeSet {
  typedef std::pair<HashType, Key*> HashedKey;

  class Comparator {
   public:
    bool operator()(const HashedKey& a, const HashedKey& b) const {
      if (a.first < b.first) return true;
      if (a.first > b.first) return true;
      return *a.second < *b.second;
    }
  };

  typedef std::set<HashedKey, Comparator> Keys;

 public:
  typedef typename Keys::iterator iterator;
  typedef typename Keys::const_iterator const_iterator;
  typedef typename Keys::size_type size_type;
  typedef typename Keys::value_type value_type;

  iterator begin() { return keys_.begin(); }
  const_iterator begin() const { return keys_.begin(); }
  iterator end() { return keys_.end(); }
  const_iterator end() const { return keys_.end(); }

  Key* Add(Thread* self, const Key& key) {
    HashType hash = HashFunc()(key);
    HashedKey hashed_key(hash, const_cast<Key*>(&key));
    MutexLock lock(self, lock_);
    auto it = keys_.find(hashed_key);
    if (it != keys_.end()) {
      return it->second;
    }
    hashed_key.second = new Key(key);
    keys_.insert(hashed_key);
    return hashed_key.second;
  }

  DedupeSet() : lock_("dedupe lock") {
  }

  ~DedupeSet() {
    STLDeleteValues(&keys_);
  }

 private:
  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  Keys keys_;
  DISALLOW_COPY_AND_ASSIGN(DedupeSet);
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_DEDUPE_SET_H_
