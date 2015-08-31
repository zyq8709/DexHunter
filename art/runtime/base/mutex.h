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

#ifndef ART_RUNTIME_BASE_MUTEX_H_
#define ART_RUNTIME_BASE_MUTEX_H_

#include <pthread.h>
#include <stdint.h>

#include <iosfwd>
#include <string>

#include "atomic_integer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"
#include "locks.h"

#if defined(__APPLE__)
#define ART_USE_FUTEXES 0
#else
#define ART_USE_FUTEXES !defined(__mips__)
#endif

// Currently Darwin doesn't support locks with timeouts.
#if !defined(__APPLE__)
#define HAVE_TIMED_RWLOCK 1
#else
#define HAVE_TIMED_RWLOCK 0
#endif

namespace art {

class ScopedContentionRecorder;
class Thread;

const bool kDebugLocking = kIsDebugBuild;

// Record Log contention information, dumpable via SIGQUIT.
#ifdef ART_USE_FUTEXES
// To enable lock contention logging, set this to true.
const bool kLogLockContentions = false;
#else
// Keep this false as lock contention logging is supported only with
// futex.
const bool kLogLockContentions = false;
#endif
const size_t kContentionLogSize = 64;
const size_t kContentionLogDataSize = kLogLockContentions ? 1 : 0;
const size_t kAllMutexDataSize = kLogLockContentions ? 1 : 0;

// Base class for all Mutex implementations
class BaseMutex {
 public:
  const char* GetName() const {
    return name_;
  }

  virtual bool IsMutex() const { return false; }
  virtual bool IsReaderWriterMutex() const { return false; }

  virtual void Dump(std::ostream& os) const = 0;

  static void DumpAll(std::ostream& os);

 protected:
  friend class ConditionVariable;

  BaseMutex(const char* name, LockLevel level);
  virtual ~BaseMutex();
  void RegisterAsLocked(Thread* self);
  void RegisterAsUnlocked(Thread* self);
  void CheckSafeToWait(Thread* self);

  friend class ScopedContentionRecorder;

  void RecordContention(uint64_t blocked_tid, uint64_t owner_tid, uint64_t nano_time_blocked);
  void DumpContention(std::ostream& os) const;

  const LockLevel level_;  // Support for lock hierarchy.
  const char* const name_;

  // A log entry that records contention but makes no guarantee that either tid will be held live.
  struct ContentionLogEntry {
    ContentionLogEntry() : blocked_tid(0), owner_tid(0) {}
    uint64_t blocked_tid;
    uint64_t owner_tid;
    AtomicInteger count;
  };
  struct ContentionLogData {
    ContentionLogEntry contention_log[kContentionLogSize];
    // The next entry in the contention log to be updated. Value ranges from 0 to
    // kContentionLogSize - 1.
    AtomicInteger cur_content_log_entry;
    // Number of times the Mutex has been contended.
    AtomicInteger contention_count;
    // Sum of time waited by all contenders in ns.
    volatile uint64_t wait_time;
    void AddToWaitTime(uint64_t value);
    ContentionLogData() : wait_time(0) {}
  };
  ContentionLogData contetion_log_data_[kContentionLogDataSize];

 public:
  bool HasEverContended() const {
    if (kLogLockContentions) {
      return contetion_log_data_->contention_count > 0;
    }
    return false;
  }
};

// A Mutex is used to achieve mutual exclusion between threads. A Mutex can be used to gain
// exclusive access to what it guards. A Mutex can be in one of two states:
// - Free - not owned by any thread,
// - Exclusive - owned by a single thread.
//
// The effect of locking and unlocking operations on the state is:
// State     | ExclusiveLock | ExclusiveUnlock
// -------------------------------------------
// Free      | Exclusive     | error
// Exclusive | Block*        | Free
// * Mutex is not reentrant and so an attempt to ExclusiveLock on the same thread will result in
//   an error. Being non-reentrant simplifies Waiting on ConditionVariables.
std::ostream& operator<<(std::ostream& os, const Mutex& mu);
class LOCKABLE Mutex : public BaseMutex {
 public:
  explicit Mutex(const char* name, LockLevel level = kDefaultMutexLevel, bool recursive = false);
  ~Mutex();

  virtual bool IsMutex() const { return true; }

  // Block until mutex is free then acquire exclusive access.
  void ExclusiveLock(Thread* self) EXCLUSIVE_LOCK_FUNCTION();
  void Lock(Thread* self) EXCLUSIVE_LOCK_FUNCTION() {  ExclusiveLock(self); }

  // Returns true if acquires exclusive access, false otherwise.
  bool ExclusiveTryLock(Thread* self) EXCLUSIVE_TRYLOCK_FUNCTION(true);
  bool TryLock(Thread* self) EXCLUSIVE_TRYLOCK_FUNCTION(true) { return ExclusiveTryLock(self); }

  // Release exclusive access.
  void ExclusiveUnlock(Thread* self) UNLOCK_FUNCTION();
  void Unlock(Thread* self) UNLOCK_FUNCTION() {  ExclusiveUnlock(self); }

  // Is the current thread the exclusive holder of the Mutex.
  bool IsExclusiveHeld(const Thread* self) const;

  // Assert that the Mutex is exclusively held by the current thread.
  void AssertExclusiveHeld(const Thread* self) {
    if (kDebugLocking && (gAborting == 0)) {
      CHECK(IsExclusiveHeld(self)) << *this;
    }
  }
  void AssertHeld(const Thread* self) { AssertExclusiveHeld(self); }

  // Assert that the Mutex is not held by the current thread.
  void AssertNotHeldExclusive(const Thread* self) {
    if (kDebugLocking && (gAborting == 0)) {
      CHECK(!IsExclusiveHeld(self)) << *this;
    }
  }
  void AssertNotHeld(const Thread* self) { AssertNotHeldExclusive(self); }

  // Id associated with exclusive owner.
  uint64_t GetExclusiveOwnerTid() const;

  // Returns how many times this Mutex has been locked, it is better to use AssertHeld/NotHeld.
  unsigned int GetDepth() const {
    return recursion_count_;
  }

  virtual void Dump(std::ostream& os) const;

 private:
#if ART_USE_FUTEXES
  // 0 is unheld, 1 is held.
  volatile int32_t state_;
  // Exclusive owner.
  volatile uint64_t exclusive_owner_;
  // Number of waiting contenders.
  volatile int32_t num_contenders_;
#else
  pthread_mutex_t mutex_;
#endif
  const bool recursive_;  // Can the lock be recursively held?
  unsigned int recursion_count_;
  friend class ConditionVariable;
  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

// A ReaderWriterMutex is used to achieve mutual exclusion between threads, similar to a Mutex.
// Unlike a Mutex a ReaderWriterMutex can be used to gain exclusive (writer) or shared (reader)
// access to what it guards. A flaw in relation to a Mutex is that it cannot be used with a
// condition variable. A ReaderWriterMutex can be in one of three states:
// - Free - not owned by any thread,
// - Exclusive - owned by a single thread,
// - Shared(n) - shared amongst n threads.
//
// The effect of locking and unlocking operations on the state is:
//
// State     | ExclusiveLock | ExclusiveUnlock | SharedLock       | SharedUnlock
// ----------------------------------------------------------------------------
// Free      | Exclusive     | error           | SharedLock(1)    | error
// Exclusive | Block         | Free            | Block            | error
// Shared(n) | Block         | error           | SharedLock(n+1)* | Shared(n-1) or Free
// * for large values of n the SharedLock may block.
std::ostream& operator<<(std::ostream& os, const ReaderWriterMutex& mu);
class LOCKABLE ReaderWriterMutex : public BaseMutex {
 public:
  explicit ReaderWriterMutex(const char* name, LockLevel level = kDefaultMutexLevel);
  ~ReaderWriterMutex();

  virtual bool IsReaderWriterMutex() const { return true; }

  // Block until ReaderWriterMutex is free then acquire exclusive access.
  void ExclusiveLock(Thread* self) EXCLUSIVE_LOCK_FUNCTION();
  void WriterLock(Thread* self) EXCLUSIVE_LOCK_FUNCTION() {  ExclusiveLock(self); }

  // Release exclusive access.
  void ExclusiveUnlock(Thread* self) UNLOCK_FUNCTION();
  void WriterUnlock(Thread* self) UNLOCK_FUNCTION() {  ExclusiveUnlock(self); }

  // Block until ReaderWriterMutex is free and acquire exclusive access. Returns true on success
  // or false if timeout is reached.
#if HAVE_TIMED_RWLOCK
  bool ExclusiveLockWithTimeout(Thread* self, int64_t ms, int32_t ns)
      EXCLUSIVE_TRYLOCK_FUNCTION(true);
#endif

  // Block until ReaderWriterMutex is shared or free then acquire a share on the access.
  void SharedLock(Thread* self) SHARED_LOCK_FUNCTION() ALWAYS_INLINE;
  void ReaderLock(Thread* self) SHARED_LOCK_FUNCTION() { SharedLock(self); }

  // Try to acquire share of ReaderWriterMutex.
  bool SharedTryLock(Thread* self) EXCLUSIVE_TRYLOCK_FUNCTION(true);

  // Release a share of the access.
  void SharedUnlock(Thread* self) UNLOCK_FUNCTION() ALWAYS_INLINE;
  void ReaderUnlock(Thread* self) UNLOCK_FUNCTION() { SharedUnlock(self); }

  // Is the current thread the exclusive holder of the ReaderWriterMutex.
  bool IsExclusiveHeld(const Thread* self) const;

  // Assert the current thread has exclusive access to the ReaderWriterMutex.
  void AssertExclusiveHeld(const Thread* self) {
    if (kDebugLocking && (gAborting == 0)) {
      CHECK(IsExclusiveHeld(self)) << *this;
    }
  }
  void AssertWriterHeld(const Thread* self) { AssertExclusiveHeld(self); }

  // Assert the current thread doesn't have exclusive access to the ReaderWriterMutex.
  void AssertNotExclusiveHeld(const Thread* self) {
    if (kDebugLocking && (gAborting == 0)) {
      CHECK(!IsExclusiveHeld(self)) << *this;
    }
  }
  void AssertNotWriterHeld(const Thread* self) { AssertNotExclusiveHeld(self); }

  // Is the current thread a shared holder of the ReaderWriterMutex.
  bool IsSharedHeld(const Thread* self) const;

  // Assert the current thread has shared access to the ReaderWriterMutex.
  void AssertSharedHeld(const Thread* self) {
    if (kDebugLocking && (gAborting == 0)) {
      // TODO: we can only assert this well when self != NULL.
      CHECK(IsSharedHeld(self) || self == NULL) << *this;
    }
  }
  void AssertReaderHeld(const Thread* self) { AssertSharedHeld(self); }

  // Assert the current thread doesn't hold this ReaderWriterMutex either in shared or exclusive
  // mode.
  void AssertNotHeld(const Thread* self) {
    if (kDebugLocking && (gAborting == 0)) {
      CHECK(!IsSharedHeld(self)) << *this;
    }
  }

  // Id associated with exclusive owner.
  uint64_t GetExclusiveOwnerTid() const;

  virtual void Dump(std::ostream& os) const;

 private:
#if ART_USE_FUTEXES
  // -1 implies held exclusive, +ve shared held by state_ many owners.
  volatile int32_t state_;
  // Exclusive owner.
  volatile uint64_t exclusive_owner_;
  // Pending readers.
  volatile int32_t num_pending_readers_;
  // Pending writers.
  volatile int32_t num_pending_writers_;
#else
  pthread_rwlock_t rwlock_;
#endif
  DISALLOW_COPY_AND_ASSIGN(ReaderWriterMutex);
};

// ConditionVariables allow threads to queue and sleep. Threads may then be resumed individually
// (Signal) or all at once (Broadcast).
class ConditionVariable {
 public:
  explicit ConditionVariable(const char* name, Mutex& mutex);
  ~ConditionVariable();

  void Broadcast(Thread* self);
  void Signal(Thread* self);
  // TODO: No thread safety analysis on Wait and TimedWait as they call mutex operations via their
  //       pointer copy, thereby defeating annotalysis.
  void Wait(Thread* self) NO_THREAD_SAFETY_ANALYSIS;
  void TimedWait(Thread* self, int64_t ms, int32_t ns) NO_THREAD_SAFETY_ANALYSIS;
  // Variant of Wait that should be used with caution. Doesn't validate that no mutexes are held
  // when waiting.
  // TODO: remove this.
  void WaitHoldingLocks(Thread* self) NO_THREAD_SAFETY_ANALYSIS;

 private:
  const char* const name_;
  // The Mutex being used by waiters. It is an error to mix condition variables between different
  // Mutexes.
  Mutex& guard_;
#if ART_USE_FUTEXES
  // A counter that is modified by signals and broadcasts. This ensures that when a waiter gives up
  // their Mutex and another thread takes it and signals, the waiting thread observes that sequence_
  // changed and doesn't enter the wait. Modified while holding guard_, but is read by futex wait
  // without guard_ held.
  volatile int32_t sequence_;
  // Number of threads that have come into to wait, not the length of the waiters on the futex as
  // waiters may have been requeued onto guard_. Guarded by guard_.
  volatile int32_t num_waiters_;
#else
  pthread_cond_t cond_;
#endif
  DISALLOW_COPY_AND_ASSIGN(ConditionVariable);
};

// Scoped locker/unlocker for a regular Mutex that acquires mu upon construction and releases it
// upon destruction.
class SCOPED_LOCKABLE MutexLock {
 public:
  explicit MutexLock(Thread* self, Mutex& mu) EXCLUSIVE_LOCK_FUNCTION(mu) : self_(self), mu_(mu) {
    mu_.ExclusiveLock(self_);
  }

  ~MutexLock() UNLOCK_FUNCTION() {
    mu_.ExclusiveUnlock(self_);
  }

 private:
  Thread* const self_;
  Mutex& mu_;
  DISALLOW_COPY_AND_ASSIGN(MutexLock);
};
// Catch bug where variable name is omitted. "MutexLock (lock);" instead of "MutexLock mu(lock)".
#define MutexLock(x) COMPILE_ASSERT(0, mutex_lock_declaration_missing_variable_name)

// Scoped locker/unlocker for a ReaderWriterMutex that acquires read access to mu upon
// construction and releases it upon destruction.
class SCOPED_LOCKABLE ReaderMutexLock {
 public:
  explicit ReaderMutexLock(Thread* self, ReaderWriterMutex& mu) EXCLUSIVE_LOCK_FUNCTION(mu) :
      self_(self), mu_(mu) {
    mu_.SharedLock(self_);
  }

  ~ReaderMutexLock() UNLOCK_FUNCTION() {
    mu_.SharedUnlock(self_);
  }

 private:
  Thread* const self_;
  ReaderWriterMutex& mu_;
  DISALLOW_COPY_AND_ASSIGN(ReaderMutexLock);
};
// Catch bug where variable name is omitted. "ReaderMutexLock (lock);" instead of
// "ReaderMutexLock mu(lock)".
#define ReaderMutexLock(x) COMPILE_ASSERT(0, reader_mutex_lock_declaration_missing_variable_name)

// Scoped locker/unlocker for a ReaderWriterMutex that acquires write access to mu upon
// construction and releases it upon destruction.
class SCOPED_LOCKABLE WriterMutexLock {
 public:
  explicit WriterMutexLock(Thread* self, ReaderWriterMutex& mu) EXCLUSIVE_LOCK_FUNCTION(mu) :
      self_(self), mu_(mu) {
    mu_.ExclusiveLock(self_);
  }

  ~WriterMutexLock() UNLOCK_FUNCTION() {
    mu_.ExclusiveUnlock(self_);
  }

 private:
  Thread* const self_;
  ReaderWriterMutex& mu_;
  DISALLOW_COPY_AND_ASSIGN(WriterMutexLock);
};
// Catch bug where variable name is omitted. "WriterMutexLock (lock);" instead of
// "WriterMutexLock mu(lock)".
#define WriterMutexLock(x) COMPILE_ASSERT(0, writer_mutex_lock_declaration_missing_variable_name)

}  // namespace art

#endif  // ART_RUNTIME_BASE_MUTEX_H_
