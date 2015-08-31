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

#ifndef ART_RUNTIME_MONITOR_H_
#define ART_RUNTIME_MONITOR_H_

#include <pthread.h>
#include <stdint.h>

#include <iosfwd>
#include <list>
#include <vector>

#include "base/mutex.h"
#include "root_visitor.h"
#include "thread_state.h"

namespace art {

/*
 * Monitor shape field. Used to distinguish thin locks from fat locks.
 */
#define LW_SHAPE_THIN 0
#define LW_SHAPE_FAT 1

/*
 * Hash state field.  Used to signify that an object has had its
 * identity hash code exposed or relocated.
 */
#define LW_HASH_STATE_UNHASHED 0
#define LW_HASH_STATE_HASHED 1
#define LW_HASH_STATE_HASHED_AND_MOVED 3
#define LW_HASH_STATE_MASK 0x3
#define LW_HASH_STATE_SHIFT 1
#define LW_HASH_STATE(x) (((x) >> LW_HASH_STATE_SHIFT) & LW_HASH_STATE_MASK)

/*
 * Lock owner field.  Contains the thread id of the thread currently
 * holding the lock.
 */
#define LW_LOCK_OWNER_MASK 0xffff
#define LW_LOCK_OWNER_SHIFT 3
#define LW_LOCK_OWNER(x) (((x) >> LW_LOCK_OWNER_SHIFT) & LW_LOCK_OWNER_MASK)

namespace mirror {
  class ArtMethod;
  class Object;
}  // namespace mirror
class Thread;
class StackVisitor;

class Monitor {
 public:
  ~Monitor();

  static bool IsSensitiveThread();
  static void Init(uint32_t lock_profiling_threshold, bool (*is_sensitive_thread_hook)());

  static uint32_t GetThinLockId(uint32_t raw_lock_word)
      NO_THREAD_SAFETY_ANALYSIS;  // Reading lock owner without holding lock is racy.

  static void MonitorEnter(Thread* thread, mirror::Object* obj)
      EXCLUSIVE_LOCK_FUNCTION(monitor_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static bool MonitorExit(Thread* thread, mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      UNLOCK_FUNCTION(monitor_lock_);

  static void Notify(Thread* self, mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void NotifyAll(Thread* self, mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void Wait(Thread* self, mirror::Object* obj, int64_t ms, int32_t ns,
                   bool interruptShouldThrow, ThreadState why)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void DescribeWait(std::ostream& os, const Thread* thread)
      LOCKS_EXCLUDED(Locks::thread_suspend_count_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Used to implement JDWP's ThreadReference.CurrentContendedMonitor.
  static mirror::Object* GetContendedMonitor(Thread* thread);

  // Calls 'callback' once for each lock held in the single stack frame represented by
  // the current state of 'stack_visitor'.
  static void VisitLocks(StackVisitor* stack_visitor, void (*callback)(mirror::Object*, void*),
                         void* callback_context)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool IsValidLockWord(int32_t lock_word);

  mirror::Object* GetObject();

 private:
  explicit Monitor(Thread* owner, mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void AppendToWaitSet(Thread* thread) EXCLUSIVE_LOCKS_REQUIRED(monitor_lock_);
  void RemoveFromWaitSet(Thread* thread) EXCLUSIVE_LOCKS_REQUIRED(monitor_lock_);

  static void Inflate(Thread* self, mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void LogContentionEvent(Thread* self, uint32_t wait_ms, uint32_t sample_percent,
                          const char* owner_filename, uint32_t owner_line_number)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void FailedUnlock(mirror::Object* obj, Thread* expected_owner, Thread* found_owner, Monitor* mon)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Lock(Thread* self) EXCLUSIVE_LOCK_FUNCTION(monitor_lock_);
  bool Unlock(Thread* thread, bool for_wait) UNLOCK_FUNCTION(monitor_lock_);

  void Notify(Thread* self) NO_THREAD_SAFETY_ANALYSIS;
  void NotifyWithLock(Thread* self)
      EXCLUSIVE_LOCKS_REQUIRED(monitor_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void NotifyAll(Thread* self) NO_THREAD_SAFETY_ANALYSIS;
  void NotifyAllWithLock()
      EXCLUSIVE_LOCKS_REQUIRED(monitor_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);


  void Wait(Thread* self, int64_t msec, int32_t nsec, bool interruptShouldThrow, ThreadState why)
      NO_THREAD_SAFETY_ANALYSIS;
  void WaitWithLock(Thread* self, int64_t ms, int32_t ns, bool interruptShouldThrow, ThreadState why)
      EXCLUSIVE_LOCKS_REQUIRED(monitor_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Translates the provided method and pc into its declaring class' source file and line number.
  void TranslateLocation(const mirror::ArtMethod* method, uint32_t pc,
                         const char*& source_file, uint32_t& line_number) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool (*is_sensitive_thread_hook_)();
  static uint32_t lock_profiling_threshold_;

  Mutex monitor_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  // Which thread currently owns the lock?
  Thread* volatile owner_;

  // Owner's recursive lock depth.
  int lock_count_ GUARDED_BY(monitor_lock_);

  // What object are we part of (for debugging).
  mirror::Object* const obj_;

  // Threads currently waiting on this monitor.
  Thread* wait_set_ GUARDED_BY(monitor_lock_);

  // Method and dex pc where the lock owner acquired the lock, used when lock
  // sampling is enabled. locking_method_ may be null if the lock is currently
  // unlocked, or if the lock is acquired by the system when the stack is empty.
  const mirror::ArtMethod* locking_method_ GUARDED_BY(monitor_lock_);
  uint32_t locking_dex_pc_ GUARDED_BY(monitor_lock_);

  friend class MonitorInfo;
  friend class MonitorList;
  friend class mirror::Object;
  DISALLOW_COPY_AND_ASSIGN(Monitor);
};

class MonitorList {
 public:
  MonitorList();
  ~MonitorList();

  void Add(Monitor* m);
  void SweepMonitorList(IsMarkedTester is_marked, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);
  void DisallowNewMonitors();
  void AllowNewMonitors();
 private:
  bool allow_new_monitors_ GUARDED_BY(monitor_list_lock_);
  Mutex monitor_list_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable monitor_add_condition_ GUARDED_BY(monitor_list_lock_);
  std::list<Monitor*> list_ GUARDED_BY(monitor_list_lock_);

  friend class Monitor;
  DISALLOW_COPY_AND_ASSIGN(MonitorList);
};

// Collects information about the current state of an object's monitor.
// This is very unsafe, and must only be called when all threads are suspended.
// For use only by the JDWP implementation.
class MonitorInfo {
 public:
  explicit MonitorInfo(mirror::Object* o) EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);

  Thread* owner;
  size_t entry_count;
  std::vector<Thread*> waiters;

 private:
  DISALLOW_COPY_AND_ASSIGN(MonitorInfo);
};

}  // namespace art

#endif  // ART_RUNTIME_MONITOR_H_
