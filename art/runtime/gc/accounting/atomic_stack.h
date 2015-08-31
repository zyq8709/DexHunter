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

#ifndef ART_RUNTIME_GC_ACCOUNTING_ATOMIC_STACK_H_
#define ART_RUNTIME_GC_ACCOUNTING_ATOMIC_STACK_H_

#include <string>

#include "atomic_integer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "UniquePtr.h"
#include "mem_map.h"
#include "utils.h"

namespace art {
namespace gc {
namespace accounting {

template <typename T>
class AtomicStack {
 public:
  // Capacity is how many elements we can store in the stack.
  static AtomicStack* Create(const std::string& name, size_t capacity) {
    UniquePtr<AtomicStack> mark_stack(new AtomicStack(name, capacity));
    mark_stack->Init();
    return mark_stack.release();
  }

  ~AtomicStack() {}

  void Reset() {
    DCHECK(mem_map_.get() != NULL);
    DCHECK(begin_ != NULL);
    front_index_ = 0;
    back_index_ = 0;
    debug_is_sorted_ = true;
    int result = madvise(begin_, sizeof(T) * capacity_, MADV_DONTNEED);
    if (result == -1) {
      PLOG(WARNING) << "madvise failed";
    }
  }

  // Beware: Mixing atomic pushes and atomic pops will cause ABA problem.

  // Returns false if we overflowed the stack.
  bool AtomicPushBack(const T& value) {
    if (kIsDebugBuild) {
      debug_is_sorted_ = false;
    }
    int32_t index;
    do {
      index = back_index_;
      if (UNLIKELY(static_cast<size_t>(index) >= capacity_)) {
        // Stack overflow.
        return false;
      }
    } while (!back_index_.compare_and_swap(index, index + 1));
    begin_[index] = value;
    return true;
  }

  void PushBack(const T& value) {
    if (kIsDebugBuild) {
      debug_is_sorted_ = false;
    }
    int32_t index = back_index_;
    DCHECK_LT(static_cast<size_t>(index), capacity_);
    back_index_ = index + 1;
    begin_[index] = value;
  }

  T PopBack() {
    DCHECK_GT(back_index_, front_index_);
    // Decrement the back index non atomically.
    back_index_ = back_index_ - 1;
    return begin_[back_index_];
  }

  // Take an item from the front of the stack.
  T PopFront() {
    int32_t index = front_index_;
    DCHECK_LT(index, back_index_.load());
    front_index_ = front_index_ + 1;
    return begin_[index];
  }

  // Pop a number of elements.
  void PopBackCount(int32_t n) {
    DCHECK_GE(Size(), static_cast<size_t>(n));
    back_index_.fetch_sub(n);
  }

  bool IsEmpty() const {
    return Size() == 0;
  }

  size_t Size() const {
    DCHECK_LE(front_index_, back_index_);
    return back_index_ - front_index_;
  }

  T* Begin() const {
    return const_cast<T*>(begin_ + front_index_);
  }

  T* End() const {
    return const_cast<T*>(begin_ + back_index_);
  }

  size_t Capacity() const {
    return capacity_;
  }

  // Will clear the stack.
  void Resize(size_t new_capacity) {
    capacity_ = new_capacity;
    Init();
  }

  void Sort() {
    int32_t start_back_index = back_index_.load();
    int32_t start_front_index = front_index_.load();
    std::sort(Begin(), End());
    CHECK_EQ(start_back_index, back_index_.load());
    CHECK_EQ(start_front_index, front_index_.load());
    if (kIsDebugBuild) {
      debug_is_sorted_ = true;
    }
  }

  bool ContainsSorted(const T& value) const {
    DCHECK(debug_is_sorted_);
    return std::binary_search(Begin(), End(), value);
  }

  bool Contains(const T& value) const {
    return std::find(Begin(), End(), value) != End();
  }

 private:
  AtomicStack(const std::string& name, const size_t capacity)
      : name_(name),
        back_index_(0),
        front_index_(0),
        begin_(NULL),
        capacity_(capacity),
        debug_is_sorted_(true) {
  }

  // Size in number of elements.
  void Init() {
    mem_map_.reset(MemMap::MapAnonymous(name_.c_str(), NULL, capacity_ * sizeof(T), PROT_READ | PROT_WRITE));
    CHECK(mem_map_.get() != NULL) << "couldn't allocate mark stack";
    byte* addr = mem_map_->Begin();
    CHECK(addr != NULL);
    debug_is_sorted_ = true;
    begin_ = reinterpret_cast<T*>(addr);
    Reset();
  }

  // Name of the mark stack.
  std::string name_;

  // Memory mapping of the atomic stack.
  UniquePtr<MemMap> mem_map_;

  // Back index (index after the last element pushed).
  AtomicInteger back_index_;

  // Front index, used for implementing PopFront.
  AtomicInteger front_index_;

  // Base of the atomic stack.
  T* begin_;

  // Maximum number of elements.
  size_t capacity_;

  // Whether or not the stack is sorted, only updated in debug mode to avoid performance overhead.
  bool debug_is_sorted_;

  DISALLOW_COPY_AND_ASSIGN(AtomicStack);
};

typedef AtomicStack<mirror::Object*> ObjectStack;

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_ATOMIC_STACK_H_
