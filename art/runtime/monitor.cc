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

#include "monitor.h"

#include <vector>

#include "base/mutex.h"
#include "base/stl_util.h"
#include "class_linker.h"
#include "dex_file-inl.h"
#include "dex_instruction.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "object_utils.h"
#include "scoped_thread_state_change.h"
#include "thread.h"
#include "thread_list.h"
#include "verifier/method_verifier.h"
#include "well_known_classes.h"

namespace art {

/*
 * Every Object has a monitor associated with it, but not every Object is
 * actually locked.  Even the ones that are locked do not need a
 * full-fledged monitor until a) there is actual contention or b) wait()
 * is called on the Object.
 *
 * For Android, we have implemented a scheme similar to the one described
 * in Bacon et al.'s "Thin locks: featherweight synchronization for Java"
 * (ACM 1998).  Things are even easier for us, though, because we have
 * a full 32 bits to work with.
 *
 * The two states of an Object's lock are referred to as "thin" and
 * "fat".  A lock may transition from the "thin" state to the "fat"
 * state and this transition is referred to as inflation.  Once a lock
 * has been inflated it remains in the "fat" state indefinitely.
 *
 * The lock value itself is stored in Object.lock.  The LSB of the
 * lock encodes its state.  When cleared, the lock is in the "thin"
 * state and its bits are formatted as follows:
 *
 *    [31 ---- 19] [18 ---- 3] [2 ---- 1] [0]
 *     lock count   thread id  hash state  0
 *
 * When set, the lock is in the "fat" state and its bits are formatted
 * as follows:
 *
 *    [31 ---- 3] [2 ---- 1] [0]
 *      pointer   hash state  1
 *
 * For an in-depth description of the mechanics of thin-vs-fat locking,
 * read the paper referred to above.
 *
 * Monitors provide:
 *  - mutually exclusive access to resources
 *  - a way for multiple threads to wait for notification
 *
 * In effect, they fill the role of both mutexes and condition variables.
 *
 * Only one thread can own the monitor at any time.  There may be several
 * threads waiting on it (the wait call unlocks it).  One or more waiting
 * threads may be getting interrupted or notified at any given time.
 *
 * TODO: the various members of monitor are not SMP-safe.
 */

// The shape is the bottom bit; either LW_SHAPE_THIN or LW_SHAPE_FAT.
#define LW_SHAPE_MASK 0x1
#define LW_SHAPE(x) static_cast<int>((x) & LW_SHAPE_MASK)

/*
 * Monitor accessor.  Extracts a monitor structure pointer from a fat
 * lock.  Performs no error checking.
 */
#define LW_MONITOR(x) \
  (reinterpret_cast<Monitor*>((x) & ~((LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT) | LW_SHAPE_MASK)))

/*
 * Lock recursion count field.  Contains a count of the number of times
 * a lock has been recursively acquired.
 */
#define LW_LOCK_COUNT_MASK 0x1fff
#define LW_LOCK_COUNT_SHIFT 19
#define LW_LOCK_COUNT(x) (((x) >> LW_LOCK_COUNT_SHIFT) & LW_LOCK_COUNT_MASK)

bool (*Monitor::is_sensitive_thread_hook_)() = NULL;
uint32_t Monitor::lock_profiling_threshold_ = 0;

bool Monitor::IsSensitiveThread() {
  if (is_sensitive_thread_hook_ != NULL) {
    return (*is_sensitive_thread_hook_)();
  }
  return false;
}

void Monitor::Init(uint32_t lock_profiling_threshold, bool (*is_sensitive_thread_hook)()) {
  lock_profiling_threshold_ = lock_profiling_threshold;
  is_sensitive_thread_hook_ = is_sensitive_thread_hook;
}

Monitor::Monitor(Thread* owner, mirror::Object* obj)
    : monitor_lock_("a monitor lock", kMonitorLock),
      owner_(owner),
      lock_count_(0),
      obj_(obj),
      wait_set_(NULL),
      locking_method_(NULL),
      locking_dex_pc_(0) {
  monitor_lock_.Lock(owner);
  // Propagate the lock state.
  uint32_t thin = *obj->GetRawLockWordAddress();
  lock_count_ = LW_LOCK_COUNT(thin);
  thin &= LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT;
  thin |= reinterpret_cast<uint32_t>(this) | LW_SHAPE_FAT;
  // Publish the updated lock word.
  android_atomic_release_store(thin, obj->GetRawLockWordAddress());
  // Lock profiling.
  if (lock_profiling_threshold_ != 0) {
    locking_method_ = owner->GetCurrentMethod(&locking_dex_pc_);
  }
}

Monitor::~Monitor() {
  DCHECK(obj_ != NULL);
  DCHECK_EQ(LW_SHAPE(*obj_->GetRawLockWordAddress()), LW_SHAPE_FAT);
}

/*
 * Links a thread into a monitor's wait set.  The monitor lock must be
 * held by the caller of this routine.
 */
void Monitor::AppendToWaitSet(Thread* thread) {
  DCHECK(owner_ == Thread::Current());
  DCHECK(thread != NULL);
  DCHECK(thread->wait_next_ == NULL) << thread->wait_next_;
  if (wait_set_ == NULL) {
    wait_set_ = thread;
    return;
  }

  // push_back.
  Thread* t = wait_set_;
  while (t->wait_next_ != NULL) {
    t = t->wait_next_;
  }
  t->wait_next_ = thread;
}

/*
 * Unlinks a thread from a monitor's wait set.  The monitor lock must
 * be held by the caller of this routine.
 */
void Monitor::RemoveFromWaitSet(Thread *thread) {
  DCHECK(owner_ == Thread::Current());
  DCHECK(thread != NULL);
  if (wait_set_ == NULL) {
    return;
  }
  if (wait_set_ == thread) {
    wait_set_ = thread->wait_next_;
    thread->wait_next_ = NULL;
    return;
  }

  Thread* t = wait_set_;
  while (t->wait_next_ != NULL) {
    if (t->wait_next_ == thread) {
      t->wait_next_ = thread->wait_next_;
      thread->wait_next_ = NULL;
      return;
    }
    t = t->wait_next_;
  }
}

mirror::Object* Monitor::GetObject() {
  return obj_;
}

void Monitor::Lock(Thread* self) {
  if (owner_ == self) {
    lock_count_++;
    return;
  }

  if (!monitor_lock_.TryLock(self)) {
    uint64_t waitStart = 0;
    uint64_t waitEnd = 0;
    uint32_t wait_threshold = lock_profiling_threshold_;
    const mirror::ArtMethod* current_locking_method = NULL;
    uint32_t current_locking_dex_pc = 0;
    {
      ScopedThreadStateChange tsc(self, kBlocked);
      if (wait_threshold != 0) {
        waitStart = NanoTime() / 1000;
      }
      current_locking_method = locking_method_;
      current_locking_dex_pc = locking_dex_pc_;

      monitor_lock_.Lock(self);
      if (wait_threshold != 0) {
        waitEnd = NanoTime() / 1000;
      }
    }

    if (wait_threshold != 0) {
      uint64_t wait_ms = (waitEnd - waitStart) / 1000;
      uint32_t sample_percent;
      if (wait_ms >= wait_threshold) {
        sample_percent = 100;
      } else {
        sample_percent = 100 * wait_ms / wait_threshold;
      }
      if (sample_percent != 0 && (static_cast<uint32_t>(rand() % 100) < sample_percent)) {
        const char* current_locking_filename;
        uint32_t current_locking_line_number;
        TranslateLocation(current_locking_method, current_locking_dex_pc,
                          current_locking_filename, current_locking_line_number);
        LogContentionEvent(self, wait_ms, sample_percent, current_locking_filename, current_locking_line_number);
      }
    }
  }
  owner_ = self;
  DCHECK_EQ(lock_count_, 0);

  // When debugging, save the current monitor holder for future
  // acquisition failures to use in sampled logging.
  if (lock_profiling_threshold_ != 0) {
    locking_method_ = self->GetCurrentMethod(&locking_dex_pc_);
  }
}

static void ThrowIllegalMonitorStateExceptionF(const char* fmt, ...)
                                              __attribute__((format(printf, 1, 2)));

static void ThrowIllegalMonitorStateExceptionF(const char* fmt, ...)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  va_list args;
  va_start(args, fmt);
  Thread* self = Thread::Current();
  ThrowLocation throw_location = self->GetCurrentLocationForThrow();
  self->ThrowNewExceptionV(throw_location, "Ljava/lang/IllegalMonitorStateException;", fmt, args);
  if (!Runtime::Current()->IsStarted()) {
    std::ostringstream ss;
    self->Dump(ss);
    std::string str(ss.str());
    LOG(ERROR) << "IllegalMonitorStateException: " << str;
  }
  va_end(args);
}

static std::string ThreadToString(Thread* thread) {
  if (thread == NULL) {
    return "NULL";
  }
  std::ostringstream oss;
  // TODO: alternatively, we could just return the thread's name.
  oss << *thread;
  return oss.str();
}

void Monitor::FailedUnlock(mirror::Object* o, Thread* expected_owner, Thread* found_owner,
                           Monitor* monitor) {
  Thread* current_owner = NULL;
  std::string current_owner_string;
  std::string expected_owner_string;
  std::string found_owner_string;
  {
    // TODO: isn't this too late to prevent threads from disappearing?
    // Acquire thread list lock so threads won't disappear from under us.
    MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
    // Re-read owner now that we hold lock.
    current_owner = (monitor != NULL) ? monitor->owner_ : NULL;
    // Get short descriptions of the threads involved.
    current_owner_string = ThreadToString(current_owner);
    expected_owner_string = ThreadToString(expected_owner);
    found_owner_string = ThreadToString(found_owner);
  }
  if (current_owner == NULL) {
    if (found_owner == NULL) {
      ThrowIllegalMonitorStateExceptionF("unlock of unowned monitor on object of type '%s'"
                                         " on thread '%s'",
                                         PrettyTypeOf(o).c_str(),
                                         expected_owner_string.c_str());
    } else {
      // Race: the original read found an owner but now there is none
      ThrowIllegalMonitorStateExceptionF("unlock of monitor owned by '%s' on object of type '%s'"
                                         " (where now the monitor appears unowned) on thread '%s'",
                                         found_owner_string.c_str(),
                                         PrettyTypeOf(o).c_str(),
                                         expected_owner_string.c_str());
    }
  } else {
    if (found_owner == NULL) {
      // Race: originally there was no owner, there is now
      ThrowIllegalMonitorStateExceptionF("unlock of monitor owned by '%s' on object of type '%s'"
                                         " (originally believed to be unowned) on thread '%s'",
                                         current_owner_string.c_str(),
                                         PrettyTypeOf(o).c_str(),
                                         expected_owner_string.c_str());
    } else {
      if (found_owner != current_owner) {
        // Race: originally found and current owner have changed
        ThrowIllegalMonitorStateExceptionF("unlock of monitor originally owned by '%s' (now"
                                           " owned by '%s') on object of type '%s' on thread '%s'",
                                           found_owner_string.c_str(),
                                           current_owner_string.c_str(),
                                           PrettyTypeOf(o).c_str(),
                                           expected_owner_string.c_str());
      } else {
        ThrowIllegalMonitorStateExceptionF("unlock of monitor owned by '%s' on object of type '%s'"
                                           " on thread '%s",
                                           current_owner_string.c_str(),
                                           PrettyTypeOf(o).c_str(),
                                           expected_owner_string.c_str());
      }
    }
  }
}

bool Monitor::Unlock(Thread* self, bool for_wait) {
  DCHECK(self != NULL);
  Thread* owner = owner_;
  if (owner == self) {
    // We own the monitor, so nobody else can be in here.
    if (lock_count_ == 0) {
      owner_ = NULL;
      locking_method_ = NULL;
      locking_dex_pc_ = 0;
      monitor_lock_.Unlock(self);
    } else {
      --lock_count_;
    }
  } else if (for_wait) {
    // Wait should have already cleared the fields.
    DCHECK_EQ(lock_count_, 0);
    DCHECK(owner == NULL);
    DCHECK(locking_method_ == NULL);
    DCHECK_EQ(locking_dex_pc_, 0u);
    monitor_lock_.Unlock(self);
  } else {
    // We don't own this, so we're not allowed to unlock it.
    // The JNI spec says that we should throw IllegalMonitorStateException
    // in this case.
    FailedUnlock(obj_, self, owner, this);
    return false;
  }
  return true;
}

/*
 * Wait on a monitor until timeout, interrupt, or notification.  Used for
 * Object.wait() and (somewhat indirectly) Thread.sleep() and Thread.join().
 *
 * If another thread calls Thread.interrupt(), we throw InterruptedException
 * and return immediately if one of the following are true:
 *  - blocked in wait(), wait(long), or wait(long, int) methods of Object
 *  - blocked in join(), join(long), or join(long, int) methods of Thread
 *  - blocked in sleep(long), or sleep(long, int) methods of Thread
 * Otherwise, we set the "interrupted" flag.
 *
 * Checks to make sure that "ns" is in the range 0-999999
 * (i.e. fractions of a millisecond) and throws the appropriate
 * exception if it isn't.
 *
 * The spec allows "spurious wakeups", and recommends that all code using
 * Object.wait() do so in a loop.  This appears to derive from concerns
 * about pthread_cond_wait() on multiprocessor systems.  Some commentary
 * on the web casts doubt on whether these can/should occur.
 *
 * Since we're allowed to wake up "early", we clamp extremely long durations
 * to return at the end of the 32-bit time epoch.
 */
void Monitor::Wait(Thread* self, int64_t ms, int32_t ns,
                   bool interruptShouldThrow, ThreadState why) {
  DCHECK(self != NULL);
  DCHECK(why == kTimedWaiting || why == kWaiting || why == kSleeping);

  // Make sure that we hold the lock.
  if (owner_ != self) {
    ThrowIllegalMonitorStateExceptionF("object not locked by thread before wait()");
    return;
  }
  monitor_lock_.AssertHeld(self);

  // We need to turn a zero-length timed wait into a regular wait because
  // Object.wait(0, 0) is defined as Object.wait(0), which is defined as Object.wait().
  if (why == kTimedWaiting && (ms == 0 && ns == 0)) {
    why = kWaiting;
  }

  WaitWithLock(self, ms, ns, interruptShouldThrow, why);
}

void Monitor::WaitWithLock(Thread* self, int64_t ms, int32_t ns,
                           bool interruptShouldThrow, ThreadState why) {
  // Enforce the timeout range.
  if (ms < 0 || ns < 0 || ns > 999999) {
    ThrowLocation throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewExceptionF(throw_location, "Ljava/lang/IllegalArgumentException;",
                             "timeout arguments out of range: ms=%lld ns=%d", ms, ns);
    return;
  }

  /*
   * Add ourselves to the set of threads waiting on this monitor, and
   * release our hold.  We need to let it go even if we're a few levels
   * deep in a recursive lock, and we need to restore that later.
   *
   * We append to the wait set ahead of clearing the count and owner
   * fields so the subroutine can check that the calling thread owns
   * the monitor.  Aside from that, the order of member updates is
   * not order sensitive as we hold the pthread mutex.
   */
  AppendToWaitSet(self);
  int prev_lock_count = lock_count_;
  lock_count_ = 0;
  owner_ = NULL;
  const mirror::ArtMethod* saved_method = locking_method_;
  locking_method_ = NULL;
  uintptr_t saved_dex_pc = locking_dex_pc_;
  locking_dex_pc_ = 0;

  /*
   * Update thread state. If the GC wakes up, it'll ignore us, knowing
   * that we won't touch any references in this state, and we'll check
   * our suspend mode before we transition out.
   */
  self->TransitionFromRunnableToSuspended(why);

  bool was_interrupted = false;
  {
    // Pseudo-atomically wait on self's wait_cond_ and release the monitor lock.
    MutexLock mu(self, *self->wait_mutex_);

    // Set wait_monitor_ to the monitor object we will be waiting on. When wait_monitor_ is
    // non-NULL a notifying or interrupting thread must signal the thread's wait_cond_ to wake it
    // up.
    DCHECK(self->wait_monitor_ == NULL);
    self->wait_monitor_ = this;

    // Release the monitor lock.
    Unlock(self, true);

    // Handle the case where the thread was interrupted before we called wait().
    if (self->interrupted_) {
      was_interrupted = true;
    } else {
      // Wait for a notification or a timeout to occur.
      if (why == kWaiting) {
        self->wait_cond_->Wait(self);
      } else {
        DCHECK(why == kTimedWaiting || why == kSleeping) << why;
        self->wait_cond_->TimedWait(self, ms, ns);
      }
      if (self->interrupted_) {
        was_interrupted = true;
      }
      self->interrupted_ = false;
    }
  }

  // Set self->status back to kRunnable, and self-suspend if needed.
  self->TransitionFromSuspendedToRunnable();

  {
    // We reset the thread's wait_monitor_ field after transitioning back to runnable so
    // that a thread in a waiting/sleeping state has a non-null wait_monitor_ for debugging
    // and diagnostic purposes. (If you reset this earlier, stack dumps will claim that threads
    // are waiting on "null".)
    MutexLock mu(self, *self->wait_mutex_);
    DCHECK(self->wait_monitor_ != NULL);
    self->wait_monitor_ = NULL;
  }

  // Re-acquire the monitor lock.
  Lock(self);

  self->wait_mutex_->AssertNotHeld(self);

  /*
   * We remove our thread from wait set after restoring the count
   * and owner fields so the subroutine can check that the calling
   * thread owns the monitor. Aside from that, the order of member
   * updates is not order sensitive as we hold the pthread mutex.
   */
  owner_ = self;
  lock_count_ = prev_lock_count;
  locking_method_ = saved_method;
  locking_dex_pc_ = saved_dex_pc;
  RemoveFromWaitSet(self);

  if (was_interrupted) {
    /*
     * We were interrupted while waiting, or somebody interrupted an
     * un-interruptible thread earlier and we're bailing out immediately.
     *
     * The doc sayeth: "The interrupted status of the current thread is
     * cleared when this exception is thrown."
     */
    {
      MutexLock mu(self, *self->wait_mutex_);
      self->interrupted_ = false;
    }
    if (interruptShouldThrow) {
      ThrowLocation throw_location = self->GetCurrentLocationForThrow();
      self->ThrowNewException(throw_location, "Ljava/lang/InterruptedException;", NULL);
    }
  }
}

void Monitor::Notify(Thread* self) {
  DCHECK(self != NULL);
  // Make sure that we hold the lock.
  if (owner_ != self) {
    ThrowIllegalMonitorStateExceptionF("object not locked by thread before notify()");
    return;
  }
  monitor_lock_.AssertHeld(self);
  NotifyWithLock(self);
}

void Monitor::NotifyWithLock(Thread* self) {
  // Signal the first waiting thread in the wait set.
  while (wait_set_ != NULL) {
    Thread* thread = wait_set_;
    wait_set_ = thread->wait_next_;
    thread->wait_next_ = NULL;

    // Check to see if the thread is still waiting.
    MutexLock mu(self, *thread->wait_mutex_);
    if (thread->wait_monitor_ != NULL) {
      thread->wait_cond_->Signal(self);
      return;
    }
  }
}

void Monitor::NotifyAll(Thread* self) {
  DCHECK(self != NULL);
  // Make sure that we hold the lock.
  if (owner_ != self) {
    ThrowIllegalMonitorStateExceptionF("object not locked by thread before notifyAll()");
    return;
  }
  monitor_lock_.AssertHeld(self);
  NotifyAllWithLock();
}

void Monitor::NotifyAllWithLock() {
  // Signal all threads in the wait set.
  while (wait_set_ != NULL) {
    Thread* thread = wait_set_;
    wait_set_ = thread->wait_next_;
    thread->wait_next_ = NULL;
    thread->Notify();
  }
}

/*
 * Changes the shape of a monitor from thin to fat, preserving the
 * internal lock state. The calling thread must own the lock.
 */
void Monitor::Inflate(Thread* self, mirror::Object* obj) {
  DCHECK(self != NULL);
  DCHECK(obj != NULL);
  DCHECK_EQ(LW_SHAPE(*obj->GetRawLockWordAddress()), LW_SHAPE_THIN);
  DCHECK_EQ(LW_LOCK_OWNER(*obj->GetRawLockWordAddress()), static_cast<int32_t>(self->GetThinLockId()));

  // Allocate and acquire a new monitor.
  Monitor* m = new Monitor(self, obj);
  VLOG(monitor) << "monitor: thread " << self->GetThinLockId()
                << " created monitor " << m << " for object " << obj;
  Runtime::Current()->GetMonitorList()->Add(m);
}

void Monitor::MonitorEnter(Thread* self, mirror::Object* obj) {
  volatile int32_t* thinp = obj->GetRawLockWordAddress();
  uint32_t sleepDelayNs;
  uint32_t minSleepDelayNs = 1000000;  /* 1 millisecond */
  uint32_t maxSleepDelayNs = 1000000000;  /* 1 second */
  uint32_t thin, newThin;

  DCHECK(self != NULL);
  DCHECK(obj != NULL);
  uint32_t threadId = self->GetThinLockId();
 retry:
  thin = *thinp;
  if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
    /*
     * The lock is a thin lock.  The owner field is used to
     * determine the acquire method, ordered by cost.
     */
    if (LW_LOCK_OWNER(thin) == threadId) {
      /*
       * The calling thread owns the lock.  Increment the
       * value of the recursion count field.
       */
      *thinp += 1 << LW_LOCK_COUNT_SHIFT;
      if (LW_LOCK_COUNT(*thinp) == LW_LOCK_COUNT_MASK) {
        /*
         * The reacquisition limit has been reached.  Inflate
         * the lock so the next acquire will not overflow the
         * recursion count field.
         */
        Inflate(self, obj);
      }
    } else if (LW_LOCK_OWNER(thin) == 0) {
      // The lock is unowned. Install the thread id of the calling thread into the owner field.
      // This is the common case: compiled code will have tried this before calling back into
      // the runtime.
      newThin = thin | (threadId << LW_LOCK_OWNER_SHIFT);
      if (android_atomic_acquire_cas(thin, newThin, thinp) != 0) {
        // The acquire failed. Try again.
        goto retry;
      }
    } else {
      VLOG(monitor) << StringPrintf("monitor: thread %d spin on lock %p (a %s) owned by %d",
                                    threadId, thinp, PrettyTypeOf(obj).c_str(), LW_LOCK_OWNER(thin));
      // The lock is owned by another thread. Notify the runtime that we are about to wait.
      self->monitor_enter_object_ = obj;
      self->TransitionFromRunnableToSuspended(kBlocked);
      // Spin until the thin lock is released or inflated.
      sleepDelayNs = 0;
      for (;;) {
        thin = *thinp;
        // Check the shape of the lock word. Another thread
        // may have inflated the lock while we were waiting.
        if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
          if (LW_LOCK_OWNER(thin) == 0) {
            // The lock has been released. Install the thread id of the
            // calling thread into the owner field.
            newThin = thin | (threadId << LW_LOCK_OWNER_SHIFT);
            if (android_atomic_acquire_cas(thin, newThin, thinp) == 0) {
              // The acquire succeed. Break out of the loop and proceed to inflate the lock.
              break;
            }
          } else {
            // The lock has not been released. Yield so the owning thread can run.
            if (sleepDelayNs == 0) {
              sched_yield();
              sleepDelayNs = minSleepDelayNs;
            } else {
              NanoSleep(sleepDelayNs);
              // Prepare the next delay value. Wrap to avoid once a second polls for eternity.
              if (sleepDelayNs < maxSleepDelayNs / 2) {
                sleepDelayNs *= 2;
              } else {
                sleepDelayNs = minSleepDelayNs;
              }
            }
          }
        } else {
          // The thin lock was inflated by another thread. Let the runtime know we are no longer
          // waiting and try again.
          VLOG(monitor) << StringPrintf("monitor: thread %d found lock %p surprise-fattened by another thread", threadId, thinp);
          self->monitor_enter_object_ = NULL;
          self->TransitionFromSuspendedToRunnable();
          goto retry;
        }
      }
      VLOG(monitor) << StringPrintf("monitor: thread %d spin on lock %p done", threadId, thinp);
      // We have acquired the thin lock. Let the runtime know that we are no longer waiting.
      self->monitor_enter_object_ = NULL;
      self->TransitionFromSuspendedToRunnable();
      // Fatten the lock.
      Inflate(self, obj);
      VLOG(monitor) << StringPrintf("monitor: thread %d fattened lock %p", threadId, thinp);
    }
  } else {
    // The lock is a fat lock.
    VLOG(monitor) << StringPrintf("monitor: thread %d locking fat lock %p (%p) %p on a %s",
                                  threadId, thinp, LW_MONITOR(*thinp),
                                  reinterpret_cast<void*>(*thinp), PrettyTypeOf(obj).c_str());
    DCHECK(LW_MONITOR(*thinp) != NULL);
    LW_MONITOR(*thinp)->Lock(self);
  }
}

bool Monitor::MonitorExit(Thread* self, mirror::Object* obj) {
  volatile int32_t* thinp = obj->GetRawLockWordAddress();

  DCHECK(self != NULL);
  // DCHECK_EQ(self->GetState(), kRunnable);
  DCHECK(obj != NULL);

  /*
   * Cache the lock word as its value can change while we are
   * examining its state.
   */
  uint32_t thin = *thinp;
  if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
    /*
     * The lock is thin.  We must ensure that the lock is owned
     * by the given thread before unlocking it.
     */
    if (LW_LOCK_OWNER(thin) == self->GetThinLockId()) {
      /*
       * We are the lock owner.  It is safe to update the lock
       * without CAS as lock ownership guards the lock itself.
       */
      if (LW_LOCK_COUNT(thin) == 0) {
        /*
         * The lock was not recursively acquired, the common
         * case.  Unlock by clearing all bits except for the
         * hash state.
         */
        thin &= (LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT);
        android_atomic_release_store(thin, thinp);
      } else {
        /*
         * The object was recursively acquired.  Decrement the
         * lock recursion count field.
         */
        *thinp -= 1 << LW_LOCK_COUNT_SHIFT;
      }
    } else {
      /*
       * We do not own the lock.  The JVM spec requires that we
       * throw an exception in this case.
       */
      FailedUnlock(obj, self, NULL, NULL);
      return false;
    }
  } else {
    /*
     * The lock is fat.  We must check to see if Unlock has
     * raised any exceptions before continuing.
     */
    DCHECK(LW_MONITOR(*thinp) != NULL);
    if (!LW_MONITOR(*thinp)->Unlock(self, false)) {
      // An exception has been raised.  Do not fall through.
      return false;
    }
  }
  return true;
}

/*
 * Object.wait().  Also called for class init.
 */
void Monitor::Wait(Thread* self, mirror::Object *obj, int64_t ms, int32_t ns,
                   bool interruptShouldThrow, ThreadState why) {
  volatile int32_t* thinp = obj->GetRawLockWordAddress();

  // If the lock is still thin, we need to fatten it.
  uint32_t thin = *thinp;
  if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
    // Make sure that 'self' holds the lock.
    if (LW_LOCK_OWNER(thin) != self->GetThinLockId()) {
      ThrowIllegalMonitorStateExceptionF("object not locked by thread before wait()");
      return;
    }

    /* This thread holds the lock.  We need to fatten the lock
     * so 'self' can block on it.  Don't update the object lock
     * field yet, because 'self' needs to acquire the lock before
     * any other thread gets a chance.
     */
    Inflate(self, obj);
    VLOG(monitor) << StringPrintf("monitor: thread %d fattened lock %p by wait()", self->GetThinLockId(), thinp);
  }
  LW_MONITOR(*thinp)->Wait(self, ms, ns, interruptShouldThrow, why);
}

void Monitor::Notify(Thread* self, mirror::Object *obj) {
  uint32_t thin = *obj->GetRawLockWordAddress();

  // If the lock is still thin, there aren't any waiters;
  // waiting on an object forces lock fattening.
  if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
    // Make sure that 'self' holds the lock.
    if (LW_LOCK_OWNER(thin) != self->GetThinLockId()) {
      ThrowIllegalMonitorStateExceptionF("object not locked by thread before notify()");
      return;
    }
    // no-op;  there are no waiters to notify.
    // We inflate here in case the Notify is in a tight loop. Without inflation here the waiter
    // will struggle to get in. Bug 6961405.
    Inflate(self, obj);
  } else {
    // It's a fat lock.
    LW_MONITOR(thin)->Notify(self);
  }
}

void Monitor::NotifyAll(Thread* self, mirror::Object *obj) {
  uint32_t thin = *obj->GetRawLockWordAddress();

  // If the lock is still thin, there aren't any waiters;
  // waiting on an object forces lock fattening.
  if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
    // Make sure that 'self' holds the lock.
    if (LW_LOCK_OWNER(thin) != self->GetThinLockId()) {
      ThrowIllegalMonitorStateExceptionF("object not locked by thread before notifyAll()");
      return;
    }
    // no-op;  there are no waiters to notify.
    // We inflate here in case the NotifyAll is in a tight loop. Without inflation here the waiter
    // will struggle to get in. Bug 6961405.
    Inflate(self, obj);
  } else {
    // It's a fat lock.
    LW_MONITOR(thin)->NotifyAll(self);
  }
}

uint32_t Monitor::GetThinLockId(uint32_t raw_lock_word) {
  if (LW_SHAPE(raw_lock_word) == LW_SHAPE_THIN) {
    return LW_LOCK_OWNER(raw_lock_word);
  } else {
    Thread* owner = LW_MONITOR(raw_lock_word)->owner_;
    return owner ? owner->GetThinLockId() : 0;
  }
}

void Monitor::DescribeWait(std::ostream& os, const Thread* thread) {
  ThreadState state = thread->GetState();

  mirror::Object* object = NULL;
  uint32_t lock_owner = ThreadList::kInvalidId;
  if (state == kWaiting || state == kTimedWaiting || state == kSleeping) {
    if (state == kSleeping) {
      os << "  - sleeping on ";
    } else {
      os << "  - waiting on ";
    }
    {
      Thread* self = Thread::Current();
      MutexLock mu(self, *thread->wait_mutex_);
      Monitor* monitor = thread->wait_monitor_;
      if (monitor != NULL) {
        object = monitor->obj_;
      }
    }
  } else if (state == kBlocked) {
    os << "  - waiting to lock ";
    object = thread->monitor_enter_object_;
    if (object != NULL) {
      lock_owner = object->GetThinLockId();
    }
  } else {
    // We're not waiting on anything.
    return;
  }

  // - waiting on <0x6008c468> (a java.lang.Class<java.lang.ref.ReferenceQueue>)
  os << "<" << object << "> (a " << PrettyTypeOf(object) << ")";

  // - waiting to lock <0x613f83d8> (a java.lang.Object) held by thread 5
  if (lock_owner != ThreadList::kInvalidId) {
    os << " held by thread " << lock_owner;
  }

  os << "\n";
}

mirror::Object* Monitor::GetContendedMonitor(Thread* thread) {
  // This is used to implement JDWP's ThreadReference.CurrentContendedMonitor, and has a bizarre
  // definition of contended that includes a monitor a thread is trying to enter...
  mirror::Object* result = thread->monitor_enter_object_;
  if (result != NULL) {
    return result;
  }
  // ...but also a monitor that the thread is waiting on.
  {
    MutexLock mu(Thread::Current(), *thread->wait_mutex_);
    Monitor* monitor = thread->wait_monitor_;
    if (monitor != NULL) {
      return monitor->obj_;
    }
  }
  return NULL;
}

void Monitor::VisitLocks(StackVisitor* stack_visitor, void (*callback)(mirror::Object*, void*),
                         void* callback_context) {
  mirror::ArtMethod* m = stack_visitor->GetMethod();
  CHECK(m != NULL);

  // Native methods are an easy special case.
  // TODO: use the JNI implementation's table of explicit MonitorEnter calls and dump those too.
  if (m->IsNative()) {
    if (m->IsSynchronized()) {
      mirror::Object* jni_this = stack_visitor->GetCurrentSirt()->GetReference(0);
      callback(jni_this, callback_context);
    }
    return;
  }

  // Proxy methods should not be synchronized.
  if (m->IsProxyMethod()) {
    CHECK(!m->IsSynchronized());
    return;
  }

  // <clinit> is another special case. The runtime holds the class lock while calling <clinit>.
  MethodHelper mh(m);
  if (mh.IsClassInitializer()) {
    callback(m->GetDeclaringClass(), callback_context);
    // Fall through because there might be synchronization in the user code too.
  }

  // Is there any reason to believe there's any synchronization in this method?
  const DexFile::CodeItem* code_item = mh.GetCodeItem();
  CHECK(code_item != NULL) << PrettyMethod(m);
  if (code_item->tries_size_ == 0) {
    return;  // No "tries" implies no synchronization, so no held locks to report.
  }

  // Ask the verifier for the dex pcs of all the monitor-enter instructions corresponding to
  // the locks held in this stack frame.
  std::vector<uint32_t> monitor_enter_dex_pcs;
  verifier::MethodVerifier::FindLocksAtDexPc(m, stack_visitor->GetDexPc(), monitor_enter_dex_pcs);
  if (monitor_enter_dex_pcs.empty()) {
    return;
  }

  for (size_t i = 0; i < monitor_enter_dex_pcs.size(); ++i) {
    // The verifier works in terms of the dex pcs of the monitor-enter instructions.
    // We want the registers used by those instructions (so we can read the values out of them).
    uint32_t dex_pc = monitor_enter_dex_pcs[i];
    uint16_t monitor_enter_instruction = code_item->insns_[dex_pc];

    // Quick sanity check.
    if ((monitor_enter_instruction & 0xff) != Instruction::MONITOR_ENTER) {
      LOG(FATAL) << "expected monitor-enter @" << dex_pc << "; was "
                 << reinterpret_cast<void*>(monitor_enter_instruction);
    }

    uint16_t monitor_register = ((monitor_enter_instruction >> 8) & 0xff);
    mirror::Object* o = reinterpret_cast<mirror::Object*>(stack_visitor->GetVReg(m, monitor_register,
                                                                                 kReferenceVReg));
    callback(o, callback_context);
  }
}

bool Monitor::IsValidLockWord(int32_t lock_word) {
  if (lock_word == 0) {
    return true;
  } else if (LW_SHAPE(lock_word) == LW_SHAPE_FAT) {
    Monitor* mon = LW_MONITOR(lock_word);
    MonitorList* list = Runtime::Current()->GetMonitorList();
    MutexLock mu(Thread::Current(), list->monitor_list_lock_);
    bool found = false;
    for (Monitor* list_mon : list->list_) {
      if (mon == list_mon) {
        found = true;
        break;
      }
    }
    return found;
  } else {
    // TODO: thin lock validity checking.
    return LW_SHAPE(lock_word) == LW_SHAPE_THIN;
  }
}

void Monitor::TranslateLocation(const mirror::ArtMethod* method, uint32_t dex_pc,
                                const char*& source_file, uint32_t& line_number) const {
  // If method is null, location is unknown
  if (method == NULL) {
    source_file = "";
    line_number = 0;
    return;
  }
  MethodHelper mh(method);
  source_file = mh.GetDeclaringClassSourceFile();
  if (source_file == NULL) {
    source_file = "";
  }
  line_number = mh.GetLineNumFromDexPC(dex_pc);
}

MonitorList::MonitorList()
    : allow_new_monitors_(true), monitor_list_lock_("MonitorList lock"),
      monitor_add_condition_("MonitorList disallow condition", monitor_list_lock_) {
}

MonitorList::~MonitorList() {
  MutexLock mu(Thread::Current(), monitor_list_lock_);
  STLDeleteElements(&list_);
}

void MonitorList::DisallowNewMonitors() {
  MutexLock mu(Thread::Current(), monitor_list_lock_);
  allow_new_monitors_ = false;
}

void MonitorList::AllowNewMonitors() {
  Thread* self = Thread::Current();
  MutexLock mu(self, monitor_list_lock_);
  allow_new_monitors_ = true;
  monitor_add_condition_.Broadcast(self);
}

void MonitorList::Add(Monitor* m) {
  Thread* self = Thread::Current();
  MutexLock mu(self, monitor_list_lock_);
  while (UNLIKELY(!allow_new_monitors_)) {
    monitor_add_condition_.WaitHoldingLocks(self);
  }
  list_.push_front(m);
}

void MonitorList::SweepMonitorList(IsMarkedTester is_marked, void* arg) {
  MutexLock mu(Thread::Current(), monitor_list_lock_);
  for (auto it = list_.begin(); it != list_.end(); ) {
    Monitor* m = *it;
    if (!is_marked(m->GetObject(), arg)) {
      VLOG(monitor) << "freeing monitor " << m << " belonging to unmarked object " << m->GetObject();
      delete m;
      it = list_.erase(it);
    } else {
      ++it;
    }
  }
}

MonitorInfo::MonitorInfo(mirror::Object* o) : owner(NULL), entry_count(0) {
  uint32_t lock_word = *o->GetRawLockWordAddress();
  if (LW_SHAPE(lock_word) == LW_SHAPE_THIN) {
    uint32_t owner_thin_lock_id = LW_LOCK_OWNER(lock_word);
    if (owner_thin_lock_id != 0) {
      owner = Runtime::Current()->GetThreadList()->FindThreadByThinLockId(owner_thin_lock_id);
      entry_count = 1 + LW_LOCK_COUNT(lock_word);
    }
    // Thin locks have no waiters.
  } else {
    CHECK_EQ(LW_SHAPE(lock_word), LW_SHAPE_FAT);
    Monitor* monitor = LW_MONITOR(lock_word);
    owner = monitor->owner_;
    entry_count = 1 + monitor->lock_count_;
    for (Thread* waiter = monitor->wait_set_; waiter != NULL; waiter = waiter->wait_next_) {
      waiters.push_back(waiter);
    }
  }
}

}  // namespace art
