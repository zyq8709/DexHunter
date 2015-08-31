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

#include <stddef.h>
#include <map>
#include <list>

#ifndef ART_COMPILER_UTILS_SCOPED_HASHTABLE_H_
#define ART_COMPILER_UTILS_SCOPED_HASHTABLE_H_

namespace utils {
template <typename K, typename V>
class ScopedHashtable {
 public:
  explicit ScopedHashtable():scopes() {
  }

  void OpenScope() {
    scopes.push_front(std::map<K, V>());
  }

  // Lookups entry K starting from the current (topmost) scope
  // and returns its value if found or NULL.
  V Lookup(K k) const {
    for (typename std::list<std::map<K, V> >::const_iterator scopes_it = scopes.begin();
        scopes_it != scopes.end(); scopes_it++) {
      typename std::map<K, V>::const_iterator result_it = (*scopes_it).find(k);
      if (result_it != (*scopes_it).end()) {
        return (*result_it).second;
      }
    }
    return NULL;
  }

  // Adds a new entry in the current (topmost) scope.
  void Add(K k, V v) {
    scopes.front().erase(k);
    scopes.front().insert(std::pair< K, V >(k, v));
  }

  // Removes the topmost scope.
  bool CloseScope() {
    // Added check to uniformly handle undefined behavior
    // when removing scope and the list of scopes is empty.
    if (scopes.size() > 0) {
      scopes.pop_front();
      return true;
    }
    return false;
  }

 private:
  std::list<std::map<K, V> > scopes;
};
}  // namespace utils

#endif  // ART_COMPILER_UTILS_SCOPED_HASHTABLE_H_
