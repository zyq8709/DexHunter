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

#ifndef ART_RUNTIME_SAFE_MAP_H_
#define ART_RUNTIME_SAFE_MAP_H_

#include <map>
#include <memory>

#include "base/logging.h"

namespace art {

// Equivalent to std::map, but without operator[] and its bug-prone semantics (in particular,
// the implicit insertion of a default-constructed value on failed lookups).
template <typename K, typename V, typename Comparator = std::less<K>,
          typename Allocator = std::allocator<std::pair<const K, V> > >
class SafeMap {
 private:
  typedef SafeMap<K, V, Comparator, Allocator> Self;

 public:
  typedef typename ::std::map<K, V, Comparator>::iterator iterator;
  typedef typename ::std::map<K, V, Comparator>::const_iterator const_iterator;
  typedef typename ::std::map<K, V, Comparator>::size_type size_type;
  typedef typename ::std::map<K, V, Comparator>::value_type value_type;

  Self& operator=(const Self& rhs) {
    map_ = rhs.map_;
    return *this;
  }

  iterator begin() { return map_.begin(); }
  const_iterator begin() const { return map_.begin(); }
  iterator end() { return map_.end(); }
  const_iterator end() const { return map_.end(); }

  bool empty() const { return map_.empty(); }
  size_type size() const { return map_.size(); }

  void clear() { map_.clear(); }
  void erase(iterator it) { map_.erase(it); }
  size_type erase(const K& k) { return map_.erase(k); }

  iterator find(const K& k) { return map_.find(k); }
  const_iterator find(const K& k) const { return map_.find(k); }

  size_type count(const K& k) const { return map_.count(k); }

  // Note that unlike std::map's operator[], this doesn't return a reference to the value.
  V Get(const K& k) const {
    const_iterator it = map_.find(k);
    DCHECK(it != map_.end());
    return it->second;
  }

  // Used to insert a new mapping.
  void Put(const K& k, const V& v) {
    std::pair<iterator, bool> result = map_.insert(std::make_pair(k, v));
    DCHECK(result.second);  // Check we didn't accidentally overwrite an existing value.
  }

  // Used to insert a new mapping or overwrite an existing mapping. Note that if the value type
  // of this container is a pointer, any overwritten pointer will be lost and if this container
  // was the owner, you have a leak.
  void Overwrite(const K& k, const V& v) {
    std::pair<iterator, bool> result = map_.insert(std::make_pair(k, v));
    if (!result.second) {
      // Already there - update the value for the existing key
      result.first->second = v;
    }
  }

  bool Equals(const Self& rhs) const {
    return map_ == rhs.map_;
  }

 private:
  ::std::map<K, V, Comparator, Allocator> map_;
};

template <typename K, typename V, typename Comparator>
bool operator==(const SafeMap<K, V, Comparator>& lhs, const SafeMap<K, V, Comparator>& rhs) {
  return lhs.Equals(rhs);
}

template <typename K, typename V, typename Comparator>
bool operator!=(const SafeMap<K, V, Comparator>& lhs, const SafeMap<K, V, Comparator>& rhs) {
  return !(lhs == rhs);
}

}  // namespace art

#endif  // ART_RUNTIME_SAFE_MAP_H_
