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

#ifndef ART_RUNTIME_LOCKS_H_
#define ART_RUNTIME_LOCKS_H_

#include <ostream>

#include "base/macros.h"

namespace art {

class LOCKABLE Mutex;
class LOCKABLE ReaderWriterMutex;

// LockLevel is used to impose a lock hierarchy [1] where acquisition of a Mutex at a higher or
// equal level to a lock a thread holds is invalid. The lock hierarchy achieves a cycle free
// partial ordering and thereby cause deadlock situations to fail checks.
//
// [1] http://www.drdobbs.com/parallel/use-lock-hierarchies-to-avoid-deadlock/204801163
enum LockLevel {
  kLoggingLock = 0,
  kUnexpectedSignalLock,
  kThreadSuspendCountLock,
  kAbortLock,
  kJdwpSocketLock,
  kAllocSpaceLock,
  kMarkSweepMarkStackLock,
  kDefaultMutexLevel,
  kMarkSweepLargeObjectLock,
  kPinTableLock,
  kLoadLibraryLock,
  kJdwpObjectRegistryLock,
  kClassLinkerClassesLock,
  kBreakpointLock,
  kThreadListLock,
  kBreakpointInvokeLock,
  kTraceLock,
  kJdwpEventListLock,
  kJdwpAttachLock,
  kJdwpStartLock,
  kRuntimeShutdownLock,
  kHeapBitmapLock,
  kMonitorLock,
  kMutatorLock,
  kZygoteCreationLock,

  kLockLevelCount  // Must come last.
};
std::ostream& operator<<(std::ostream& os, const LockLevel& rhs);

// Global mutexes corresponding to the levels above.
class Locks {
 public:
  static void Init();

  // The mutator_lock_ is used to allow mutators to execute in a shared (reader) mode or to block
  // mutators by having an exclusive (writer) owner. In normal execution each mutator thread holds
  // a share on the mutator_lock_. The garbage collector may also execute with shared access but
  // at times requires exclusive access to the heap (not to be confused with the heap meta-data
  // guarded by the heap_lock_ below). When the garbage collector requires exclusive access it asks
  // the mutators to suspend themselves which also involves usage of the thread_suspend_count_lock_
  // to cover weaknesses in using ReaderWriterMutexes with ConditionVariables. We use a condition
  // variable to wait upon in the suspension logic as releasing and then re-acquiring a share on
  // the mutator lock doesn't necessarily allow the exclusive user (e.g the garbage collector)
  // chance to acquire the lock.
  //
  // Thread suspension:
  // Shared users                                  | Exclusive user
  // (holding mutator lock and in kRunnable state) |   .. running ..
  //   .. running ..                               | Request thread suspension by:
  //   .. running ..                               |   - acquiring thread_suspend_count_lock_
  //   .. running ..                               |   - incrementing Thread::suspend_count_ on
  //   .. running ..                               |     all mutator threads
  //   .. running ..                               |   - releasing thread_suspend_count_lock_
  //   .. running ..                               | Block trying to acquire exclusive mutator lock
  // Poll Thread::suspend_count_ and enter full    |   .. blocked ..
  // suspend code.                                 |   .. blocked ..
  // Change state to kSuspended                    |   .. blocked ..
  // x: Release share on mutator_lock_             | Carry out exclusive access
  // Acquire thread_suspend_count_lock_            |   .. exclusive ..
  // while Thread::suspend_count_ > 0              |   .. exclusive ..
  //   - wait on Thread::resume_cond_              |   .. exclusive ..
  //     (releases thread_suspend_count_lock_)     |   .. exclusive ..
  //   .. waiting ..                               | Release mutator_lock_
  //   .. waiting ..                               | Request thread resumption by:
  //   .. waiting ..                               |   - acquiring thread_suspend_count_lock_
  //   .. waiting ..                               |   - decrementing Thread::suspend_count_ on
  //   .. waiting ..                               |     all mutator threads
  //   .. waiting ..                               |   - notifying on Thread::resume_cond_
  //    - re-acquire thread_suspend_count_lock_    |   - releasing thread_suspend_count_lock_
  // Release thread_suspend_count_lock_            |  .. running ..
  // Acquire share on mutator_lock_                |  .. running ..
  //  - This could block but the thread still      |  .. running ..
  //    has a state of kSuspended and so this      |  .. running ..
  //    isn't an issue.                            |  .. running ..
  // Acquire thread_suspend_count_lock_            |  .. running ..
  //  - we poll here as we're transitioning into   |  .. running ..
  //    kRunnable and an individual thread suspend |  .. running ..
  //    request (e.g for debugging) won't try      |  .. running ..
  //    to acquire the mutator lock (which would   |  .. running ..
  //    block as we hold the mutator lock). This   |  .. running ..
  //    poll ensures that if the suspender thought |  .. running ..
  //    we were suspended by incrementing our      |  .. running ..
  //    Thread::suspend_count_ and then reading    |  .. running ..
  //    our state we go back to waiting on         |  .. running ..
  //    Thread::resume_cond_.                      |  .. running ..
  // can_go_runnable = Thread::suspend_count_ == 0 |  .. running ..
  // Release thread_suspend_count_lock_            |  .. running ..
  // if can_go_runnable                            |  .. running ..
  //   Change state to kRunnable                   |  .. running ..
  // else                                          |  .. running ..
  //   Goto x                                      |  .. running ..
  //  .. running ..                                |  .. running ..
  static ReaderWriterMutex* mutator_lock_;

  // Allow reader-writer mutual exclusion on the mark and live bitmaps of the heap.
  static ReaderWriterMutex* heap_bitmap_lock_ ACQUIRED_AFTER(mutator_lock_);

  // Guards shutdown of the runtime.
  static Mutex* runtime_shutdown_lock_ ACQUIRED_AFTER(heap_bitmap_lock_);

  // The thread_list_lock_ guards ThreadList::list_. It is also commonly held to stop threads
  // attaching and detaching.
  static Mutex* thread_list_lock_ ACQUIRED_AFTER(runtime_shutdown_lock_);

  // Guards breakpoints and single-stepping.
  static Mutex* breakpoint_lock_ ACQUIRED_AFTER(thread_list_lock_);

  // Guards trace requests.
  static Mutex* trace_lock_ ACQUIRED_AFTER(breakpoint_lock_);

  // Guards lists of classes within the class linker.
  static ReaderWriterMutex* classlinker_classes_lock_ ACQUIRED_AFTER(trace_lock_);

  // When declaring any Mutex add DEFAULT_MUTEX_ACQUIRED_AFTER to use annotalysis to check the code
  // doesn't try to hold a higher level Mutex.
  #define DEFAULT_MUTEX_ACQUIRED_AFTER ACQUIRED_AFTER(classlinker_classes_lock_)

  // Have an exclusive aborting thread.
  static Mutex* abort_lock_ ACQUIRED_AFTER(classlinker_classes_lock_);

  // Allow mutual exclusion when manipulating Thread::suspend_count_.
  // TODO: Does the trade-off of a per-thread lock make sense?
  static Mutex* thread_suspend_count_lock_ ACQUIRED_AFTER(abort_lock_);

  // One unexpected signal at a time lock.
  static Mutex* unexpected_signal_lock_ ACQUIRED_AFTER(thread_suspend_count_lock_);

  // Have an exclusive logging thread.
  static Mutex* logging_lock_ ACQUIRED_AFTER(unexpected_signal_lock_);
};

}  // namespace art

#endif  // ART_RUNTIME_LOCKS_H_
