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

#ifndef ART_RUNTIME_GC_ACCOUNTING_GC_ALLOCATOR_H_
#define ART_RUNTIME_GC_ACCOUNTING_GC_ALLOCATOR_H_

#include "utils.h"

#include <cstdlib>
#include <limits>
#include <memory>

namespace art {
namespace gc {
namespace accounting {
  void* RegisterGCAllocation(size_t bytes);
  void RegisterGCDeAllocation(void* p, size_t bytes);

  static const bool kMeasureGCMemoryOverhead = false;

  template <typename T>
  class GCAllocatorImpl : public std::allocator<T> {
  public:
    typedef typename std::allocator<T>::value_type value_type;
    typedef typename std::allocator<T>::size_type size_type;
    typedef typename std::allocator<T>::difference_type difference_type;
    typedef typename std::allocator<T>::pointer pointer;
    typedef typename std::allocator<T>::const_pointer const_pointer;
    typedef typename std::allocator<T>::reference reference;
    typedef typename std::allocator<T>::const_reference const_reference;

    // Used internally by STL data structures.
    template <class U>
    GCAllocatorImpl(const GCAllocatorImpl<U>& alloc) throw() {
    }

    // Used internally by STL data structures.
    GCAllocatorImpl() throw() {
    }

    // Enables an allocator for objects of one type to allocate storage for objects of another type.
    // Used internally by STL data structures.
    template <class U>
    struct rebind {
        typedef GCAllocatorImpl<U> other;
    };

    pointer allocate(size_type n, const_pointer hint = 0) {
      return reinterpret_cast<pointer>(RegisterGCAllocation(n * sizeof(T)));
    }

    template <typename PT>
    void deallocate(PT p, size_type n) {
      RegisterGCDeAllocation(p, n * sizeof(T));
    }
  };

  // C++ doesn't allow template typedefs. This is a workaround template typedef which is
  // GCAllocatorImpl<T> if kMeasureGCMemoryOverhead is true, std::allocator<T> otherwise.
  template <typename T>
  class GCAllocator : public TypeStaticIf<kMeasureGCMemoryOverhead,
                                          GCAllocatorImpl<T>,
                                          std::allocator<T> >::value {
  };
}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_GC_ALLOCATOR_H_
