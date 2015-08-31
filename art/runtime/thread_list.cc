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

#include "thread_list.h"

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/mutex.h"
#include "base/timing_logger.h"
#include "debugger.h"
#include "thread.h"
#include "utils.h"

namespace art {

ThreadList::ThreadList()
    : allocated_ids_lock_("allocated thread ids lock"),
      suspend_all_count_(0), debug_suspend_all_count_(0),
      thread_exit_cond_("thread exit condition variable", *Locks::thread_list_lock_) {
}

ThreadList::~ThreadList() {
  // Detach the current thread if necessary. If we failed to start, there might not be any threads.
  // We need to detach the current thread here in case there's another thread waiting to join with
  // us.
  if (Contains(Thread::Current())) {
    Runtime::Current()->DetachCurrentThread();
  }

  WaitForOtherNonDaemonThreadsToExit();
  // TODO: there's an unaddressed race here where a thread may attach during shutdown, see
  //       Thread::Init.
  SuspendAllDaemonThreads();
}

bool ThreadList::Contains(Thread* thread) {
  return find(list_.begin(), list_.end(), thread) != list_.end();
}

bool ThreadList::Contains(pid_t tid) {
  for (const auto& thread : list_) {
    if (thread->tid_ == tid) {
      return true;
    }
  }
  return false;
}

pid_t ThreadList::GetLockOwner() {
  return Locks::thread_list_lock_->GetExclusiveOwnerTid();
}

void ThreadList::DumpForSigQuit(std::ostream& os) {
  {
    MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
    DumpLocked(os);
  }
  DumpUnattachedThreads(os);
}

static void DumpUnattachedThread(std::ostream& os, pid_t tid) NO_THREAD_SAFETY_ANALYSIS {
  // TODO: No thread safety analysis as DumpState with a NULL thread won't access fields, should
  // refactor DumpState to avoid skipping analysis.
  Thread::DumpState(os, NULL, tid);
  DumpKernelStack(os, tid, "  kernel: ", false);
  // TODO: Reenable this when the native code in system_server can handle it.
  // Currently "adb shell kill -3 `pid system_server`" will cause it to exit.
  if (false) {
    DumpNativeStack(os, tid, "  native: ", false);
  }
  os << "\n";
}

void ThreadList::DumpUnattachedThreads(std::ostream& os) {
  DIR* d = opendir("/proc/self/task");
  if (!d) {
    return;
  }

  Thread* self = Thread::Current();
  dirent* e;
  while ((e = readdir(d)) != NULL) {
    char* end;
    pid_t tid = strtol(e->d_name, &end, 10);
    if (!*end) {
      bool contains;
      {
        MutexLock mu(self, *Locks::thread_list_lock_);
        contains = Contains(tid);
      }
      if (!contains) {
        DumpUnattachedThread(os, tid);
      }
    }
  }
  closedir(d);
}

void ThreadList::DumpLocked(std::ostream& os) {
  os << "DALVIK THREADS (" << list_.size() << "):\n";
  for (const auto& thread : list_) {
    thread->Dump(os);
    os << "\n";
  }
}

void ThreadList::AssertThreadsAreSuspended(Thread* self, Thread* ignore1, Thread* ignore2) {
  MutexLock mu(self, *Locks::thread_list_lock_);
  MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
  for (const auto& thread : list_) {
    if (thread != ignore1 && thread != ignore2) {
      CHECK(thread->IsSuspended())
            << "\nUnsuspended thread: <<" << *thread << "\n"
            << "self: <<" << *Thread::Current();
    }
  }
}

#if HAVE_TIMED_RWLOCK
// Attempt to rectify locks so that we dump thread list with required locks before exiting.
static void UnsafeLogFatalForThreadSuspendAllTimeout(Thread* self) NO_THREAD_SAFETY_ANALYSIS {
  Runtime* runtime = Runtime::Current();
  std::ostringstream ss;
  ss << "Thread suspend timeout\n";
  runtime->DumpLockHolders(ss);
  ss << "\n";
  runtime->GetThreadList()->DumpLocked(ss);
  LOG(FATAL) << ss.str();
}
#endif

size_t ThreadList::RunCheckpoint(Closure* checkpoint_function) {
  Thread* self = Thread::Current();
  if (kIsDebugBuild) {
    Locks::mutator_lock_->AssertNotExclusiveHeld(self);
    Locks::thread_list_lock_->AssertNotHeld(self);
    Locks::thread_suspend_count_lock_->AssertNotHeld(self);
    CHECK_NE(self->GetState(), kRunnable);
  }

  std::vector<Thread*> suspended_count_modified_threads;
  size_t count = 0;
  {
    // Call a checkpoint function for each thread, threads which are suspend get their checkpoint
    // manually called.
    MutexLock mu(self, *Locks::thread_list_lock_);
    for (const auto& thread : list_) {
      if (thread != self) {
        for (;;) {
          if (thread->RequestCheckpoint(checkpoint_function)) {
            // This thread will run it's checkpoint some time in the near future.
            count++;
            break;
          } else {
            // We are probably suspended, try to make sure that we stay suspended.
            MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
            // The thread switched back to runnable.
            if (thread->GetState() == kRunnable) {
              continue;
            }
            thread->ModifySuspendCount(self, +1, false);
            suspended_count_modified_threads.push_back(thread);
            break;
          }
        }
      }
    }
  }

  // Run the checkpoint on ourself while we wait for threads to suspend.
  checkpoint_function->Run(self);

  // Run the checkpoint on the suspended threads.
  for (const auto& thread : suspended_count_modified_threads) {
    if (!thread->IsSuspended()) {
      // Wait until the thread is suspended.
      uint64_t start = NanoTime();
      do {
        // Sleep for 100us.
        usleep(100);
      } while (!thread->IsSuspended());
      uint64_t end = NanoTime();
      // Shouldn't need to wait for longer than 1 millisecond.
      const uint64_t threshold = 1;
      if (NsToMs(end - start) > threshold) {
        LOG(INFO) << "Warning: waited longer than " << threshold
                  << " ms for thread suspend\n";
      }
    }
    // We know for sure that the thread is suspended at this point.
    thread->RunCheckpointFunction();
    {
      MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
      thread->ModifySuspendCount(self, -1, false);
    }
  }

  {
    // Imitate ResumeAll, threads may be waiting on Thread::resume_cond_ since we raised their
    // suspend count. Now the suspend_count_ is lowered so we must do the broadcast.
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    Thread::resume_cond_->Broadcast(self);
  }

  // Add one for self.
  return count + suspended_count_modified_threads.size() + 1;
}

void ThreadList::SuspendAll() {
  Thread* self = Thread::Current();

  VLOG(threads) << *self << " SuspendAll starting...";

  if (kIsDebugBuild) {
    Locks::mutator_lock_->AssertNotHeld(self);
    Locks::thread_list_lock_->AssertNotHeld(self);
    Locks::thread_suspend_count_lock_->AssertNotHeld(self);
    CHECK_NE(self->GetState(), kRunnable);
  }
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    {
      MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
      // Update global suspend all state for attaching threads.
      ++suspend_all_count_;
      // Increment everybody's suspend count (except our own).
      for (const auto& thread : list_) {
        if (thread == self) {
          continue;
        }
        VLOG(threads) << "requesting thread suspend: " << *thread;
        thread->ModifySuspendCount(self, +1, false);
      }
    }
  }

  // Block on the mutator lock until all Runnable threads release their share of access.
#if HAVE_TIMED_RWLOCK
  // Timeout if we wait more than 30 seconds.
  if (UNLIKELY(!Locks::mutator_lock_->ExclusiveLockWithTimeout(self, 30 * 1000, 0))) {
    UnsafeLogFatalForThreadSuspendAllTimeout(self);
  }
#else
  Locks::mutator_lock_->ExclusiveLock(self);
#endif

  // Debug check that all threads are suspended.
  AssertThreadsAreSuspended(self, self);

  VLOG(threads) << *self << " SuspendAll complete";
}

void ThreadList::ResumeAll() {
  Thread* self = Thread::Current();

  VLOG(threads) << *self << " ResumeAll starting";

  // Debug check that all threads are suspended.
  AssertThreadsAreSuspended(self, self);

  Locks::mutator_lock_->ExclusiveUnlock(self);
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    // Update global suspend all state for attaching threads.
    --suspend_all_count_;
    // Decrement the suspend counts for all threads.
    for (const auto& thread : list_) {
      if (thread == self) {
        continue;
      }
      thread->ModifySuspendCount(self, -1, false);
    }

    // Broadcast a notification to all suspended threads, some or all of
    // which may choose to wake up.  No need to wait for them.
    VLOG(threads) << *self << " ResumeAll waking others";
    Thread::resume_cond_->Broadcast(self);
  }
  VLOG(threads) << *self << " ResumeAll complete";
}

void ThreadList::Resume(Thread* thread, bool for_debugger) {
  Thread* self = Thread::Current();
  DCHECK_NE(thread, self);
  VLOG(threads) << "Resume(" << *thread << ") starting..." << (for_debugger ? " (debugger)" : "");

  {
    // To check Contains.
    MutexLock mu(self, *Locks::thread_list_lock_);
    // To check IsSuspended.
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    DCHECK(thread->IsSuspended());
    if (!Contains(thread)) {
      return;
    }
    thread->ModifySuspendCount(self, -1, for_debugger);
  }

  {
    VLOG(threads) << "Resume(" << *thread << ") waking others";
    MutexLock mu(self, *Locks::thread_suspend_count_lock_);
    Thread::resume_cond_->Broadcast(self);
  }

  VLOG(threads) << "Resume(" << *thread << ") complete";
}

void ThreadList::SuspendAllForDebugger() {
  Thread* self = Thread::Current();
  Thread* debug_thread = Dbg::GetDebugThread();

  VLOG(threads) << *self << " SuspendAllForDebugger starting...";

  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    {
      MutexLock mu(self, *Locks::thread_suspend_count_lock_);
      // Update global suspend all state for attaching threads.
      ++suspend_all_count_;
      ++debug_suspend_all_count_;
      // Increment everybody's suspend count (except our own).
      for (const auto& thread : list_) {
        if (thread == self || thread == debug_thread) {
          continue;
        }
        VLOG(threads) << "requesting thread suspend: " << *thread;
        thread->ModifySuspendCount(self, +1, true);
      }
    }
  }

  // Block on the mutator lock until all Runnable threads release their share of access then
  // immediately unlock again.
#if HAVE_TIMED_RWLOCK
  // Timeout if we wait more than 30 seconds.
  if (!Locks::mutator_lock_->ExclusiveLockWithTimeout(self, 30 * 1000, 0)) {
    UnsafeLogFatalForThreadSuspendAllTimeout(self);
  } else {
    Locks::mutator_lock_->ExclusiveUnlock(self);
  }
#else
  Locks::mutator_lock_->ExclusiveLock(self);
  Locks::mutator_lock_->ExclusiveUnlock(self);
#endif
  AssertThreadsAreSuspended(self, self, debug_thread);

  VLOG(threads) << *self << " SuspendAll complete";
}

void ThreadList::SuspendSelfForDebugger() {
  Thread* self = Thread::Current();

  // The debugger thread must not suspend itself due to debugger activity!
  Thread* debug_thread = Dbg::GetDebugThread();
  CHECK(debug_thread != NULL);
  CHECK(self != debug_thread);
  CHECK_NE(self->GetState(), kRunnable);
  Locks::mutator_lock_->AssertNotHeld(self);

  {
    // Collisions with other suspends aren't really interesting. We want
    // to ensure that we're the only one fiddling with the suspend count
    // though.
    MutexLock mu(self, *Locks::thread_suspend_count_lock_);
    self->ModifySuspendCount(self, +1, true);
    CHECK_GT(self->suspend_count_, 0);
  }

  VLOG(threads) << *self << " self-suspending (debugger)";

  // Tell JDWP that we've completed suspension. The JDWP thread can't
  // tell us to resume before we're fully asleep because we hold the
  // suspend count lock.
  Dbg::ClearWaitForEventThread();

  {
    MutexLock mu(self, *Locks::thread_suspend_count_lock_);
    while (self->suspend_count_ != 0) {
      Thread::resume_cond_->Wait(self);
      if (self->suspend_count_ != 0) {
        // The condition was signaled but we're still suspended. This
        // can happen if the debugger lets go while a SIGQUIT thread
        // dump event is pending (assuming SignalCatcher was resumed for
        // just long enough to try to grab the thread-suspend lock).
        LOG(DEBUG) << *self << " still suspended after undo "
                   << "(suspend count=" << self->suspend_count_ << ")";
      }
    }
    CHECK_EQ(self->suspend_count_, 0);
  }

  VLOG(threads) << *self << " self-reviving (debugger)";
}

void ThreadList::UndoDebuggerSuspensions() {
  Thread* self = Thread::Current();

  VLOG(threads) << *self << " UndoDebuggerSuspensions starting";

  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    // Update global suspend all state for attaching threads.
    suspend_all_count_ -= debug_suspend_all_count_;
    debug_suspend_all_count_ = 0;
    // Update running threads.
    for (const auto& thread : list_) {
      if (thread == self || thread->debug_suspend_count_ == 0) {
        continue;
      }
      thread->ModifySuspendCount(self, -thread->debug_suspend_count_, true);
    }
  }

  {
    MutexLock mu(self, *Locks::thread_suspend_count_lock_);
    Thread::resume_cond_->Broadcast(self);
  }

  VLOG(threads) << "UndoDebuggerSuspensions(" << *self << ") complete";
}

void ThreadList::WaitForOtherNonDaemonThreadsToExit() {
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  bool all_threads_are_daemons;
  do {
    {
      // No more threads can be born after we start to shutdown.
      MutexLock mu(self, *Locks::runtime_shutdown_lock_);
      CHECK(Runtime::Current()->IsShuttingDown());
      CHECK_EQ(Runtime::Current()->NumberOfThreadsBeingBorn(), 0U);
    }
    all_threads_are_daemons = true;
    MutexLock mu(self, *Locks::thread_list_lock_);
    for (const auto& thread : list_) {
      if (thread != self && !thread->IsDaemon()) {
        all_threads_are_daemons = false;
        break;
      }
    }
    if (!all_threads_are_daemons) {
      // Wait for another thread to exit before re-checking.
      thread_exit_cond_.Wait(self);
    }
  } while (!all_threads_are_daemons);
}

void ThreadList::SuspendAllDaemonThreads() {
  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::thread_list_lock_);
  {  // Tell all the daemons it's time to suspend.
    MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
    for (const auto& thread : list_) {
      // This is only run after all non-daemon threads have exited, so the remainder should all be
      // daemons.
      CHECK(thread->IsDaemon()) << *thread;
      if (thread != self) {
        thread->ModifySuspendCount(self, +1, false);
      }
    }
  }
  // Give the threads a chance to suspend, complaining if they're slow.
  bool have_complained = false;
  for (int i = 0; i < 10; ++i) {
    usleep(200 * 1000);
    bool all_suspended = true;
    for (const auto& thread : list_) {
      if (thread != self && thread->GetState() == kRunnable) {
        if (!have_complained) {
          LOG(WARNING) << "daemon thread not yet suspended: " << *thread;
          have_complained = true;
        }
        all_suspended = false;
      }
    }
    if (all_suspended) {
      return;
    }
  }
  LOG(ERROR) << "suspend all daemons failed";
}
void ThreadList::Register(Thread* self) {
  DCHECK_EQ(self, Thread::Current());

  if (VLOG_IS_ON(threads)) {
    std::ostringstream oss;
    self->ShortDump(oss);  // We don't hold the mutator_lock_ yet and so cannot call Dump.
    LOG(INFO) << "ThreadList::Register() " << *self  << "\n" << oss;
  }

  // Atomically add self to the thread list and make its thread_suspend_count_ reflect ongoing
  // SuspendAll requests.
  MutexLock mu(self, *Locks::thread_list_lock_);
  MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
  self->suspend_count_ = suspend_all_count_;
  self->debug_suspend_count_ = debug_suspend_all_count_;
  if (self->suspend_count_ > 0) {
    self->AtomicSetFlag(kSuspendRequest);
  }
  CHECK(!Contains(self));
  list_.push_back(self);
}

void ThreadList::Unregister(Thread* self) {
  DCHECK_EQ(self, Thread::Current());

  VLOG(threads) << "ThreadList::Unregister() " << *self;

  // Any time-consuming destruction, plus anything that can call back into managed code or
  // suspend and so on, must happen at this point, and not in ~Thread.
  self->Destroy();

  uint32_t thin_lock_id = self->thin_lock_id_;
  self->thin_lock_id_ = 0;
  ReleaseThreadId(self, thin_lock_id);
  while (self != NULL) {
    // Remove and delete the Thread* while holding the thread_list_lock_ and
    // thread_suspend_count_lock_ so that the unregistering thread cannot be suspended.
    // Note: deliberately not using MutexLock that could hold a stale self pointer.
    Locks::thread_list_lock_->ExclusiveLock(self);
    CHECK(Contains(self));
    // Note: we don't take the thread_suspend_count_lock_ here as to be suspending a thread other
    // than yourself you need to hold the thread_list_lock_ (see Thread::ModifySuspendCount).
    if (!self->IsSuspended()) {
      list_.remove(self);
      delete self;
      self = NULL;
    }
    Locks::thread_list_lock_->ExclusiveUnlock(self);
  }

  // Clear the TLS data, so that the underlying native thread is recognizably detached.
  // (It may wish to reattach later.)
  CHECK_PTHREAD_CALL(pthread_setspecific, (Thread::pthread_key_self_, NULL), "detach self");

  // Signal that a thread just detached.
  MutexLock mu(NULL, *Locks::thread_list_lock_);
  thread_exit_cond_.Signal(NULL);
}

void ThreadList::ForEach(void (*callback)(Thread*, void*), void* context) {
  for (const auto& thread : list_) {
    callback(thread, context);
  }
}

void ThreadList::VisitRoots(RootVisitor* visitor, void* arg) const {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  for (const auto& thread : list_) {
    thread->VisitRoots(visitor, arg);
  }
}

void ThreadList::VerifyRoots(VerifyRootVisitor* visitor, void* arg) const {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  for (const auto& thread : list_) {
    thread->VerifyRoots(visitor, arg);
  }
}

uint32_t ThreadList::AllocThreadId(Thread* self) {
  MutexLock mu(self, allocated_ids_lock_);
  for (size_t i = 0; i < allocated_ids_.size(); ++i) {
    if (!allocated_ids_[i]) {
      allocated_ids_.set(i);
      return i + 1;  // Zero is reserved to mean "invalid".
    }
  }
  LOG(FATAL) << "Out of internal thread ids";
  return 0;
}

void ThreadList::ReleaseThreadId(Thread* self, uint32_t id) {
  MutexLock mu(self, allocated_ids_lock_);
  --id;  // Zero is reserved to mean "invalid".
  DCHECK(allocated_ids_[id]) << id;
  allocated_ids_.reset(id);
}

Thread* ThreadList::FindThreadByThinLockId(uint32_t thin_lock_id) {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  for (const auto& thread : list_) {
    if (thread->GetThinLockId() == thin_lock_id) {
      return thread;
    }
  }
  return NULL;
}

}  // namespace art
