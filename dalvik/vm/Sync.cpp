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

#include "Dalvik.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

/*
 * Every Object has a monitor associated with it, but not every Object is
 * actually locked.  Even the ones that are locked do not need a
 * full-fledged monitor until a) there is actual contention or b) wait()
 * is called on the Object.
 *
 * For Dalvik, we have implemented a scheme similar to the one described
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
 */

/*
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
struct Monitor {
    Thread*     owner;          /* which thread currently owns the lock? */
    int         lockCount;      /* owner's recursive lock depth */
    Object*     obj;            /* what object are we part of [debug only] */

    Thread*     waitSet;	/* threads currently waiting on this monitor */

    pthread_mutex_t lock;

    Monitor*    next;

    /*
     * Who last acquired this monitor, when lock sampling is enabled.
     * Even when enabled, ownerMethod may be NULL.
     */
    const Method* ownerMethod;
    u4 ownerPc;
};


/*
 * Create and initialize a monitor.
 */
Monitor* dvmCreateMonitor(Object* obj)
{
    Monitor* mon;

    mon = (Monitor*) calloc(1, sizeof(Monitor));
    if (mon == NULL) {
        ALOGE("Unable to allocate monitor");
        dvmAbort();
    }
    mon->obj = obj;
    dvmInitMutex(&mon->lock);

    /* replace the head of the list with the new monitor */
    do {
        mon->next = gDvm.monitorList;
    } while (android_atomic_release_cas((int32_t)mon->next, (int32_t)mon,
            (int32_t*)(void*)&gDvm.monitorList) != 0);

    return mon;
}

/*
 * Free the monitor list.  Only used when shutting the VM down.
 */
void dvmFreeMonitorList()
{
    Monitor* mon;
    Monitor* nextMon;

    mon = gDvm.monitorList;
    while (mon != NULL) {
        nextMon = mon->next;
        free(mon);
        mon = nextMon;
    }
}

/*
 * Get the object that a monitor is part of.
 */
Object* dvmGetMonitorObject(Monitor* mon)
{
    if (mon == NULL)
        return NULL;
    else
        return mon->obj;
}

/*
 * Returns the thread id of the thread owning the given lock.
 */
static u4 lockOwner(Object* obj)
{
    Thread *owner;
    u4 lock;

    assert(obj != NULL);
    /*
     * Since we're reading the lock value multiple times, latch it so
     * that it doesn't change out from under us if we get preempted.
     */
    lock = obj->lock;
    if (LW_SHAPE(lock) == LW_SHAPE_THIN) {
        return LW_LOCK_OWNER(lock);
    } else {
        owner = LW_MONITOR(lock)->owner;
        return owner ? owner->threadId : 0;
    }
}

/*
 * Get the thread that holds the lock on the specified object.  The
 * object may be unlocked, thin-locked, or fat-locked.
 *
 * The caller must lock the thread list before calling here.
 */
Thread* dvmGetObjectLockHolder(Object* obj)
{
    u4 threadId = lockOwner(obj);

    if (threadId == 0)
        return NULL;
    return dvmGetThreadByThreadId(threadId);
}

/*
 * Checks whether the given thread holds the given
 * objects's lock.
 */
bool dvmHoldsLock(Thread* thread, Object* obj)
{
    if (thread == NULL || obj == NULL) {
        return false;
    } else {
        return thread->threadId == lockOwner(obj);
    }
}

/*
 * Free the monitor associated with an object and make the object's lock
 * thin again.  This is called during garbage collection.
 */
static void freeMonitor(Monitor *mon)
{
    assert(mon != NULL);
    assert(mon->obj != NULL);
    assert(LW_SHAPE(mon->obj->lock) == LW_SHAPE_FAT);

    /* This lock is associated with an object
     * that's being swept.  The only possible way
     * anyone could be holding this lock would be
     * if some JNI code locked but didn't unlock
     * the object, in which case we've got some bad
     * native code somewhere.
     */
    assert(pthread_mutex_trylock(&mon->lock) == 0);
    assert(pthread_mutex_unlock(&mon->lock) == 0);
    dvmDestroyMutex(&mon->lock);
    free(mon);
}

/*
 * Frees monitor objects belonging to unmarked objects.
 */
void dvmSweepMonitorList(Monitor** mon, int (*isUnmarkedObject)(void*))
{
    Monitor handle;
    Monitor *prev, *curr;
    Object *obj;

    assert(mon != NULL);
    assert(isUnmarkedObject != NULL);
    prev = &handle;
    prev->next = curr = *mon;
    while (curr != NULL) {
        obj = curr->obj;
        if (obj != NULL && (*isUnmarkedObject)(obj) != 0) {
            prev->next = curr->next;
            freeMonitor(curr);
            curr = prev->next;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
    *mon = handle.next;
}

static char *logWriteInt(char *dst, int value)
{
    *dst++ = EVENT_TYPE_INT;
    set4LE((u1 *)dst, value);
    return dst + 4;
}

static char *logWriteString(char *dst, const char *value, size_t len)
{
    *dst++ = EVENT_TYPE_STRING;
    len = len < 32 ? len : 32;
    set4LE((u1 *)dst, len);
    dst += 4;
    memcpy(dst, value, len);
    return dst + len;
}

#define EVENT_LOG_TAG_dvm_lock_sample 20003

static void logContentionEvent(Thread *self, u4 waitMs, u4 samplePercent,
                               const char *ownerFileName, u4 ownerLineNumber)
{
    const StackSaveArea *saveArea;
    const Method *meth;
    u4 relativePc;
    char eventBuffer[174];
    const char *fileName;
    char procName[33];
    char *cp;
    size_t len;
    int fd;

    /* When a thread is being destroyed it is normal that the frame depth is zero */
    if (self->interpSave.curFrame == NULL) {
        return;
    }

    saveArea = SAVEAREA_FROM_FP(self->interpSave.curFrame);
    meth = saveArea->method;
    cp = eventBuffer;

    /* Emit the event list length, 1 byte. */
    *cp++ = 9;

    /* Emit the process name, <= 37 bytes. */
    fd = open("/proc/self/cmdline", O_RDONLY);
    memset(procName, 0, sizeof(procName));
    read(fd, procName, sizeof(procName) - 1);
    close(fd);
    len = strlen(procName);
    cp = logWriteString(cp, procName, len);

    /* Emit the sensitive thread ("main thread") status, 5 bytes. */
    bool isSensitive = false;
    if (gDvm.isSensitiveThreadHook != NULL) {
        isSensitive = gDvm.isSensitiveThreadHook();
    }
    cp = logWriteInt(cp, isSensitive);

    /* Emit self thread name string, <= 37 bytes. */
    std::string selfName = dvmGetThreadName(self);
    cp = logWriteString(cp, selfName.c_str(), selfName.size());

    /* Emit the wait time, 5 bytes. */
    cp = logWriteInt(cp, waitMs);

    /* Emit the source code file name, <= 37 bytes. */
    fileName = dvmGetMethodSourceFile(meth);
    if (fileName == NULL) fileName = "";
    cp = logWriteString(cp, fileName, strlen(fileName));

    /* Emit the source code line number, 5 bytes. */
    relativePc = saveArea->xtra.currentPc - saveArea->method->insns;
    cp = logWriteInt(cp, dvmLineNumFromPC(meth, relativePc));

    /* Emit the lock owner source code file name, <= 37 bytes. */
    if (ownerFileName == NULL) {
        ownerFileName = "";
    } else if (strcmp(fileName, ownerFileName) == 0) {
        /* Common case, so save on log space. */
        ownerFileName = "-";
    }
    cp = logWriteString(cp, ownerFileName, strlen(ownerFileName));

    /* Emit the source code line number, 5 bytes. */
    cp = logWriteInt(cp, ownerLineNumber);

    /* Emit the sample percentage, 5 bytes. */
    cp = logWriteInt(cp, samplePercent);

    assert((size_t)(cp - eventBuffer) <= sizeof(eventBuffer));
    android_btWriteLog(EVENT_LOG_TAG_dvm_lock_sample,
                       EVENT_TYPE_LIST,
                       eventBuffer,
                       (size_t)(cp - eventBuffer));
}

/*
 * Lock a monitor.
 */
static void lockMonitor(Thread* self, Monitor* mon)
{
    ThreadStatus oldStatus;
    u4 waitThreshold, samplePercent;
    u8 waitStart, waitEnd, waitMs;

    if (mon->owner == self) {
        mon->lockCount++;
        return;
    }
    if (dvmTryLockMutex(&mon->lock) != 0) {
        oldStatus = dvmChangeStatus(self, THREAD_MONITOR);
        waitThreshold = gDvm.lockProfThreshold;
        if (waitThreshold) {
            waitStart = dvmGetRelativeTimeUsec();
        }

        const Method* currentOwnerMethod = mon->ownerMethod;
        u4 currentOwnerPc = mon->ownerPc;

        dvmLockMutex(&mon->lock);
        if (waitThreshold) {
            waitEnd = dvmGetRelativeTimeUsec();
        }
        dvmChangeStatus(self, oldStatus);
        if (waitThreshold) {
            waitMs = (waitEnd - waitStart) / 1000;
            if (waitMs >= waitThreshold) {
                samplePercent = 100;
            } else {
                samplePercent = 100 * waitMs / waitThreshold;
            }
            if (samplePercent != 0 && ((u4)rand() % 100 < samplePercent)) {
                const char* currentOwnerFileName = "no_method";
                u4 currentOwnerLineNumber = 0;
                if (currentOwnerMethod != NULL) {
                    currentOwnerFileName = dvmGetMethodSourceFile(currentOwnerMethod);
                    if (currentOwnerFileName == NULL) {
                        currentOwnerFileName = "no_method_file";
                    }
                    currentOwnerLineNumber = dvmLineNumFromPC(currentOwnerMethod, currentOwnerPc);
                }
                logContentionEvent(self, waitMs, samplePercent,
                                   currentOwnerFileName, currentOwnerLineNumber);
            }
        }
    }
    mon->owner = self;
    assert(mon->lockCount == 0);

    // When debugging, save the current monitor holder for future
    // acquisition failures to use in sampled logging.
    if (gDvm.lockProfThreshold > 0) {
        mon->ownerMethod = NULL;
        mon->ownerPc = 0;
        if (self->interpSave.curFrame == NULL) {
            return;
        }
        const StackSaveArea* saveArea = SAVEAREA_FROM_FP(self->interpSave.curFrame);
        if (saveArea == NULL) {
            return;
        }
        mon->ownerMethod = saveArea->method;
        mon->ownerPc = (saveArea->xtra.currentPc - saveArea->method->insns);
    }
}

/*
 * Try to lock a monitor.
 *
 * Returns "true" on success.
 */
#ifdef WITH_COPYING_GC
static bool tryLockMonitor(Thread* self, Monitor* mon)
{
    if (mon->owner == self) {
        mon->lockCount++;
        return true;
    } else {
        if (dvmTryLockMutex(&mon->lock) == 0) {
            mon->owner = self;
            assert(mon->lockCount == 0);
            return true;
        } else {
            return false;
        }
    }
}
#endif

/*
 * Unlock a monitor.
 *
 * Returns true if the unlock succeeded.
 * If the unlock failed, an exception will be pending.
 */
static bool unlockMonitor(Thread* self, Monitor* mon)
{
    assert(self != NULL);
    assert(mon != NULL);
    if (mon->owner == self) {
        /*
         * We own the monitor, so nobody else can be in here.
         */
        if (mon->lockCount == 0) {
            mon->owner = NULL;
            mon->ownerMethod = NULL;
            mon->ownerPc = 0;
            dvmUnlockMutex(&mon->lock);
        } else {
            mon->lockCount--;
        }
    } else {
        /*
         * We don't own this, so we're not allowed to unlock it.
         * The JNI spec says that we should throw IllegalMonitorStateException
         * in this case.
         */
        dvmThrowIllegalMonitorStateException("unlock of unowned monitor");
        return false;
    }
    return true;
}

/*
 * Checks the wait set for circular structure.  Returns 0 if the list
 * is not circular.  Otherwise, returns 1.  Used only by asserts.
 */
#ifndef NDEBUG
static int waitSetCheck(Monitor *mon)
{
    Thread *fast, *slow;
    size_t n;

    assert(mon != NULL);
    fast = slow = mon->waitSet;
    n = 0;
    for (;;) {
        if (fast == NULL) return 0;
        if (fast->waitNext == NULL) return 0;
        if (fast == slow && n > 0) return 1;
        n += 2;
        fast = fast->waitNext->waitNext;
        slow = slow->waitNext;
    }
}
#endif

/*
 * Links a thread into a monitor's wait set.  The monitor lock must be
 * held by the caller of this routine.
 */
static void waitSetAppend(Monitor *mon, Thread *thread)
{
    Thread *elt;

    assert(mon != NULL);
    assert(mon->owner == dvmThreadSelf());
    assert(thread != NULL);
    assert(thread->waitNext == NULL);
    assert(waitSetCheck(mon) == 0);
    if (mon->waitSet == NULL) {
        mon->waitSet = thread;
        return;
    }
    elt = mon->waitSet;
    while (elt->waitNext != NULL) {
        elt = elt->waitNext;
    }
    elt->waitNext = thread;
}

/*
 * Unlinks a thread from a monitor's wait set.  The monitor lock must
 * be held by the caller of this routine.
 */
static void waitSetRemove(Monitor *mon, Thread *thread)
{
    Thread *elt;

    assert(mon != NULL);
    assert(mon->owner == dvmThreadSelf());
    assert(thread != NULL);
    assert(waitSetCheck(mon) == 0);
    if (mon->waitSet == NULL) {
        return;
    }
    if (mon->waitSet == thread) {
        mon->waitSet = thread->waitNext;
        thread->waitNext = NULL;
        return;
    }
    elt = mon->waitSet;
    while (elt->waitNext != NULL) {
        if (elt->waitNext == thread) {
            elt->waitNext = thread->waitNext;
            thread->waitNext = NULL;
            return;
        }
        elt = elt->waitNext;
    }
}

/*
 * Converts the given relative waiting time into an absolute time.
 */
static void absoluteTime(s8 msec, s4 nsec, struct timespec *ts)
{
    s8 endSec;

#ifdef HAVE_TIMEDWAIT_MONOTONIC
    clock_gettime(CLOCK_MONOTONIC, ts);
#else
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ts->tv_sec = tv.tv_sec;
        ts->tv_nsec = tv.tv_usec * 1000;
    }
#endif
    endSec = ts->tv_sec + msec / 1000;
    if (endSec >= 0x7fffffff) {
        ALOGV("NOTE: end time exceeds epoch");
        endSec = 0x7ffffffe;
    }
    ts->tv_sec = endSec;
    ts->tv_nsec = (ts->tv_nsec + (msec % 1000) * 1000000) + nsec;

    /* catch rollover */
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

int dvmRelativeCondWait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                        s8 msec, s4 nsec)
{
    int ret;
    struct timespec ts;
    absoluteTime(msec, nsec, &ts);
#if defined(HAVE_TIMEDWAIT_MONOTONIC)
    ret = pthread_cond_timedwait_monotonic(cond, mutex, &ts);
#else
    ret = pthread_cond_timedwait(cond, mutex, &ts);
#endif
    assert(ret == 0 || ret == ETIMEDOUT);
    return ret;
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
 * Checks to make sure that "nsec" is in the range 0-999999
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
static void waitMonitor(Thread* self, Monitor* mon, s8 msec, s4 nsec,
    bool interruptShouldThrow)
{
    struct timespec ts;
    bool wasInterrupted = false;
    bool timed;
    int ret;

    assert(self != NULL);
    assert(mon != NULL);

    /* Make sure that we hold the lock. */
    if (mon->owner != self) {
        dvmThrowIllegalMonitorStateException(
            "object not locked by thread before wait()");
        return;
    }

    /*
     * Enforce the timeout range.
     */
    if (msec < 0 || nsec < 0 || nsec > 999999) {
        dvmThrowIllegalArgumentException("timeout arguments out of range");
        return;
    }

    /*
     * Compute absolute wakeup time, if necessary.
     */
    if (msec == 0 && nsec == 0) {
        timed = false;
    } else {
        absoluteTime(msec, nsec, &ts);
        timed = true;
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
    waitSetAppend(mon, self);
    int prevLockCount = mon->lockCount;
    mon->lockCount = 0;
    mon->owner = NULL;

    const Method* savedMethod = mon->ownerMethod;
    u4 savedPc = mon->ownerPc;
    mon->ownerMethod = NULL;
    mon->ownerPc = 0;

    /*
     * Update thread status.  If the GC wakes up, it'll ignore us, knowing
     * that we won't touch any references in this state, and we'll check
     * our suspend mode before we transition out.
     */
    if (timed)
        dvmChangeStatus(self, THREAD_TIMED_WAIT);
    else
        dvmChangeStatus(self, THREAD_WAIT);

    dvmLockMutex(&self->waitMutex);

    /*
     * Set waitMonitor to the monitor object we will be waiting on.
     * When waitMonitor is non-NULL a notifying or interrupting thread
     * must signal the thread's waitCond to wake it up.
     */
    assert(self->waitMonitor == NULL);
    self->waitMonitor = mon;

    /*
     * Handle the case where the thread was interrupted before we called
     * wait().
     */
    if (self->interrupted) {
        wasInterrupted = true;
        self->waitMonitor = NULL;
        dvmUnlockMutex(&self->waitMutex);
        goto done;
    }

    /*
     * Release the monitor lock and wait for a notification or
     * a timeout to occur.
     */
    dvmUnlockMutex(&mon->lock);

    if (!timed) {
        ret = pthread_cond_wait(&self->waitCond, &self->waitMutex);
        assert(ret == 0);
    } else {
#ifdef HAVE_TIMEDWAIT_MONOTONIC
        ret = pthread_cond_timedwait_monotonic(&self->waitCond, &self->waitMutex, &ts);
#else
        ret = pthread_cond_timedwait(&self->waitCond, &self->waitMutex, &ts);
#endif
        assert(ret == 0 || ret == ETIMEDOUT);
    }
    if (self->interrupted) {
        wasInterrupted = true;
    }

    self->interrupted = false;
    self->waitMonitor = NULL;

    dvmUnlockMutex(&self->waitMutex);

    /* Reacquire the monitor lock. */
    lockMonitor(self, mon);

done:
    /*
     * We remove our thread from wait set after restoring the count
     * and owner fields so the subroutine can check that the calling
     * thread owns the monitor. Aside from that, the order of member
     * updates is not order sensitive as we hold the pthread mutex.
     */
    mon->owner = self;
    mon->lockCount = prevLockCount;
    mon->ownerMethod = savedMethod;
    mon->ownerPc = savedPc;
    waitSetRemove(mon, self);

    /* set self->status back to THREAD_RUNNING, and self-suspend if needed */
    dvmChangeStatus(self, THREAD_RUNNING);

    if (wasInterrupted) {
        /*
         * We were interrupted while waiting, or somebody interrupted an
         * un-interruptible thread earlier and we're bailing out immediately.
         *
         * The doc sayeth: "The interrupted status of the current thread is
         * cleared when this exception is thrown."
         */
        self->interrupted = false;
        if (interruptShouldThrow) {
            dvmThrowInterruptedException(NULL);
        }
    }
}

/*
 * Notify one thread waiting on this monitor.
 */
static void notifyMonitor(Thread* self, Monitor* mon)
{
    Thread* thread;

    assert(self != NULL);
    assert(mon != NULL);

    /* Make sure that we hold the lock. */
    if (mon->owner != self) {
        dvmThrowIllegalMonitorStateException(
            "object not locked by thread before notify()");
        return;
    }
    /* Signal the first waiting thread in the wait set. */
    while (mon->waitSet != NULL) {
        thread = mon->waitSet;
        mon->waitSet = thread->waitNext;
        thread->waitNext = NULL;
        dvmLockMutex(&thread->waitMutex);
        /* Check to see if the thread is still waiting. */
        if (thread->waitMonitor != NULL) {
            pthread_cond_signal(&thread->waitCond);
            dvmUnlockMutex(&thread->waitMutex);
            return;
        }
        dvmUnlockMutex(&thread->waitMutex);
    }
}

/*
 * Notify all threads waiting on this monitor.
 */
static void notifyAllMonitor(Thread* self, Monitor* mon)
{
    Thread* thread;

    assert(self != NULL);
    assert(mon != NULL);

    /* Make sure that we hold the lock. */
    if (mon->owner != self) {
        dvmThrowIllegalMonitorStateException(
            "object not locked by thread before notifyAll()");
        return;
    }
    /* Signal all threads in the wait set. */
    while (mon->waitSet != NULL) {
        thread = mon->waitSet;
        mon->waitSet = thread->waitNext;
        thread->waitNext = NULL;
        dvmLockMutex(&thread->waitMutex);
        /* Check to see if the thread is still waiting. */
        if (thread->waitMonitor != NULL) {
            pthread_cond_signal(&thread->waitCond);
        }
        dvmUnlockMutex(&thread->waitMutex);
    }
}

/*
 * Changes the shape of a monitor from thin to fat, preserving the
 * internal lock state.  The calling thread must own the lock.
 */
static void inflateMonitor(Thread *self, Object *obj)
{
    Monitor *mon;
    u4 thin;

    assert(self != NULL);
    assert(obj != NULL);
    assert(LW_SHAPE(obj->lock) == LW_SHAPE_THIN);
    assert(LW_LOCK_OWNER(obj->lock) == self->threadId);
    /* Allocate and acquire a new monitor. */
    mon = dvmCreateMonitor(obj);
    lockMonitor(self, mon);
    /* Propagate the lock state. */
    thin = obj->lock;
    mon->lockCount = LW_LOCK_COUNT(thin);
    thin &= LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT;
    thin |= (u4)mon | LW_SHAPE_FAT;
    /* Publish the updated lock word. */
    android_atomic_release_store(thin, (int32_t *)&obj->lock);
}

/*
 * Implements monitorenter for "synchronized" stuff.
 *
 * This does not fail or throw an exception (unless deadlock prediction
 * is enabled and set to "err" mode).
 */
void dvmLockObject(Thread* self, Object *obj)
{
    volatile u4 *thinp;
    ThreadStatus oldStatus;
    struct timespec tm;
    long sleepDelayNs;
    long minSleepDelayNs = 1000000;  /* 1 millisecond */
    long maxSleepDelayNs = 1000000000;  /* 1 second */
    u4 thin, newThin, threadId;

    assert(self != NULL);
    assert(obj != NULL);
    threadId = self->threadId;
    thinp = &obj->lock;
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
            obj->lock += 1 << LW_LOCK_COUNT_SHIFT;
            if (LW_LOCK_COUNT(obj->lock) == LW_LOCK_COUNT_MASK) {
                /*
                 * The reacquisition limit has been reached.  Inflate
                 * the lock so the next acquire will not overflow the
                 * recursion count field.
                 */
                inflateMonitor(self, obj);
            }
        } else if (LW_LOCK_OWNER(thin) == 0) {
            /*
             * The lock is unowned.  Install the thread id of the
             * calling thread into the owner field.  This is the
             * common case.  In performance critical code the JIT
             * will have tried this before calling out to the VM.
             */
            newThin = thin | (threadId << LW_LOCK_OWNER_SHIFT);
            if (android_atomic_acquire_cas(thin, newThin,
                    (int32_t*)thinp) != 0) {
                /*
                 * The acquire failed.  Try again.
                 */
                goto retry;
            }
        } else {
            ALOGV("(%d) spin on lock %p: %#x (%#x) %#x",
                 threadId, &obj->lock, 0, *thinp, thin);
            /*
             * The lock is owned by another thread.  Notify the VM
             * that we are about to wait.
             */
            oldStatus = dvmChangeStatus(self, THREAD_MONITOR);
            /*
             * Spin until the thin lock is released or inflated.
             */
            sleepDelayNs = 0;
            for (;;) {
                thin = *thinp;
                /*
                 * Check the shape of the lock word.  Another thread
                 * may have inflated the lock while we were waiting.
                 */
                if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
                    if (LW_LOCK_OWNER(thin) == 0) {
                        /*
                         * The lock has been released.  Install the
                         * thread id of the calling thread into the
                         * owner field.
                         */
                        newThin = thin | (threadId << LW_LOCK_OWNER_SHIFT);
                        if (android_atomic_acquire_cas(thin, newThin,
                                (int32_t *)thinp) == 0) {
                            /*
                             * The acquire succeed.  Break out of the
                             * loop and proceed to inflate the lock.
                             */
                            break;
                        }
                    } else {
                        /*
                         * The lock has not been released.  Yield so
                         * the owning thread can run.
                         */
                        if (sleepDelayNs == 0) {
                            sched_yield();
                            sleepDelayNs = minSleepDelayNs;
                        } else {
                            tm.tv_sec = 0;
                            tm.tv_nsec = sleepDelayNs;
                            nanosleep(&tm, NULL);
                            /*
                             * Prepare the next delay value.  Wrap to
                             * avoid once a second polls for eternity.
                             */
                            if (sleepDelayNs < maxSleepDelayNs / 2) {
                                sleepDelayNs *= 2;
                            } else {
                                sleepDelayNs = minSleepDelayNs;
                            }
                        }
                    }
                } else {
                    /*
                     * The thin lock was inflated by another thread.
                     * Let the VM know we are no longer waiting and
                     * try again.
                     */
                    ALOGV("(%d) lock %p surprise-fattened",
                             threadId, &obj->lock);
                    dvmChangeStatus(self, oldStatus);
                    goto retry;
                }
            }
            ALOGV("(%d) spin on lock done %p: %#x (%#x) %#x",
                 threadId, &obj->lock, 0, *thinp, thin);
            /*
             * We have acquired the thin lock.  Let the VM know that
             * we are no longer waiting.
             */
            dvmChangeStatus(self, oldStatus);
            /*
             * Fatten the lock.
             */
            inflateMonitor(self, obj);
            ALOGV("(%d) lock %p fattened", threadId, &obj->lock);
        }
    } else {
        /*
         * The lock is a fat lock.
         */
        assert(LW_MONITOR(obj->lock) != NULL);
        lockMonitor(self, LW_MONITOR(obj->lock));
    }
}

/*
 * Implements monitorexit for "synchronized" stuff.
 *
 * On failure, throws an exception and returns "false".
 */
bool dvmUnlockObject(Thread* self, Object *obj)
{
    u4 thin;

    assert(self != NULL);
    assert(self->status == THREAD_RUNNING);
    assert(obj != NULL);
    /*
     * Cache the lock word as its value can change while we are
     * examining its state.
     */
    thin = *(volatile u4 *)&obj->lock;
    if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
        /*
         * The lock is thin.  We must ensure that the lock is owned
         * by the given thread before unlocking it.
         */
        if (LW_LOCK_OWNER(thin) == self->threadId) {
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
                android_atomic_release_store(thin, (int32_t*)&obj->lock);
            } else {
                /*
                 * The object was recursively acquired.  Decrement the
                 * lock recursion count field.
                 */
                obj->lock -= 1 << LW_LOCK_COUNT_SHIFT;
            }
        } else {
            /*
             * We do not own the lock.  The JVM spec requires that we
             * throw an exception in this case.
             */
            dvmThrowIllegalMonitorStateException("unlock of unowned monitor");
            return false;
        }
    } else {
        /*
         * The lock is fat.  We must check to see if unlockMonitor has
         * raised any exceptions before continuing.
         */
        assert(LW_MONITOR(obj->lock) != NULL);
        if (!unlockMonitor(self, LW_MONITOR(obj->lock))) {
            /*
             * An exception has been raised.  Do not fall through.
             */
            return false;
        }
    }
    return true;
}

/*
 * Object.wait().  Also called for class init.
 */
void dvmObjectWait(Thread* self, Object *obj, s8 msec, s4 nsec,
    bool interruptShouldThrow)
{
    Monitor* mon;
    u4 thin = *(volatile u4 *)&obj->lock;

    /* If the lock is still thin, we need to fatten it.
     */
    if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
        /* Make sure that 'self' holds the lock.
         */
        if (LW_LOCK_OWNER(thin) != self->threadId) {
            dvmThrowIllegalMonitorStateException(
                "object not locked by thread before wait()");
            return;
        }

        /* This thread holds the lock.  We need to fatten the lock
         * so 'self' can block on it.  Don't update the object lock
         * field yet, because 'self' needs to acquire the lock before
         * any other thread gets a chance.
         */
        inflateMonitor(self, obj);
        ALOGV("(%d) lock %p fattened by wait()", self->threadId, &obj->lock);
    }
    mon = LW_MONITOR(obj->lock);
    waitMonitor(self, mon, msec, nsec, interruptShouldThrow);
}

/*
 * Object.notify().
 */
void dvmObjectNotify(Thread* self, Object *obj)
{
    u4 thin = *(volatile u4 *)&obj->lock;

    /* If the lock is still thin, there aren't any waiters;
     * waiting on an object forces lock fattening.
     */
    if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
        /* Make sure that 'self' holds the lock.
         */
        if (LW_LOCK_OWNER(thin) != self->threadId) {
            dvmThrowIllegalMonitorStateException(
                "object not locked by thread before notify()");
            return;
        }

        /* no-op;  there are no waiters to notify.
         */
    } else {
        /* It's a fat lock.
         */
        notifyMonitor(self, LW_MONITOR(thin));
    }
}

/*
 * Object.notifyAll().
 */
void dvmObjectNotifyAll(Thread* self, Object *obj)
{
    u4 thin = *(volatile u4 *)&obj->lock;

    /* If the lock is still thin, there aren't any waiters;
     * waiting on an object forces lock fattening.
     */
    if (LW_SHAPE(thin) == LW_SHAPE_THIN) {
        /* Make sure that 'self' holds the lock.
         */
        if (LW_LOCK_OWNER(thin) != self->threadId) {
            dvmThrowIllegalMonitorStateException(
                "object not locked by thread before notifyAll()");
            return;
        }

        /* no-op;  there are no waiters to notify.
         */
    } else {
        /* It's a fat lock.
         */
        notifyAllMonitor(self, LW_MONITOR(thin));
    }
}

/*
 * This implements java.lang.Thread.sleep(long msec, int nsec).
 *
 * The sleep is interruptible by other threads, which means we can't just
 * plop into an OS sleep call.  (We probably could if we wanted to send
 * signals around and rely on EINTR, but that's inefficient and relies
 * on native code respecting our signal mask.)
 *
 * We have to do all of this stuff for Object.wait() as well, so it's
 * easiest to just sleep on a private Monitor.
 *
 * It appears that we want sleep(0,0) to go through the motions of sleeping
 * for a very short duration, rather than just returning.
 */
void dvmThreadSleep(u8 msec, u4 nsec)
{
    Thread* self = dvmThreadSelf();
    Monitor* mon = gDvm.threadSleepMon;

    /* sleep(0,0) wakes up immediately, wait(0,0) means wait forever; adjust */
    if (msec == 0 && nsec == 0)
        nsec++;

    lockMonitor(self, mon);
    waitMonitor(self, mon, msec, nsec, true);
    unlockMonitor(self, mon);
}

/*
 * Implement java.lang.Thread.interrupt().
 */
void dvmThreadInterrupt(Thread* thread)
{
    assert(thread != NULL);

    dvmLockMutex(&thread->waitMutex);

    /*
     * If the interrupted flag is already set no additional action is
     * required.
     */
    if (thread->interrupted == true) {
        dvmUnlockMutex(&thread->waitMutex);
        return;
    }

    /*
     * Raise the "interrupted" flag.  This will cause it to bail early out
     * of the next wait() attempt, if it's not currently waiting on
     * something.
     */
    thread->interrupted = true;

    /*
     * Is the thread waiting?
     *
     * Note that fat vs. thin doesn't matter here;  waitMonitor
     * is only set when a thread actually waits on a monitor,
     * which implies that the monitor has already been fattened.
     */
    if (thread->waitMonitor != NULL) {
        pthread_cond_signal(&thread->waitCond);
    }

    dvmUnlockMutex(&thread->waitMutex);
}

#ifndef WITH_COPYING_GC
u4 dvmIdentityHashCode(Object *obj)
{
    return (u4)obj;
}
#else
/*
 * Returns the identity hash code of the given object.
 */
u4 dvmIdentityHashCode(Object *obj)
{
    Thread *self, *thread;
    volatile u4 *lw;
    size_t size;
    u4 lock, owner, hashState;

    if (obj == NULL) {
        /*
         * Null is defined to have an identity hash code of 0.
         */
        return 0;
    }
    lw = &obj->lock;
retry:
    hashState = LW_HASH_STATE(*lw);
    if (hashState == LW_HASH_STATE_HASHED) {
        /*
         * The object has been hashed but has not had its hash code
         * relocated by the garbage collector.  Use the raw object
         * address.
         */
        return (u4)obj >> 3;
    } else if (hashState == LW_HASH_STATE_HASHED_AND_MOVED) {
        /*
         * The object has been hashed and its hash code has been
         * relocated by the collector.  Use the value of the naturally
         * aligned word following the instance data.
         */
        assert(!dvmIsClassObject(obj));
        if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISARRAY)) {
            size = dvmArrayObjectSize((ArrayObject *)obj);
            size = (size + 2) & ~2;
        } else {
            size = obj->clazz->objectSize;
        }
        return *(u4 *)(((char *)obj) + size);
    } else if (hashState == LW_HASH_STATE_UNHASHED) {
        /*
         * The object has never been hashed.  Change the hash state to
         * hashed and use the raw object address.
         */
        self = dvmThreadSelf();
        if (self->threadId == lockOwner(obj)) {
            /*
             * We already own the lock so we can update the hash state
             * directly.
             */
            *lw |= (LW_HASH_STATE_HASHED << LW_HASH_STATE_SHIFT);
            return (u4)obj >> 3;
        }
        /*
         * We do not own the lock.  Try acquiring the lock.  Should
         * this fail, we must suspend the owning thread.
         */
        if (LW_SHAPE(*lw) == LW_SHAPE_THIN) {
            /*
             * If the lock is thin assume it is unowned.  We simulate
             * an acquire, update, and release with a single CAS.
             */
            lock = (LW_HASH_STATE_HASHED << LW_HASH_STATE_SHIFT);
            if (android_atomic_acquire_cas(
                                0,
                                (int32_t)lock,
                                (int32_t *)lw) == 0) {
                /*
                 * A new lockword has been installed with a hash state
                 * of hashed.  Use the raw object address.
                 */
                return (u4)obj >> 3;
            }
        } else {
            if (tryLockMonitor(self, LW_MONITOR(*lw))) {
                /*
                 * The monitor lock has been acquired.  Change the
                 * hash state to hashed and use the raw object
                 * address.
                 */
                *lw |= (LW_HASH_STATE_HASHED << LW_HASH_STATE_SHIFT);
                unlockMonitor(self, LW_MONITOR(*lw));
                return (u4)obj >> 3;
            }
        }
        /*
         * At this point we have failed to acquire the lock.  We must
         * identify the owning thread and suspend it.
         */
        dvmLockThreadList(self);
        /*
         * Cache the lock word as its value can change between
         * determining its shape and retrieving its owner.
         */
        lock = *lw;
        if (LW_SHAPE(lock) == LW_SHAPE_THIN) {
            /*
             * Find the thread with the corresponding thread id.
             */
            owner = LW_LOCK_OWNER(lock);
            assert(owner != self->threadId);
            /*
             * If the lock has no owner do not bother scanning the
             * thread list and fall through to the failure handler.
             */
            thread = owner ? gDvm.threadList : NULL;
            while (thread != NULL) {
                if (thread->threadId == owner) {
                    break;
                }
                thread = thread->next;
            }
        } else {
            thread = LW_MONITOR(lock)->owner;
        }
        /*
         * If thread is NULL the object has been released since the
         * thread list lock was acquired.  Try again.
         */
        if (thread == NULL) {
            dvmUnlockThreadList();
            goto retry;
        }
        /*
         * Wait for the owning thread to suspend.
         */
        dvmSuspendThread(thread);
        if (dvmHoldsLock(thread, obj)) {
            /*
             * The owning thread has been suspended.  We can safely
             * change the hash state to hashed.
             */
            *lw |= (LW_HASH_STATE_HASHED << LW_HASH_STATE_SHIFT);
            dvmResumeThread(thread);
            dvmUnlockThreadList();
            return (u4)obj >> 3;
        }
        /*
         * The wrong thread has been suspended.  Try again.
         */
        dvmResumeThread(thread);
        dvmUnlockThreadList();
        goto retry;
    }
    ALOGE("object %p has an unknown hash state %#x", obj, hashState);
    dvmDumpThread(dvmThreadSelf(), false);
    dvmAbort();
    return 0;  /* Quiet the compiler. */
}
#endif  /* WITH_COPYING_GC */
