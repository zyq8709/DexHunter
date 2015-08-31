/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include "atomic.h"

#define NEED_SWAP_MUTEXES !defined(__arm__) && !defined(__i386__)

#if NEED_SWAP_MUTEXES
#include <vector>
#include "base/mutex.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "thread.h"
#endif

namespace art {

#if NEED_SWAP_MUTEXES
// We stripe across a bunch of different mutexes to reduce contention.
static const size_t kSwapMutexCount = 32;
static std::vector<Mutex*>* gSwapMutexes;

static Mutex& GetSwapMutex(const volatile int64_t* addr) {
  return *(*gSwapMutexes)[(reinterpret_cast<unsigned>(addr) >> 3U) % kSwapMutexCount];
}
#endif

void QuasiAtomic::Startup() {
#if NEED_SWAP_MUTEXES
  gSwapMutexes = new std::vector<Mutex*>;
  for (size_t i = 0; i < kSwapMutexCount; ++i) {
    gSwapMutexes->push_back(new Mutex("QuasiAtomic stripe"));
  }
#endif
}

void QuasiAtomic::Shutdown() {
#if NEED_SWAP_MUTEXES
  STLDeleteElements(gSwapMutexes);
  delete gSwapMutexes;
#endif
}

int64_t QuasiAtomic::Read64(volatile const int64_t* addr) {
  int64_t value;
#if NEED_SWAP_MUTEXES
  MutexLock mu(Thread::Current(), GetSwapMutex(addr));
  value = *addr;
#elif defined(__arm__)
  // Exclusive loads are defined not to tear, clearing the exclusive state isn't necessary. If we
  // have LPAE (such as Cortex-A15) then ldrd would suffice.
  __asm__ __volatile__("@ QuasiAtomic::Read64\n"
      "ldrexd     %0, %H0, [%1]"
      : "=&r" (value)
      : "r" (addr));
#elif defined(__i386__)
  __asm__ __volatile__(
      "movq     %1, %0\n"
      : "=x" (value)
      : "m" (*addr));
#else
#error Unexpected architecture
#endif
  return value;
}

void QuasiAtomic::Write64(volatile int64_t* addr, int64_t value) {
#if NEED_SWAP_MUTEXES
  MutexLock mu(Thread::Current(), GetSwapMutex(addr));
  *addr = value;
#elif defined(__arm__)
  // The write is done as a swap so that the cache-line is in the exclusive state for the store. If
  // we know that ARM architecture has LPAE (such as Cortex-A15) this isn't necessary and strd will
  // suffice.
  int64_t prev;
  int status;
  do {
    __asm__ __volatile__("@ QuasiAtomic::Write64\n"
        "ldrexd     %0, %H0, [%3]\n"
        "strexd     %1, %4, %H4, [%3]"
        : "=&r" (prev), "=&r" (status), "+m"(*addr)
        : "r" (addr), "r" (value)
        : "cc");
  } while (__builtin_expect(status != 0, 0));
#elif defined(__i386__)
  __asm__ __volatile__(
      "movq     %1, %0"
      : "=m" (*addr)
      : "x" (value));
#else
#error Unexpected architecture
#endif
}


bool QuasiAtomic::Cas64(int64_t old_value, int64_t new_value, volatile int64_t* addr) {
#if NEED_SWAP_MUTEXES
  MutexLock mu(Thread::Current(), GetSwapMutex(addr));
  if (*addr == old_value) {
    *addr = new_value;
    return true;
  }
  return false;
#elif defined(__arm__)
  int64_t prev;
  int status;
  do {
    __asm__ __volatile__("@ QuasiAtomic::Cas64\n"
        "ldrexd     %0, %H0, [%3]\n"
        "mov        %1, #0\n"
        "teq        %0, %4\n"
        "teqeq      %H0, %H4\n"
        "strexdeq   %1, %5, %H5, [%3]"
        : "=&r" (prev), "=&r" (status), "+m"(*addr)
        : "r" (addr), "Ir" (old_value), "r" (new_value)
        : "cc");
  } while (__builtin_expect(status != 0, 0));
  return prev == old_value;
#elif defined(__i386__)
  // The compiler does the right job and works better than inline assembly, especially with -O0
  // compilation.
  return __sync_bool_compare_and_swap(addr, old_value, new_value);
#else
#error Unexpected architecture
#endif
}

bool QuasiAtomic::LongAtomicsUseMutexes() {
#if NEED_SWAP_MUTEXES
  return true;
#else
  return false;
#endif
}

}  // namespace art
