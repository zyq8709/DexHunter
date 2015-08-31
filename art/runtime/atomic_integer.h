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

#ifndef ART_RUNTIME_ATOMIC_INTEGER_H_
#define ART_RUNTIME_ATOMIC_INTEGER_H_

#include "cutils/atomic.h"
#include "cutils/atomic-inline.h"

namespace art {

class AtomicInteger {
 public:
  AtomicInteger() : value_(0) { }

  explicit AtomicInteger(int32_t value) : value_(value) { }

  // Unsafe = operator for non atomic operations on the integer.
  void store(int32_t desired) {
    value_ = desired;
  }

  AtomicInteger& operator=(int32_t desired) {
    store(desired);
    return *this;
  }

  int32_t load() const {
    return value_;
  }

  operator int32_t() const {
    return load();
  }

  int32_t fetch_add(const int32_t value) {
    return android_atomic_add(value, &value_);
  }

  int32_t fetch_sub(const int32_t value) {
    return android_atomic_add(-value, &value_);
  }

  int32_t operator++() {
    return android_atomic_inc(&value_) + 1;
  }

  int32_t operator++(int32_t) {
    return android_atomic_inc(&value_);
  }

  int32_t operator--() {
    return android_atomic_dec(&value_) - 1;
  }

  int32_t operator--(int32_t) {
    return android_atomic_dec(&value_);
  }

  bool compare_and_swap(int32_t expected_value, int32_t desired_value) {
    return android_atomic_cas(expected_value, desired_value, &value_) == 0;
  }

 private:
  volatile int32_t value_;
};

}  // namespace art

#endif  // ART_RUNTIME_ATOMIC_INTEGER_H_
