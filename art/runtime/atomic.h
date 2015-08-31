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

#ifndef ART_RUNTIME_ATOMIC_H_
#define ART_RUNTIME_ATOMIC_H_

#include <stdint.h>

#include "base/macros.h"

namespace art {

// NOTE: Two "quasiatomic" operations on the exact same memory address
// are guaranteed to operate atomically with respect to each other,
// but no guarantees are made about quasiatomic operations mixed with
// non-quasiatomic operations on the same address, nor about
// quasiatomic operations that are performed on partially-overlapping
// memory.
class QuasiAtomic {
 public:
  static void Startup();

  static void Shutdown();

  // Reads the 64-bit value at "addr" without tearing.
  static int64_t Read64(volatile const int64_t* addr);

  // Writes to the 64-bit value at "addr" without tearing.
  static void Write64(volatile int64_t* addr, int64_t val);

  // Atomically compare the value at "addr" to "old_value", if equal replace it with "new_value"
  // and return true. Otherwise, don't swap, and return false.
  static bool Cas64(int64_t old_value, int64_t new_value, volatile int64_t* addr);

  // Does the architecture provide reasonable atomic long operations or do we fall back on mutexes?
  static bool LongAtomicsUseMutexes();

 private:
  DISALLOW_COPY_AND_ASSIGN(QuasiAtomic);
};

}  // namespace art

#endif  // ART_RUNTIME_ATOMIC_H_
