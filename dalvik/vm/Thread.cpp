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

#define ATRACE_TAG ATRACE_TAG_DALVIK

/*
 * Thread support.
 */
#include "Dalvik.h"
#include "os/os.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#if defined(HAVE_PRCTL)
#include <sys/prctl.h>
#endif

#if defined(WITH_SELF_VERIFICATION)
#include "interp/Jit.h"         // need for self verification
#endif

 #include <cutils/trace.h>

/* desktop Linux needs a little help with gettid() */
#if defined(HAVE_GETTID) && !defined(HAVE_ANDROID_OS)
#define __KERNEL__
# include <linux/unistd.h>
#ifdef _syscall0
_syscall0(pid_t,gettid)
#else
pid_t gettid() { return syscall(__NR_gettid);}
#endif
#undef __KERNEL__
#endif

// Change this to enable logging on cgroup errors
#define ENABLE_CGROUP_ERR_LOGGING 0

// change this to ALOGV/ALOGD to debug thread activity
#define LOG_THREAD  LOGVV

/*
Notes on Threading

All threads are native pthreads.  All threads, except the JDWP debugger
thread, are visible to code running in the VM and to the debugger.  (We
don't want the debugger to try to manipulate the thread that listens for
instructions from the debugger.)  Internal VM threads are in the "system"
ThreadGroup, all others are in the "main" ThreadGroup, per convention.

The GC only runs when all threads have been suspended.  Threads are
expected to suspend themselves, using a "safe point" mechanism.  We check
for a suspend request at certain points in the main interpreter loop,
and on requests coming in from native code (e.g. all JNI functions).
Certain debugger events may inspire threads to self-suspend.

Native methods must use JNI calls to modify object references to avoid
clashes with the GC.  JNI doesn't provide a way for native code to access
arrays of objects as such -- code must always get/set individual entries --
so it should be possible to fully control access through JNI.

Internal native VM threads, such as the finalizer thread, must explicitly
check for suspension periodically.  In most cases they will be sound
asleep on a condition variable, and won't notice the suspension anyway.

Threads may be suspended by the GC, debugger, or the SIGQUIT listener
thread.  The debugger may suspend or resume individual threads, while the
GC always suspends all threads.  Each thread has a "suspend count" that
is incremented on suspend requests and decremented on resume requests.
When the count is zero, the thread is runnable.  This allows us to fulfill
a debugger requirement: if the debugger suspends a thread, the thread is
not allowed to run again until the debugger resumes it (or disconnects,
in which case we must resume all debugger-suspended threads).

Paused threads sleep on a condition variable, and are awoken en masse.
Certain "slow" VM operations, such as starting up a new thread, will be
done in a separate "VMWAIT" state, so that the rest of the VM doesn't
freeze up waiting for the operation to finish.  Threads must check for
pending suspension when leaving VMWAIT.

Because threads suspend themselves while interpreting code or when native
code makes JNI calls, there is no risk of suspending while holding internal
VM locks.  All threads can enter a suspended (or native-code-only) state.
Also, we don't have to worry about object references existing solely
in hardware registers.

We do, however, have to worry about objects that were allocated internally
and aren't yet visible to anything else in the VM.  If we allocate an
object, and then go to sleep on a mutex after changing to a non-RUNNING
state (e.g. while trying to allocate a second object), the first object
could be garbage-collected out from under us while we sleep.  To manage
this, we automatically add all allocated objects to an internal object
tracking list, and only remove them when we know we won't be suspended
before the object appears in the GC root set.

The debugger may choose to suspend or resume a single thread, which can
lead to application-level deadlocks; this is expected behavior.  The VM
will only check for suspension of single threads when the debugger is
active (the java.lang.Thread calls for this are deprecated and hence are
not supported).  Resumption of a single thread is handled by decrementing
the thread's suspend count and sending a broadcast signal to the condition
variable.  (This will cause all threads to wake up and immediately go back
to sleep, which isn't tremendously efficient, but neither is having the
debugger attached.)

The debugger is not allowed to resume threads suspended by the GC.  This
is trivially enforced by ignoring debugger requests while the GC is running
(the JDWP thread is suspended during GC).

The VM maintains a Thread struct for every pthread known to the VM.  There
is a java/lang/Thread object associated with every Thread.  At present,
there is no safe way to go from a Thread object to a Thread struct except by
locking and scanning the list; this is necessary because the lifetimes of
the two are not closely coupled.  We may want to change this behavior,
though at present the only performance impact is on the debugger (see
threadObjToThread()).  See also notes about dvmDetachCurrentThread().
*/
/*
Alternate implementation (signal-based):

Threads run without safe points -- zero overhead.  The VM uses a signal
(e.g. pthread_kill(SIGUSR1)) to notify threads of suspension or resumption.

The trouble with using signals to suspend threads is that it means a thread
can be in the middle of an operation when garbage collection starts.
To prevent some sticky situations, we have to introduce critical sections
to the VM code.

Critical sections temporarily block suspension for a given thread.
The thread must move to a non-blocked state (and self-suspend) after
finishing its current task.  If the thread blocks on a resource held
by a suspended thread, we're hosed.

One approach is to require that no blocking operations, notably
acquisition of mutexes, can be performed within a critical section.
This is too limiting.  For example, if thread A gets suspended while
holding the thread list lock, it will prevent the GC or debugger from
being able to safely access the thread list.  We need to wrap the critical
section around the entire operation (enter critical, get lock, do stuff,
release lock, exit critical).

A better approach is to declare that certain resources can only be held
within critical sections.  A thread that enters a critical section and
then gets blocked on the thread list lock knows that the thread it is
waiting for is also in a critical section, and will release the lock
before suspending itself.  Eventually all threads will complete their
operations and self-suspend.  For this to work, the VM must:

 (1) Determine the set of resources that may be accessed from the GC or
     debugger threads.  The mutexes guarding those go into the "critical
     resource set" (CRS).
 (2) Ensure that no resource in the CRS can be acquired outside of a
     critical section.  This can be verified with an assert().
 (3) Ensure that only resources in the CRS can be held while in a critical
     section.  This is harder to enforce.

If any of these conditions are not met, deadlock can ensue when grabbing
resources in the GC or debugger (#1) or waiting for threads to suspend
(#2,#3).  (You won't actually deadlock in the GC, because if the semantics
above are followed you don't need to lock anything in the GC.  The risk is
rather that the GC will access data structures in an intermediate state.)

This approach requires more care and awareness in the VM than
safe-pointing.  Because the GC and debugger are fairly intrusive, there
really aren't any internal VM resources that aren't shared.  Thus, the
enter/exit critical calls can be added to internal mutex wrappers, which
makes it easy to get #1 and #2 right.

An ordering should be established for all locks to avoid deadlocks.

Monitor locks, which are also implemented with pthread calls, should not
cause any problems here.  Threads fighting over such locks will not be in
critical sections and can be suspended freely.

This can get tricky if we ever need exclusive access to VM and non-VM
resources at the same time.  It's not clear if this is a real concern.

There are (at least) two ways to handle the incoming signals:

 (a) Always accept signals.  If we're in a critical section, the signal
     handler just returns without doing anything (the "suspend level"
     should have been incremented before the signal was sent).  Otherwise,
     if the "suspend level" is nonzero, we go to sleep.
 (b) Block signals in critical sections.  This ensures that we can't be
     interrupted in a critical section, but requires pthread_sigmask()
     calls on entry and exit.

This is a choice between blocking the message and blocking the messenger.
Because UNIX signals are unreliable (you can only know that you have been
signaled, not whether you were signaled once or 10 times), the choice is
not significant for correctness.  The choice depends on the efficiency
of pthread_sigmask() and the desire to actually block signals.  Either way,
it is best to ensure that there is only one indication of "blocked";
having two (i.e. block signals and set a flag, then only send a signal
if the flag isn't set) can lead to race conditions.

The signal handler must take care to copy registers onto the stack (via
setjmp), so that stack scans find all references.  Because we have to scan
native stacks, "exact" GC is not possible with this approach.

Some other concerns with flinging signals around:
 - Odd interactions with some debuggers (e.g. gdb on the Mac)
 - Restrictions on some standard library calls during GC (e.g. don't
   use printf on stdout to print GC debug messages)
*/

#define kMaxThreadId        ((1 << 16) - 1)
#define kMainThreadId       1


static Thread* allocThread(int interpStackSize);
static bool prepareThread(Thread* thread);
static void setThreadSelf(Thread* thread);
static void unlinkThread(Thread* thread);
static void freeThread(Thread* thread);
static void assignThreadId(Thread* thread);
static bool createFakeEntryFrame(Thread* thread);
static bool createFakeRunFrame(Thread* thread);
static void* interpThreadStart(void* arg);
static void* internalThreadStart(void* arg);
static void threadExitUncaughtException(Thread* thread, Object* group);
static void threadExitCheck(void* arg);
static void waitForThreadSuspend(Thread* self, Thread* thread);

/*
 * Initialize thread list and main thread's environment.  We need to set
 * up some basic stuff so that dvmThreadSelf() will work when we start
 * loading classes (e.g. to check for exceptions).
 */
bool dvmThreadStartup()
{
    Thread* thread;

    /* allocate a TLS slot */
    if (pthread_key_create(&gDvm.pthreadKeySelf, threadExitCheck) != 0) {
        ALOGE("ERROR: pthread_key_create failed");
        return false;
    }

    /* test our pthread lib */
    if (pthread_getspecific(gDvm.pthreadKeySelf) != NULL)
        ALOGW("WARNING: newly-created pthread TLS slot is not NULL");

    /* prep thread-related locks and conditions */
    dvmInitMutex(&gDvm.threadListLock);
    pthread_cond_init(&gDvm.threadStartCond, NULL);
    pthread_cond_init(&gDvm.vmExitCond, NULL);
    dvmInitMutex(&gDvm._threadSuspendLock);
    dvmInitMutex(&gDvm.threadSuspendCountLock);
    pthread_cond_init(&gDvm.threadSuspendCountCond, NULL);

    /*
     * Dedicated monitor for Thread.sleep().
     * TODO: change this to an Object* so we don't have to expose this
     * call, and we interact better with JDWP monitor calls.  Requires
     * deferring the object creation to much later (e.g. final "main"
     * thread prep) or until first use.
     */
    gDvm.threadSleepMon = dvmCreateMonitor(NULL);

    gDvm.threadIdMap = dvmAllocBitVector(kMaxThreadId, false);

    thread = allocThread(gDvm.mainThreadStackSize);
    if (thread == NULL)
        return false;

    /* switch mode for when we run initializers */
    thread->status = THREAD_RUNNING;

    /*
     * We need to assign the threadId early so we can lock/notify
     * object monitors.  We'll set the "threadObj" field later.
     */
    prepareThread(thread);
    gDvm.threadList = thread;

#ifdef COUNT_PRECISE_METHODS
    gDvm.preciseMethods = dvmPointerSetAlloc(200);
#endif

    return true;
}

/*
 * All threads should be stopped by now.  Clean up some thread globals.
 */
void dvmThreadShutdown()
{
    if (gDvm.threadList != NULL) {
        /*
         * If we walk through the thread list and try to free the
         * lingering thread structures (which should only be for daemon
         * threads), the daemon threads may crash if they execute before
         * the process dies.  Let them leak.
         */
        freeThread(gDvm.threadList);
        gDvm.threadList = NULL;
    }

    dvmFreeBitVector(gDvm.threadIdMap);

    dvmFreeMonitorList();

    pthread_key_delete(gDvm.pthreadKeySelf);
}


/*
 * Grab the suspend count global lock.
 */
static inline void lockThreadSuspendCount()
{
    /*
     * Don't try to change to VMWAIT here.  When we change back to RUNNING
     * we have to check for a pending suspend, which results in grabbing
     * this lock recursively.  Doesn't work with "fast" pthread mutexes.
     *
     * This lock is always held for very brief periods, so as long as
     * mutex ordering is respected we shouldn't stall.
     */
    dvmLockMutex(&gDvm.threadSuspendCountLock);
}

/*
 * Release the suspend count global lock.
 */
static inline void unlockThreadSuspendCount()
{
    dvmUnlockMutex(&gDvm.threadSuspendCountLock);
}

/*
 * Grab the thread list global lock.
 *
 * This is held while "suspend all" is trying to make everybody stop.  If
 * the shutdown is in progress, and somebody tries to grab the lock, they'll
 * have to wait for the GC to finish.  Therefore it's important that the
 * thread not be in RUNNING mode.
 *
 * We don't have to check to see if we should be suspended once we have
 * the lock.  Nobody can suspend all threads without holding the thread list
 * lock while they do it, so by definition there isn't a GC in progress.
 *
 * This function deliberately avoids the use of dvmChangeStatus(),
 * which could grab threadSuspendCountLock.  To avoid deadlock, threads
 * are required to grab the thread list lock before the thread suspend
 * count lock.  (See comment in DvmGlobals.)
 *
 * TODO: consider checking for suspend after acquiring the lock, and
 * backing off if set.  As stated above, it can't happen during normal
 * execution, but it *can* happen during shutdown when daemon threads
 * are being suspended.
 */
void dvmLockThreadList(Thread* self)
{
    ThreadStatus oldStatus;

    if (self == NULL)       /* try to get it from TLS */
        self = dvmThreadSelf();

    if (self != NULL) {
        oldStatus = self->status;
        self->status = THREAD_VMWAIT;
    } else {
        /* happens during VM shutdown */
        oldStatus = THREAD_UNDEFINED;  // shut up gcc
    }

    dvmLockMutex(&gDvm.threadListLock);

    if (self != NULL)
        self->status = oldStatus;
}

/*
 * Try to lock the thread list.
 *
 * Returns "true" if we locked it.  This is a "fast" mutex, so if the
 * current thread holds the lock this will fail.
 */
bool dvmTryLockThreadList()
{
    return (dvmTryLockMutex(&gDvm.threadListLock) == 0);
}

/*
 * Release the thread list global lock.
 */
void dvmUnlockThreadList()
{
    dvmUnlockMutex(&gDvm.threadListLock);
}

/*
 * Convert SuspendCause to a string.
 */
static const char* getSuspendCauseStr(SuspendCause why)
{
    switch (why) {
    case SUSPEND_NOT:               return "NOT?";
    case SUSPEND_FOR_GC:            return "gc";
    case SUSPEND_FOR_DEBUG:         return "debug";
    case SUSPEND_FOR_DEBUG_EVENT:   return "debug-event";
    case SUSPEND_FOR_STACK_DUMP:    return "stack-dump";
    case SUSPEND_FOR_VERIFY:        return "verify";
    case SUSPEND_FOR_HPROF:         return "hprof";
#if defined(WITH_JIT)
    case SUSPEND_FOR_TBL_RESIZE:    return "table-resize";
    case SUSPEND_FOR_IC_PATCH:      return "inline-cache-patch";
    case SUSPEND_FOR_CC_RESET:      return "reset-code-cache";
    case SUSPEND_FOR_REFRESH:       return "refresh jit status";
#endif
    default:                        return "UNKNOWN";
    }
}

/*
 * Grab the "thread suspend" lock.  This is required to prevent the
 * GC and the debugger from simultaneously suspending all threads.
 *
 * If we fail to get the lock, somebody else is trying to suspend all
 * threads -- including us.  If we go to sleep on the lock we'll deadlock
 * the VM.  Loop until we get it or somebody puts us to sleep.
 */
static void lockThreadSuspend(const char* who, SuspendCause why)
{
    const int kSpinSleepTime = 3*1000*1000;        /* 3s */
    u8 startWhen = 0;       // init req'd to placate gcc
    int sleepIter = 0;
    int cc;

    do {
        cc = dvmTryLockMutex(&gDvm._threadSuspendLock);
        if (cc != 0) {
            Thread* self = dvmThreadSelf();

            if (!dvmCheckSuspendPending(self)) {
                /*
                 * Could be that a resume-all is in progress, and something
                 * grabbed the CPU when the wakeup was broadcast.  The thread
                 * performing the resume hasn't had a chance to release the
                 * thread suspend lock.  (We release before the broadcast,
                 * so this should be a narrow window.)
                 *
                 * Could be we hit the window as a suspend was started,
                 * and the lock has been grabbed but the suspend counts
                 * haven't been incremented yet.
                 *
                 * Could be an unusual JNI thread-attach thing.
                 *
                 * Could be the debugger telling us to resume at roughly
                 * the same time we're posting an event.
                 *
                 * Could be two app threads both want to patch predicted
                 * chaining cells around the same time.
                 */
                ALOGI("threadid=%d ODD: want thread-suspend lock (%s:%s),"
                     " it's held, no suspend pending",
                    self->threadId, who, getSuspendCauseStr(why));
            } else {
                /* we suspended; reset timeout */
                sleepIter = 0;
            }

            /* give the lock-holder a chance to do some work */
            if (sleepIter == 0)
                startWhen = dvmGetRelativeTimeUsec();
            if (!dvmIterativeSleep(sleepIter++, kSpinSleepTime, startWhen)) {
                ALOGE("threadid=%d: couldn't get thread-suspend lock (%s:%s),"
                     " bailing",
                    self->threadId, who, getSuspendCauseStr(why));
                /* threads are not suspended, thread dump could crash */
                dvmDumpAllThreads(false);
                dvmAbort();
            }
        }
    } while (cc != 0);
    assert(cc == 0);
}

/*
 * Release the "thread suspend" lock.
 */
static inline void unlockThreadSuspend()
{
    dvmUnlockMutex(&gDvm._threadSuspendLock);
}


/*
 * Kill any daemon threads that still exist.  All of ours should be
 * stopped, so these should be Thread objects or JNI-attached threads
 * started by the application.  Actively-running threads are likely
 * to crash the process if they continue to execute while the VM
 * shuts down, so we really need to kill or suspend them.  (If we want
 * the VM to restart within this process, we need to kill them, but that
 * leaves open the possibility of orphaned resources.)
 *
 * Waiting for the thread to suspend may be unwise at this point, but
 * if one of these is wedged in a critical section then we probably
 * would've locked up on the last GC attempt.
 *
 * It's possible for this function to get called after a failed
 * initialization, so be careful with assumptions about the environment.
 *
 * This will be called from whatever thread calls DestroyJavaVM, usually
 * but not necessarily the main thread.  It's likely, but not guaranteed,
 * that the current thread has already been cleaned up.
 */
void dvmSlayDaemons()
{
    Thread* self = dvmThreadSelf();     // may be null
    Thread* target;
    int threadId = 0;
    bool doWait = false;

    dvmLockThreadList(self);

    if (self != NULL)
        threadId = self->threadId;

    target = gDvm.threadList;
    while (target != NULL) {
        if (target == self) {
            target = target->next;
            continue;
        }

        if (!dvmGetFieldBoolean(target->threadObj,
                gDvm.offJavaLangThread_daemon))
        {
            /* should never happen; suspend it with the rest */
            ALOGW("threadid=%d: non-daemon id=%d still running at shutdown?!",
                threadId, target->threadId);
        }

        std::string threadName(dvmGetThreadName(target));
        ALOGV("threadid=%d: suspending daemon id=%d name='%s'",
                threadId, target->threadId, threadName.c_str());

        /* mark as suspended */
        lockThreadSuspendCount();
        dvmAddToSuspendCounts(target, 1, 0);
        unlockThreadSuspendCount();
        doWait = true;

        target = target->next;
    }

    //dvmDumpAllThreads(false);

    /*
     * Unlock the thread list, relocking it later if necessary.  It's
     * possible a thread is in VMWAIT after calling dvmLockThreadList,
     * and that function *doesn't* check for pending suspend after
     * acquiring the lock.  We want to let them finish their business
     * and see the pending suspend before we continue here.
     *
     * There's no guarantee of mutex fairness, so this might not work.
     * (The alternative is to have dvmLockThreadList check for suspend
     * after acquiring the lock and back off, something we should consider.)
     */
    dvmUnlockThreadList();

    if (doWait) {
        bool complained = false;

        usleep(200 * 1000);

        dvmLockThreadList(self);

        /*
         * Sleep for a bit until the threads have suspended.  We're trying
         * to exit, so don't wait for too long.
         */
        int i;
        for (i = 0; i < 10; i++) {
            bool allSuspended = true;

            target = gDvm.threadList;
            while (target != NULL) {
                if (target == self) {
                    target = target->next;
                    continue;
                }

                if (target->status == THREAD_RUNNING) {
                    if (!complained)
                        ALOGD("threadid=%d not ready yet", target->threadId);
                    allSuspended = false;
                    /* keep going so we log each running daemon once */
                }

                target = target->next;
            }

            if (allSuspended) {
                ALOGV("threadid=%d: all daemons have suspended", threadId);
                break;
            } else {
                if (!complained) {
                    complained = true;
                    ALOGD("threadid=%d: waiting briefly for daemon suspension",
                        threadId);
                }
            }

            usleep(200 * 1000);
        }
        dvmUnlockThreadList();
    }

#if 0   /* bad things happen if they come out of JNI or "spuriously" wake up */
    /*
     * Abandon the threads and recover their resources.
     */
    target = gDvm.threadList;
    while (target != NULL) {
        Thread* nextTarget = target->next;
        unlinkThread(target);
        freeThread(target);
        target = nextTarget;
    }
#endif

    //dvmDumpAllThreads(true);
}


/*
 * Finish preparing the parts of the Thread struct required to support
 * JNI registration.
 */
bool dvmPrepMainForJni(JNIEnv* pEnv)
{
    Thread* self;

    /* main thread is always first in list at this point */
    self = gDvm.threadList;
    assert(self->threadId == kMainThreadId);

    /* create a "fake" JNI frame at the top of the main thread interp stack */
    if (!createFakeEntryFrame(self))
        return false;

    /* fill these in, since they weren't ready at dvmCreateJNIEnv time */
    dvmSetJniEnvThreadId(pEnv, self);
    dvmSetThreadJNIEnv(self, (JNIEnv*) pEnv);

    return true;
}


/*
 * Finish preparing the main thread, allocating some objects to represent
 * it.  As part of doing so, we finish initializing Thread and ThreadGroup.
 * This will execute some interpreted code (e.g. class initializers).
 */
bool dvmPrepMainThread()
{
    Thread* thread;
    Object* groupObj;
    Object* threadObj;
    Object* vmThreadObj;
    StringObject* threadNameStr;
    Method* init;
    JValue unused;

    ALOGV("+++ finishing prep on main VM thread");

    /* main thread is always first in list at this point */
    thread = gDvm.threadList;
    assert(thread->threadId == kMainThreadId);

    /*
     * Make sure the classes are initialized.  We have to do this before
     * we create an instance of them.
     */
    if (!dvmInitClass(gDvm.classJavaLangClass)) {
        ALOGE("'Class' class failed to initialize");
        return false;
    }
    if (!dvmInitClass(gDvm.classJavaLangThreadGroup) ||
        !dvmInitClass(gDvm.classJavaLangThread) ||
        !dvmInitClass(gDvm.classJavaLangVMThread))
    {
        ALOGE("thread classes failed to initialize");
        return false;
    }

    groupObj = dvmGetMainThreadGroup();
    if (groupObj == NULL)
        return false;

    /*
     * Allocate and construct a Thread with the internal-creation
     * constructor.
     */
    threadObj = dvmAllocObject(gDvm.classJavaLangThread, ALLOC_DEFAULT);
    if (threadObj == NULL) {
        ALOGE("unable to allocate main thread object");
        return false;
    }
    dvmReleaseTrackedAlloc(threadObj, NULL);

    threadNameStr = dvmCreateStringFromCstr("main");
    if (threadNameStr == NULL)
        return false;
    dvmReleaseTrackedAlloc((Object*)threadNameStr, NULL);

    init = dvmFindDirectMethodByDescriptor(gDvm.classJavaLangThread, "<init>",
            "(Ljava/lang/ThreadGroup;Ljava/lang/String;IZ)V");
    assert(init != NULL);
    dvmCallMethod(thread, init, threadObj, &unused, groupObj, threadNameStr,
        THREAD_NORM_PRIORITY, false);
    if (dvmCheckException(thread)) {
        ALOGE("exception thrown while constructing main thread object");
        return false;
    }

    /*
     * Allocate and construct a VMThread.
     */
    vmThreadObj = dvmAllocObject(gDvm.classJavaLangVMThread, ALLOC_DEFAULT);
    if (vmThreadObj == NULL) {
        ALOGE("unable to allocate main vmthread object");
        return false;
    }
    dvmReleaseTrackedAlloc(vmThreadObj, NULL);

    init = dvmFindDirectMethodByDescriptor(gDvm.classJavaLangVMThread, "<init>",
            "(Ljava/lang/Thread;)V");
    dvmCallMethod(thread, init, vmThreadObj, &unused, threadObj);
    if (dvmCheckException(thread)) {
        ALOGE("exception thrown while constructing main vmthread object");
        return false;
    }

    /* set the VMThread.vmData field to our Thread struct */
    assert(gDvm.offJavaLangVMThread_vmData != 0);
    dvmSetFieldInt(vmThreadObj, gDvm.offJavaLangVMThread_vmData, (u4)thread);

    /*
     * Stuff the VMThread back into the Thread.  From this point on, other
     * Threads will see that this Thread is running (at least, they would,
     * if there were any).
     */
    dvmSetFieldObject(threadObj, gDvm.offJavaLangThread_vmThread,
        vmThreadObj);

    thread->threadObj = threadObj;

    /*
     * Set the "context class loader" field in the system class loader.
     *
     * Retrieving the system class loader will cause invocation of
     * ClassLoader.getSystemClassLoader(), which could conceivably call
     * Thread.currentThread(), so we want the Thread to be fully configured
     * before we do this.
     */
    Object* systemLoader = dvmGetSystemClassLoader();
    if (systemLoader == NULL) {
        ALOGW("WARNING: system class loader is NULL (setting main ctxt)");
        /* keep going? */
    } else {
        dvmSetFieldObject(threadObj, gDvm.offJavaLangThread_contextClassLoader,
            systemLoader);
        dvmReleaseTrackedAlloc(systemLoader, NULL);
    }

    /* include self in non-daemon threads (mainly for AttachCurrentThread) */
    gDvm.nonDaemonThreadCount++;

    return true;
}


/*
 * Alloc and initialize a Thread struct.
 *
 * Does not create any objects, just stuff on the system (malloc) heap.
 */
static Thread* allocThread(int interpStackSize)
{
    Thread* thread;
    u1* stackBottom;

    thread = (Thread*) calloc(1, sizeof(Thread));
    if (thread == NULL)
        return NULL;

    /* Check sizes and alignment */
    assert((((uintptr_t)&thread->interpBreak.all) & 0x7) == 0);
    assert(sizeof(thread->interpBreak) == sizeof(thread->interpBreak.all));


#if defined(WITH_SELF_VERIFICATION)
    if (dvmSelfVerificationShadowSpaceAlloc(thread) == NULL)
        return NULL;
#endif

    assert(interpStackSize >= kMinStackSize && interpStackSize <=kMaxStackSize);

    thread->status = THREAD_INITIALIZING;

    /*
     * Allocate and initialize the interpreted code stack.  We essentially
     * "lose" the alloc pointer, which points at the bottom of the stack,
     * but we can get it back later because we know how big the stack is.
     *
     * The stack must be aligned on a 4-byte boundary.
     */
#ifdef MALLOC_INTERP_STACK
    stackBottom = (u1*) malloc(interpStackSize);
    if (stackBottom == NULL) {
#if defined(WITH_SELF_VERIFICATION)
        dvmSelfVerificationShadowSpaceFree(thread);
#endif
        free(thread);
        return NULL;
    }
    memset(stackBottom, 0xc5, interpStackSize);     // stop valgrind complaints
#else
    stackBottom = (u1*) mmap(NULL, interpStackSize, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (stackBottom == MAP_FAILED) {
#if defined(WITH_SELF_VERIFICATION)
        dvmSelfVerificationShadowSpaceFree(thread);
#endif
        free(thread);
        return NULL;
    }
#endif

    assert(((u4)stackBottom & 0x03) == 0); // looks like our malloc ensures this
    thread->interpStackSize = interpStackSize;
    thread->interpStackStart = stackBottom + interpStackSize;
    thread->interpStackEnd = stackBottom + STACK_OVERFLOW_RESERVE;

#ifndef DVM_NO_ASM_INTERP
    thread->mainHandlerTable = dvmAsmInstructionStart;
    thread->altHandlerTable = dvmAsmAltInstructionStart;
    thread->interpBreak.ctl.curHandlerTable = thread->mainHandlerTable;
#endif

    /* give the thread code a chance to set things up */
    dvmInitInterpStack(thread, interpStackSize);

    /* One-time setup for interpreter/JIT state */
    dvmInitInterpreterState(thread);

    return thread;
}

/*
 * Get a meaningful thread ID.  At present this only has meaning under Linux,
 * where getpid() and gettid() sometimes agree and sometimes don't depending
 * on your thread model (try "export LD_ASSUME_KERNEL=2.4.19").
 */
pid_t dvmGetSysThreadId()
{
#ifdef HAVE_GETTID
    return gettid();
#else
    return getpid();
#endif
}

/*
 * Finish initialization of a Thread struct.
 *
 * This must be called while executing in the new thread, but before the
 * thread is added to the thread list.
 *
 * NOTE: The threadListLock must be held by the caller (needed for
 * assignThreadId()).
 */
static bool prepareThread(Thread* thread)
{
    assignThreadId(thread);
    thread->handle = pthread_self();
    thread->systemTid = dvmGetSysThreadId();

    //ALOGI("SYSTEM TID IS %d (pid is %d)", (int) thread->systemTid,
    //    (int) getpid());
    /*
     * If we were called by dvmAttachCurrentThread, the self value is
     * already correctly established as "thread".
     */
    setThreadSelf(thread);

    ALOGV("threadid=%d: interp stack at %p",
        thread->threadId, thread->interpStackStart - thread->interpStackSize);

    /*
     * Initialize invokeReq.
     */
    dvmInitMutex(&thread->invokeReq.lock);
    pthread_cond_init(&thread->invokeReq.cv, NULL);

    /*
     * Initialize our reference tracking tables.
     *
     * Most threads won't use jniMonitorRefTable, so we clear out the
     * structure but don't call the init function (which allocs storage).
     */
    if (!thread->jniLocalRefTable.init(kJniLocalRefMin,
            kJniLocalRefMax, kIndirectKindLocal)) {
        return false;
    }
    if (!dvmInitReferenceTable(&thread->internalLocalRefTable,
            kInternalRefDefault, kInternalRefMax))
        return false;

    memset(&thread->jniMonitorRefTable, 0, sizeof(thread->jniMonitorRefTable));

    pthread_cond_init(&thread->waitCond, NULL);
    dvmInitMutex(&thread->waitMutex);

    /* Initialize safepoint callback mechanism */
    dvmInitMutex(&thread->callbackMutex);

    return true;
}

/*
 * Remove a thread from the internal list.
 * Clear out the links to make it obvious that the thread is
 * no longer on the list.  Caller must hold gDvm.threadListLock.
 */
static void unlinkThread(Thread* thread)
{
    LOG_THREAD("threadid=%d: removing from list", thread->threadId);
    if (thread == gDvm.threadList) {
        assert(thread->prev == NULL);
        gDvm.threadList = thread->next;
    } else {
        assert(thread->prev != NULL);
        thread->prev->next = thread->next;
    }
    if (thread->next != NULL)
        thread->next->prev = thread->prev;
    thread->prev = thread->next = NULL;
}

/*
 * Free a Thread struct, and all the stuff allocated within.
 */
static void freeThread(Thread* thread)
{
    if (thread == NULL)
        return;

    /* thread->threadId is zero at this point */
    LOGVV("threadid=%d: freeing", thread->threadId);

    if (thread->interpStackStart != NULL) {
        u1* interpStackBottom;

        interpStackBottom = thread->interpStackStart;
        interpStackBottom -= thread->interpStackSize;
#ifdef MALLOC_INTERP_STACK
        free(interpStackBottom);
#else
        if (munmap(interpStackBottom, thread->interpStackSize) != 0)
            ALOGW("munmap(thread stack) failed");
#endif
    }

    thread->jniLocalRefTable.destroy();
    dvmClearReferenceTable(&thread->internalLocalRefTable);
    if (&thread->jniMonitorRefTable.table != NULL)
        dvmClearReferenceTable(&thread->jniMonitorRefTable);

#if defined(WITH_SELF_VERIFICATION)
    dvmSelfVerificationShadowSpaceFree(thread);
#endif
    free(thread->stackTraceSample);
    free(thread);
}

/*
 * Like pthread_self(), but on a Thread*.
 */
Thread* dvmThreadSelf()
{
    return (Thread*) pthread_getspecific(gDvm.pthreadKeySelf);
}

/*
 * Explore our sense of self.  Stuffs the thread pointer into TLS.
 */
static void setThreadSelf(Thread* thread)
{
    int cc;

    cc = pthread_setspecific(gDvm.pthreadKeySelf, thread);
    if (cc != 0) {
        /*
         * Sometimes this fails under Bionic with EINVAL during shutdown.
         * This can happen if the timing is just right, e.g. a thread
         * fails to attach during shutdown, but the "fail" path calls
         * here to ensure we clean up after ourselves.
         */
        if (thread != NULL) {
            ALOGE("pthread_setspecific(%p) failed, err=%d", thread, cc);
            dvmAbort();     /* the world is fundamentally hosed */
        }
    }
}

/*
 * This is associated with the pthreadKeySelf key.  It's called by the
 * pthread library when a thread is exiting and the "self" pointer in TLS
 * is non-NULL, meaning the VM hasn't had a chance to clean up.  In normal
 * operation this will not be called.
 *
 * This is mainly of use to ensure that we don't leak resources if, for
 * example, a thread attaches itself to us with AttachCurrentThread and
 * then exits without notifying the VM.
 *
 * We could do the detach here instead of aborting, but this will lead to
 * portability problems.  Other implementations do not do this check and
 * will simply be unaware that the thread has exited, leading to resource
 * leaks (and, if this is a non-daemon thread, an infinite hang when the
 * VM tries to shut down).
 *
 * Because some implementations may want to use the pthread destructor
 * to initiate the detach, and the ordering of destructors is not defined,
 * we want to iterate a couple of times to give those a chance to run.
 */
static void threadExitCheck(void* arg)
{
    const int kMaxCount = 2;

    Thread* self = (Thread*) arg;
    assert(self != NULL);

    ALOGV("threadid=%d: threadExitCheck(%p) count=%d",
        self->threadId, arg, self->threadExitCheckCount);

    if (self->status == THREAD_ZOMBIE) {
        ALOGW("threadid=%d: Weird -- shouldn't be in threadExitCheck",
            self->threadId);
        return;
    }

    if (self->threadExitCheckCount < kMaxCount) {
        /*
         * Spin a couple of times to let other destructors fire.
         */
        ALOGD("threadid=%d: thread exiting, not yet detached (count=%d)",
            self->threadId, self->threadExitCheckCount);
        self->threadExitCheckCount++;
        int cc = pthread_setspecific(gDvm.pthreadKeySelf, self);
        if (cc != 0) {
            ALOGE("threadid=%d: unable to re-add thread to TLS",
                self->threadId);
            dvmAbort();
        }
    } else {
        ALOGE("threadid=%d: native thread exited without detaching",
            self->threadId);
        dvmAbort();
    }
}


/*
 * Assign the threadId.  This needs to be a small integer so that our
 * "thin" locks fit in a small number of bits.
 *
 * We reserve zero for use as an invalid ID.
 *
 * This must be called with threadListLock held.
 */
static void assignThreadId(Thread* thread)
{
    /*
     * Find a small unique integer.  threadIdMap is a vector of
     * kMaxThreadId bits;  dvmAllocBit() returns the index of a
     * bit, meaning that it will always be < kMaxThreadId.
     */
    int num = dvmAllocBit(gDvm.threadIdMap);
    if (num < 0) {
        ALOGE("Ran out of thread IDs");
        dvmAbort();     // TODO: make this a non-fatal error result
    }

    thread->threadId = num + 1;

    assert(thread->threadId != 0);
}

/*
 * Give back the thread ID.
 */
static void releaseThreadId(Thread* thread)
{
    assert(thread->threadId > 0);
    dvmClearBit(gDvm.threadIdMap, thread->threadId - 1);
    thread->threadId = 0;
}


/*
 * Add a stack frame that makes it look like the native code in the main
 * thread was originally invoked from interpreted code.  This gives us a
 * place to hang JNI local references.  The VM spec says (v2 5.2) that the
 * VM begins by executing "main" in a class, so in a way this brings us
 * closer to the spec.
 */
static bool createFakeEntryFrame(Thread* thread)
{
    /*
     * Because we are creating a frame that represents application code, we
     * want to stuff the application class loader into the method's class
     * loader field, even though we're using the system class loader to
     * load it.  This makes life easier over in JNI FindClass (though it
     * could bite us in other ways).
     *
     * Unfortunately this is occurring too early in the initialization,
     * of necessity coming before JNI is initialized, and we're not quite
     * ready to set up the application class loader.  Also, overwriting
     * the class' defining classloader pointer seems unwise.
     *
     * Instead, we save a pointer to the method and explicitly check for
     * it in FindClass.  The method is private so nobody else can call it.
     */

    assert(thread->threadId == kMainThreadId);      /* main thread only */

    if (!dvmPushJNIFrame(thread, gDvm.methDalvikSystemNativeStart_main))
        return false;

    /*
     * Null out the "String[] args" argument.
     */
    assert(gDvm.methDalvikSystemNativeStart_main->registersSize == 1);
    u4* framePtr = (u4*) thread->interpSave.curFrame;
    framePtr[0] = 0;

    return true;
}


/*
 * Add a stack frame that makes it look like the native thread has been
 * executing interpreted code.  This gives us a place to hang JNI local
 * references.
 */
static bool createFakeRunFrame(Thread* thread)
{
    return dvmPushJNIFrame(thread, gDvm.methDalvikSystemNativeStart_run);
}

/*
 * Helper function to set the name of the current thread
 */
static void setThreadName(const char *threadName)
{
    int hasAt = 0;
    int hasDot = 0;
    const char *s = threadName;
    while (*s) {
        if (*s == '.') hasDot = 1;
        else if (*s == '@') hasAt = 1;
        s++;
    }
    int len = s - threadName;
    if (len < 15 || hasAt || !hasDot) {
        s = threadName;
    } else {
        s = threadName + len - 15;
    }
#if defined(HAVE_ANDROID_PTHREAD_SETNAME_NP)
    /* pthread_setname_np fails rather than truncating long strings */
    char buf[16];       // MAX_TASK_COMM_LEN=16 is hard-coded into bionic
    strncpy(buf, s, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    int err = pthread_setname_np(pthread_self(), buf);
    if (err != 0) {
        ALOGW("Unable to set the name of current thread to '%s': %s",
            buf, strerror(err));
    }
#elif defined(HAVE_PRCTL)
    prctl(PR_SET_NAME, (unsigned long) s, 0, 0, 0);
#else
    ALOGD("No way to set current thread's name (%s)", s);
#endif
}

/*
 * Create a thread as a result of java.lang.Thread.start().
 *
 * We do have to worry about some concurrency problems, e.g. programs
 * that try to call Thread.start() on the same object from multiple threads.
 * (This will fail for all but one, but we have to make sure that it succeeds
 * for exactly one.)
 *
 * Some of the complexity here arises from our desire to mimic the
 * Thread vs. VMThread class decomposition we inherited.  We've been given
 * a Thread, and now we need to create a VMThread and then populate both
 * objects.  We also need to create one of our internal Thread objects.
 *
 * Pass in a stack size of 0 to get the default.
 *
 * The "threadObj" reference must be pinned by the caller to prevent the GC
 * from moving it around (e.g. added to the tracked allocation list).
 */
bool dvmCreateInterpThread(Object* threadObj, int reqStackSize)
{
    assert(threadObj != NULL);

    Thread* self = dvmThreadSelf();
    int stackSize;
    if (reqStackSize == 0)
        stackSize = gDvm.stackSize;
    else if (reqStackSize < kMinStackSize)
        stackSize = kMinStackSize;
    else if (reqStackSize > kMaxStackSize)
        stackSize = kMaxStackSize;
    else
        stackSize = reqStackSize;

    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);
    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);

    /*
     * To minimize the time spent in the critical section, we allocate the
     * vmThread object here.
     */
    Object* vmThreadObj = dvmAllocObject(gDvm.classJavaLangVMThread, ALLOC_DEFAULT);
    if (vmThreadObj == NULL)
        return false;

    Thread* newThread = allocThread(stackSize);
    if (newThread == NULL) {
        dvmReleaseTrackedAlloc(vmThreadObj, NULL);
        return false;
    }

    newThread->threadObj = threadObj;

    assert(newThread->status == THREAD_INITIALIZING);

    /*
     * We need to lock out other threads while we test and set the
     * "vmThread" field in java.lang.Thread, because we use that to determine
     * if this thread has been started before.  We use the thread list lock
     * because it's handy and we're going to need to grab it again soon
     * anyway.
     */
    dvmLockThreadList(self);

    if (dvmGetFieldObject(threadObj, gDvm.offJavaLangThread_vmThread) != NULL) {
        dvmUnlockThreadList();
        dvmThrowIllegalThreadStateException(
            "thread has already been started");
        freeThread(newThread);
        dvmReleaseTrackedAlloc(vmThreadObj, NULL);
        return false;
    }

    /*
     * There are actually three data structures: Thread (object), VMThread
     * (object), and Thread (C struct).  All of them point to at least one
     * other.
     *
     * As soon as "VMThread.vmData" is assigned, other threads can start
     * making calls into us (e.g. setPriority).
     */
    dvmSetFieldInt(vmThreadObj, gDvm.offJavaLangVMThread_vmData, (u4)newThread);
    dvmSetFieldObject(threadObj, gDvm.offJavaLangThread_vmThread, vmThreadObj);

    /*
     * Thread creation might take a while, so release the lock.
     */
    dvmUnlockThreadList();

    ThreadStatus oldStatus = dvmChangeStatus(self, THREAD_VMWAIT);
    pthread_t threadHandle;
    int cc = pthread_create(&threadHandle, &threadAttr, interpThreadStart, newThread);
    pthread_attr_destroy(&threadAttr);
    dvmChangeStatus(self, oldStatus);

    if (cc != 0) {
        /*
         * Failure generally indicates that we have exceeded system
         * resource limits.  VirtualMachineError is probably too severe,
         * so use OutOfMemoryError.
         */

        dvmSetFieldObject(threadObj, gDvm.offJavaLangThread_vmThread, NULL);

        ALOGE("pthread_create (stack size %d bytes) failed: %s", stackSize, strerror(cc));
        dvmThrowExceptionFmt(gDvm.exOutOfMemoryError,
                             "pthread_create (stack size %d bytes) failed: %s",
                             stackSize, strerror(cc));
        goto fail;
    }

    /*
     * We need to wait for the thread to start.  Otherwise, depending on
     * the whims of the OS scheduler, we could return and the code in our
     * thread could try to do operations on the new thread before it had
     * finished starting.
     *
     * The new thread will lock the thread list, change its state to
     * THREAD_STARTING, broadcast to gDvm.threadStartCond, and then sleep
     * on gDvm.threadStartCond (which uses the thread list lock).  This
     * thread (the parent) will either see that the thread is already ready
     * after we grab the thread list lock, or will be awakened from the
     * condition variable on the broadcast.
     *
     * We don't want to stall the rest of the VM while the new thread
     * starts, which can happen if the GC wakes up at the wrong moment.
     * So, we change our own status to VMWAIT, and self-suspend if
     * necessary after we finish adding the new thread.
     *
     *
     * We have to deal with an odd race with the GC/debugger suspension
     * mechanism when creating a new thread.  The information about whether
     * or not a thread should be suspended is contained entirely within
     * the Thread struct; this is usually cleaner to deal with than having
     * one or more globally-visible suspension flags.  The trouble is that
     * we could create the thread while the VM is trying to suspend all
     * threads.  The suspend-count won't be nonzero for the new thread,
     * so dvmChangeStatus(THREAD_RUNNING) won't cause a suspension.
     *
     * The easiest way to deal with this is to prevent the new thread from
     * running until the parent says it's okay.  This results in the
     * following (correct) sequence of events for a "badly timed" GC
     * (where '-' is us, 'o' is the child, and '+' is some other thread):
     *
     *  - call pthread_create()
     *  - lock thread list
     *  - put self into THREAD_VMWAIT so GC doesn't wait for us
     *  - sleep on condition var (mutex = thread list lock) until child starts
     *  + GC triggered by another thread
     *  + thread list locked; suspend counts updated; thread list unlocked
     *  + loop waiting for all runnable threads to suspend
     *  + success, start GC
     *  o child thread wakes, signals condition var to wake parent
     *  o child waits for parent ack on condition variable
     *  - we wake up, locking thread list
     *  - add child to thread list
     *  - unlock thread list
     *  - change our state back to THREAD_RUNNING; GC causes us to suspend
     *  + GC finishes; all threads in thread list are resumed
     *  - lock thread list
     *  - set child to THREAD_VMWAIT, and signal it to start
     *  - unlock thread list
     *  o child resumes
     *  o child changes state to THREAD_RUNNING
     *
     * The above shows the GC starting up during thread creation, but if
     * it starts anywhere after VMThread.create() is called it will
     * produce the same series of events.
     *
     * Once the child is in the thread list, it will be suspended and
     * resumed like any other thread.  In the above scenario the resume-all
     * code will try to resume the new thread, which was never actually
     * suspended, and try to decrement the child's thread suspend count to -1.
     * We can catch this in the resume-all code.
     *
     * Bouncing back and forth between threads like this adds a small amount
     * of scheduler overhead to thread startup.
     *
     * One alternative to having the child wait for the parent would be
     * to have the child inherit the parents' suspension count.  This
     * would work for a GC, since we can safely assume that the parent
     * thread didn't cause it, but we must only do so if the parent suspension
     * was caused by a suspend-all.  If the parent was being asked to
     * suspend singly by the debugger, the child should not inherit the value.
     *
     * We could also have a global "new thread suspend count" that gets
     * picked up by new threads before changing state to THREAD_RUNNING.
     * This would be protected by the thread list lock and set by a
     * suspend-all.
     */
    dvmLockThreadList(self);
    assert(self->status == THREAD_RUNNING);
    self->status = THREAD_VMWAIT;
    while (newThread->status != THREAD_STARTING)
        pthread_cond_wait(&gDvm.threadStartCond, &gDvm.threadListLock);

    LOG_THREAD("threadid=%d: adding to list", newThread->threadId);
    newThread->next = gDvm.threadList->next;
    if (newThread->next != NULL)
        newThread->next->prev = newThread;
    newThread->prev = gDvm.threadList;
    gDvm.threadList->next = newThread;

    /* Add any existing global modes to the interpBreak control */
    dvmInitializeInterpBreak(newThread);

    if (!dvmGetFieldBoolean(threadObj, gDvm.offJavaLangThread_daemon))
        gDvm.nonDaemonThreadCount++;        // guarded by thread list lock

    dvmUnlockThreadList();

    /* change status back to RUNNING, self-suspending if necessary */
    dvmChangeStatus(self, THREAD_RUNNING);

    /*
     * Tell the new thread to start.
     *
     * We must hold the thread list lock before messing with another thread.
     * In the general case we would also need to verify that newThread was
     * still in the thread list, but in our case the thread has not started
     * executing user code and therefore has not had a chance to exit.
     *
     * We move it to VMWAIT, and it then shifts itself to RUNNING, which
     * comes with a suspend-pending check.
     */
    dvmLockThreadList(self);

    assert(newThread->status == THREAD_STARTING);
    newThread->status = THREAD_VMWAIT;
    pthread_cond_broadcast(&gDvm.threadStartCond);

    dvmUnlockThreadList();

    dvmReleaseTrackedAlloc(vmThreadObj, NULL);
    return true;

fail:
    freeThread(newThread);
    dvmReleaseTrackedAlloc(vmThreadObj, NULL);
    return false;
}

/*
 * pthread entry function for threads started from interpreted code.
 */
static void* interpThreadStart(void* arg)
{
    Thread* self = (Thread*) arg;

    std::string threadName(dvmGetThreadName(self));
    setThreadName(threadName.c_str());

    /*
     * Finish initializing the Thread struct.
     */
    dvmLockThreadList(self);
    prepareThread(self);

    LOG_THREAD("threadid=%d: created from interp", self->threadId);

    /*
     * Change our status and wake our parent, who will add us to the
     * thread list and advance our state to VMWAIT.
     */
    self->status = THREAD_STARTING;
    pthread_cond_broadcast(&gDvm.threadStartCond);

    /*
     * Wait until the parent says we can go.  Assuming there wasn't a
     * suspend pending, this will happen immediately.  When it completes,
     * we're full-fledged citizens of the VM.
     *
     * We have to use THREAD_VMWAIT here rather than THREAD_RUNNING
     * because the pthread_cond_wait below needs to reacquire a lock that
     * suspend-all is also interested in.  If we get unlucky, the parent could
     * change us to THREAD_RUNNING, then a GC could start before we get
     * signaled, and suspend-all will grab the thread list lock and then
     * wait for us to suspend.  We'll be in the tail end of pthread_cond_wait
     * trying to get the lock.
     */
    while (self->status != THREAD_VMWAIT)
        pthread_cond_wait(&gDvm.threadStartCond, &gDvm.threadListLock);

    dvmUnlockThreadList();

    /*
     * Add a JNI context.
     */
    self->jniEnv = dvmCreateJNIEnv(self);

    /*
     * Change our state so the GC will wait for us from now on.  If a GC is
     * in progress this call will suspend us.
     */
    dvmChangeStatus(self, THREAD_RUNNING);

    /*
     * Notify the debugger & DDM.  The debugger notification may cause
     * us to suspend ourselves (and others).  The thread state may change
     * to VMWAIT briefly if network packets are sent.
     */
    if (gDvm.debuggerConnected)
        dvmDbgPostThreadStart(self);

    /*
     * Set the system thread priority according to the Thread object's
     * priority level.  We don't usually need to do this, because both the
     * Thread object and system thread priorities inherit from parents.  The
     * tricky case is when somebody creates a Thread object, calls
     * setPriority(), and then starts the thread.  We could manage this with
     * a "needs priority update" flag to avoid the redundant call.
     */
    int priority = dvmGetFieldInt(self->threadObj,
                        gDvm.offJavaLangThread_priority);
    dvmChangeThreadPriority(self, priority);

    /*
     * Execute the "run" method.
     *
     * At this point our stack is empty, so somebody who comes looking for
     * stack traces right now won't have much to look at.  This is normal.
     */
    Method* run = self->threadObj->clazz->vtable[gDvm.voffJavaLangThread_run];
    JValue unused;

    ALOGV("threadid=%d: calling run()", self->threadId);
    assert(strcmp(run->name, "run") == 0);
    dvmCallMethod(self, run, self->threadObj, &unused);
    ALOGV("threadid=%d: exiting", self->threadId);

    /*
     * Remove the thread from various lists, report its death, and free
     * its resources.
     */
    dvmDetachCurrentThread();

    return NULL;
}

/*
 * The current thread is exiting with an uncaught exception.  The
 * Java programming language allows the application to provide a
 * thread-exit-uncaught-exception handler for the VM, for a specific
 * Thread, and for all threads in a ThreadGroup.
 *
 * Version 1.5 added the per-thread handler.  We need to call
 * "uncaughtException" in the handler object, which is either the
 * ThreadGroup object or the Thread-specific handler.
 *
 * This should only be called when an exception is pending.  Before
 * returning, the exception will be cleared.
 */
static void threadExitUncaughtException(Thread* self, Object* group)
{
    Object* exception;
    Object* handlerObj;
    Method* uncaughtHandler;

    ALOGW("threadid=%d: thread exiting with uncaught exception (group=%p)",
        self->threadId, group);
    assert(group != NULL);

    /*
     * Get a pointer to the exception, then clear out the one in the
     * thread.  We don't want to have it set when executing interpreted code.
     */
    exception = dvmGetException(self);
    assert(exception != NULL);
    dvmAddTrackedAlloc(exception, self);
    dvmClearException(self);

    /*
     * Get the Thread's "uncaughtHandler" object.  Use it if non-NULL;
     * else use "group" (which is an instance of UncaughtExceptionHandler).
     * The ThreadGroup will handle it directly or call the default
     * uncaught exception handler.
     */
    handlerObj = dvmGetFieldObject(self->threadObj,
            gDvm.offJavaLangThread_uncaughtHandler);
    if (handlerObj == NULL)
        handlerObj = group;

    /*
     * Find the "uncaughtException" method in this object.  The method
     * was declared in the Thread.UncaughtExceptionHandler interface.
     */
    uncaughtHandler = dvmFindVirtualMethodHierByDescriptor(handlerObj->clazz,
            "uncaughtException", "(Ljava/lang/Thread;Ljava/lang/Throwable;)V");

    if (uncaughtHandler != NULL) {
        //ALOGI("+++ calling %s.uncaughtException",
        //     handlerObj->clazz->descriptor);
        JValue unused;
        dvmCallMethod(self, uncaughtHandler, handlerObj, &unused,
            self->threadObj, exception);
    } else {
        /* should be impossible, but handle it anyway */
        ALOGW("WARNING: no 'uncaughtException' method in class %s",
            handlerObj->clazz->descriptor);
        dvmSetException(self, exception);
        dvmLogExceptionStackTrace();
    }

    /* if the uncaught handler threw, clear it */
    dvmClearException(self);

    dvmReleaseTrackedAlloc(exception, self);

    /* Remove this thread's suspendCount from global suspendCount sum */
    lockThreadSuspendCount();
    dvmAddToSuspendCounts(self, -self->suspendCount, 0);
    unlockThreadSuspendCount();
}


/*
 * Create an internal VM thread, for things like JDWP and finalizers.
 *
 * The easiest way to do this is create a new thread and then use the
 * JNI AttachCurrentThread implementation.
 *
 * This does not return until after the new thread has begun executing.
 */
bool dvmCreateInternalThread(pthread_t* pHandle, const char* name,
    InternalThreadStart func, void* funcArg)
{
    InternalStartArgs* pArgs;
    Object* systemGroup;
    volatile Thread* newThread = NULL;
    volatile int createStatus = 0;

    systemGroup = dvmGetSystemThreadGroup();
    if (systemGroup == NULL)
        return false;

    pArgs = (InternalStartArgs*) malloc(sizeof(*pArgs));
    pArgs->func = func;
    pArgs->funcArg = funcArg;
    pArgs->name = strdup(name);     // storage will be owned by new thread
    pArgs->group = systemGroup;
    pArgs->isDaemon = true;
    pArgs->pThread = &newThread;
    pArgs->pCreateStatus = &createStatus;

    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);

    int cc = pthread_create(pHandle, &threadAttr, internalThreadStart, pArgs);
    pthread_attr_destroy(&threadAttr);
    if (cc != 0) {
        ALOGE("internal thread creation failed: %s", strerror(cc));
        free(pArgs->name);
        free(pArgs);
        return false;
    }

    /*
     * Wait for the child to start.  This gives us an opportunity to make
     * sure that the thread started correctly, and allows our caller to
     * assume that the thread has started running.
     *
     * Because we aren't holding a lock across the thread creation, it's
     * possible that the child will already have completed its
     * initialization.  Because the child only adjusts "createStatus" while
     * holding the thread list lock, the initial condition on the "while"
     * loop will correctly avoid the wait if this occurs.
     *
     * It's also possible that we'll have to wait for the thread to finish
     * being created, and as part of allocating a Thread object it might
     * need to initiate a GC.  We switch to VMWAIT while we pause.
     */
    Thread* self = dvmThreadSelf();
    ThreadStatus oldStatus = dvmChangeStatus(self, THREAD_VMWAIT);
    dvmLockThreadList(self);
    while (createStatus == 0)
        pthread_cond_wait(&gDvm.threadStartCond, &gDvm.threadListLock);

    if (newThread == NULL) {
        ALOGW("internal thread create failed (createStatus=%d)", createStatus);
        assert(createStatus < 0);
        /* don't free pArgs -- if pthread_create succeeded, child owns it */
        dvmUnlockThreadList();
        dvmChangeStatus(self, oldStatus);
        return false;
    }

    /* thread could be in any state now (except early init states) */
    //assert(newThread->status == THREAD_RUNNING);

    dvmUnlockThreadList();
    dvmChangeStatus(self, oldStatus);

    return true;
}

/*
 * pthread entry function for internally-created threads.
 *
 * We are expected to free "arg" and its contents.  If we're a daemon
 * thread, and we get cancelled abruptly when the VM shuts down, the
 * storage won't be freed.  If this becomes a concern we can make a copy
 * on the stack.
 */
static void* internalThreadStart(void* arg)
{
    InternalStartArgs* pArgs = (InternalStartArgs*) arg;
    JavaVMAttachArgs jniArgs;

    jniArgs.version = JNI_VERSION_1_2;
    jniArgs.name = pArgs->name;
    jniArgs.group = reinterpret_cast<jobject>(pArgs->group);

    setThreadName(pArgs->name);

    /* use local jniArgs as stack top */
    if (dvmAttachCurrentThread(&jniArgs, pArgs->isDaemon)) {
        /*
         * Tell the parent of our success.
         *
         * threadListLock is the mutex for threadStartCond.
         */
        dvmLockThreadList(dvmThreadSelf());
        *pArgs->pCreateStatus = 1;
        *pArgs->pThread = dvmThreadSelf();
        pthread_cond_broadcast(&gDvm.threadStartCond);
        dvmUnlockThreadList();

        LOG_THREAD("threadid=%d: internal '%s'",
            dvmThreadSelf()->threadId, pArgs->name);

        /* execute */
        (*pArgs->func)(pArgs->funcArg);

        /* detach ourselves */
        dvmDetachCurrentThread();
    } else {
        /*
         * Tell the parent of our failure.  We don't have a Thread struct,
         * so we can't be suspended, so we don't need to enter a critical
         * section.
         */
        dvmLockThreadList(dvmThreadSelf());
        *pArgs->pCreateStatus = -1;
        assert(*pArgs->pThread == NULL);
        pthread_cond_broadcast(&gDvm.threadStartCond);
        dvmUnlockThreadList();

        assert(*pArgs->pThread == NULL);
    }

    free(pArgs->name);
    free(pArgs);
    return NULL;
}

/*
 * Attach the current thread to the VM.
 *
 * Used for internally-created threads and JNI's AttachCurrentThread.
 */
bool dvmAttachCurrentThread(const JavaVMAttachArgs* pArgs, bool isDaemon)
{
    Thread* self = NULL;
    Object* threadObj = NULL;
    Object* vmThreadObj = NULL;
    StringObject* threadNameStr = NULL;
    Method* init;
    bool ok, ret;

    /* allocate thread struct, and establish a basic sense of self */
    self = allocThread(gDvm.stackSize);
    if (self == NULL)
        goto fail;
    setThreadSelf(self);

    /*
     * Finish our thread prep.  We need to do this before adding ourselves
     * to the thread list or invoking any interpreted code.  prepareThread()
     * requires that we hold the thread list lock.
     */
    dvmLockThreadList(self);
    ok = prepareThread(self);
    dvmUnlockThreadList();
    if (!ok)
        goto fail;

    self->jniEnv = dvmCreateJNIEnv(self);
    if (self->jniEnv == NULL)
        goto fail;

    /*
     * Create a "fake" JNI frame at the top of the main thread interp stack.
     * It isn't really necessary for the internal threads, but it gives
     * the debugger something to show.  It is essential for the JNI-attached
     * threads.
     */
    if (!createFakeRunFrame(self))
        goto fail;

    /*
     * The native side of the thread is ready; add it to the list.  Once
     * it's on the list the thread is visible to the JDWP code and the GC.
     */
    LOG_THREAD("threadid=%d: adding to list (attached)", self->threadId);

    dvmLockThreadList(self);

    self->next = gDvm.threadList->next;
    if (self->next != NULL)
        self->next->prev = self;
    self->prev = gDvm.threadList;
    gDvm.threadList->next = self;
    if (!isDaemon)
        gDvm.nonDaemonThreadCount++;

    dvmUnlockThreadList();

    /*
     * Switch state from initializing to running.
     *
     * It's possible that a GC began right before we added ourselves
     * to the thread list, and is still going.  That means our thread
     * suspend count won't reflect the fact that we should be suspended.
     * To deal with this, we transition to VMWAIT, pulse the heap lock,
     * and then advance to RUNNING.  That will ensure that we stall until
     * the GC completes.
     *
     * Once we're in RUNNING, we're like any other thread in the VM (except
     * for the lack of an initialized threadObj).  We're then free to
     * allocate and initialize objects.
     */
    assert(self->status == THREAD_INITIALIZING);
    dvmChangeStatus(self, THREAD_VMWAIT);
    dvmLockMutex(&gDvm.gcHeapLock);
    dvmUnlockMutex(&gDvm.gcHeapLock);
    dvmChangeStatus(self, THREAD_RUNNING);

    /*
     * Create Thread and VMThread objects.
     */
    threadObj = dvmAllocObject(gDvm.classJavaLangThread, ALLOC_DEFAULT);
    vmThreadObj = dvmAllocObject(gDvm.classJavaLangVMThread, ALLOC_DEFAULT);
    if (threadObj == NULL || vmThreadObj == NULL)
        goto fail_unlink;

    /*
     * This makes threadObj visible to the GC.  We still have it in the
     * tracked allocation table, so it can't move around on us.
     */
    self->threadObj = threadObj;
    dvmSetFieldInt(vmThreadObj, gDvm.offJavaLangVMThread_vmData, (u4)self);

    /*
     * Create a string for the thread name.
     */
    if (pArgs->name != NULL) {
        threadNameStr = dvmCreateStringFromCstr(pArgs->name);
        if (threadNameStr == NULL) {
            assert(dvmCheckException(dvmThreadSelf()));
            goto fail_unlink;
        }
    }

    init = dvmFindDirectMethodByDescriptor(gDvm.classJavaLangThread, "<init>",
            "(Ljava/lang/ThreadGroup;Ljava/lang/String;IZ)V");
    if (init == NULL) {
        assert(dvmCheckException(self));
        goto fail_unlink;
    }

    /*
     * Now we're ready to run some interpreted code.
     *
     * We need to construct the Thread object and set the VMThread field.
     * Setting VMThread tells interpreted code that we're alive.
     *
     * Call the (group, name, priority, daemon) constructor on the Thread.
     * This sets the thread's name and adds it to the specified group, and
     * provides values for priority and daemon (which are normally inherited
     * from the current thread).
     */
    JValue unused;
    dvmCallMethod(self, init, threadObj, &unused, (Object*)pArgs->group,
            threadNameStr, os_getThreadPriorityFromSystem(), isDaemon);
    if (dvmCheckException(self)) {
        ALOGE("exception thrown while constructing attached thread object");
        goto fail_unlink;
    }

    /*
     * Set the VMThread field, which tells interpreted code that we're alive.
     *
     * The risk of a thread start collision here is very low; somebody
     * would have to be deliberately polling the ThreadGroup list and
     * trying to start threads against anything it sees, which would
     * generally cause problems for all thread creation.  However, for
     * correctness we test "vmThread" before setting it.
     *
     * TODO: this still has a race, it's just smaller.  Not sure this is
     * worth putting effort into fixing.  Need to hold a lock while
     * fiddling with the field, or maybe initialize the Thread object in a
     * way that ensures another thread can't call start() on it.
     */
    if (dvmGetFieldObject(threadObj, gDvm.offJavaLangThread_vmThread) != NULL) {
        ALOGW("WOW: thread start hijack");
        dvmThrowIllegalThreadStateException(
            "thread has already been started");
        /* We don't want to free anything associated with the thread
         * because someone is obviously interested in it.  Just let
         * it go and hope it will clean itself up when its finished.
         * This case should never happen anyway.
         *
         * Since we're letting it live, we need to finish setting it up.
         * We just have to let the caller know that the intended operation
         * has failed.
         *
         * [ This seems strange -- stepping on the vmThread object that's
         * already present seems like a bad idea.  TODO: figure this out. ]
         */
        ret = false;
    } else {
        ret = true;
    }
    dvmSetFieldObject(threadObj, gDvm.offJavaLangThread_vmThread, vmThreadObj);

    /* we can now safely un-pin these */
    dvmReleaseTrackedAlloc(threadObj, self);
    dvmReleaseTrackedAlloc(vmThreadObj, self);
    dvmReleaseTrackedAlloc((Object*)threadNameStr, self);

    LOG_THREAD("threadid=%d: attached from native, name=%s",
        self->threadId, pArgs->name);

    /* tell the debugger & DDM */
    if (gDvm.debuggerConnected)
        dvmDbgPostThreadStart(self);

    return ret;

fail_unlink:
    dvmLockThreadList(self);
    unlinkThread(self);
    if (!isDaemon)
        gDvm.nonDaemonThreadCount--;
    dvmUnlockThreadList();
    /* fall through to "fail" */
fail:
    dvmReleaseTrackedAlloc(threadObj, self);
    dvmReleaseTrackedAlloc(vmThreadObj, self);
    dvmReleaseTrackedAlloc((Object*)threadNameStr, self);
    if (self != NULL) {
        if (self->jniEnv != NULL) {
            dvmDestroyJNIEnv(self->jniEnv);
            self->jniEnv = NULL;
        }
        freeThread(self);
    }
    setThreadSelf(NULL);
    return false;
}

/*
 * Detach the thread from the various data structures, notify other threads
 * that are waiting to "join" it, and free up all heap-allocated storage.
 *
 * Used for all threads.
 *
 * When we get here the interpreted stack should be empty.  The JNI 1.6 spec
 * requires us to enforce this for the DetachCurrentThread call, probably
 * because it also says that DetachCurrentThread causes all monitors
 * associated with the thread to be released.  (Because the stack is empty,
 * we only have to worry about explicit JNI calls to MonitorEnter.)
 *
 * THOUGHT:
 * We might want to avoid freeing our internal Thread structure until the
 * associated Thread/VMThread objects get GCed.  Our Thread is impossible to
 * get to once the thread shuts down, but there is a small possibility of
 * an operation starting in another thread before this thread halts, and
 * finishing much later (perhaps the thread got stalled by a weird OS bug).
 * We don't want something like Thread.isInterrupted() crawling through
 * freed storage.  Can do with a Thread finalizer, or by creating a
 * dedicated ThreadObject class for java/lang/Thread and moving all of our
 * state into that.
 */
void dvmDetachCurrentThread()
{
    Thread* self = dvmThreadSelf();
    Object* vmThread;
    Object* group;

    /*
     * Make sure we're not detaching a thread that's still running.  (This
     * could happen with an explicit JNI detach call.)
     *
     * A thread created by interpreted code will finish with a depth of
     * zero, while a JNI-attached thread will have the synthetic "stack
     * starter" native method at the top.
     */
    int curDepth = dvmComputeExactFrameDepth(self->interpSave.curFrame);
    if (curDepth != 0) {
        bool topIsNative = false;

        if (curDepth == 1) {
            /* not expecting a lingering break frame; just look at curFrame */
            assert(!dvmIsBreakFrame((u4*)self->interpSave.curFrame));
            StackSaveArea* ssa = SAVEAREA_FROM_FP(self->interpSave.curFrame);
            if (dvmIsNativeMethod(ssa->method))
                topIsNative = true;
        }

        if (!topIsNative) {
            ALOGE("ERROR: detaching thread with interp frames (count=%d)",
                curDepth);
            dvmDumpThread(self, false);
            dvmAbort();
        }
    }

    group = dvmGetFieldObject(self->threadObj, gDvm.offJavaLangThread_group);
    LOG_THREAD("threadid=%d: detach (group=%p)", self->threadId, group);

    /*
     * Release any held monitors.  Since there are no interpreted stack
     * frames, the only thing left are the monitors held by JNI MonitorEnter
     * calls.
     */
    dvmReleaseJniMonitors(self);

    /*
     * Do some thread-exit uncaught exception processing if necessary.
     */
    if (dvmCheckException(self))
        threadExitUncaughtException(self, group);

    /*
     * Remove the thread from the thread group.
     */
    if (group != NULL) {
        Method* removeThread =
            group->clazz->vtable[gDvm.voffJavaLangThreadGroup_removeThread];
        JValue unused;
        dvmCallMethod(self, removeThread, group, &unused, self->threadObj);
    }

    /*
     * Clear the vmThread reference in the Thread object.  Interpreted code
     * will now see that this Thread is not running.  As this may be the
     * only reference to the VMThread object that the VM knows about, we
     * have to create an internal reference to it first.
     */
    vmThread = dvmGetFieldObject(self->threadObj,
                    gDvm.offJavaLangThread_vmThread);
    dvmAddTrackedAlloc(vmThread, self);
    dvmSetFieldObject(self->threadObj, gDvm.offJavaLangThread_vmThread, NULL);

    /* clear out our struct Thread pointer, since it's going away */
    dvmSetFieldObject(vmThread, gDvm.offJavaLangVMThread_vmData, NULL);

    /*
     * Tell the debugger & DDM.  This may cause the current thread or all
     * threads to suspend.
     *
     * The JDWP spec is somewhat vague about when this happens, other than
     * that it's issued by the dying thread, which may still appear in
     * an "all threads" listing.
     */
    if (gDvm.debuggerConnected)
        dvmDbgPostThreadDeath(self);

    /*
     * Thread.join() is implemented as an Object.wait() on the VMThread
     * object.  Signal anyone who is waiting.
     */
    dvmLockObject(self, vmThread);
    dvmObjectNotifyAll(self, vmThread);
    dvmUnlockObject(self, vmThread);

    dvmReleaseTrackedAlloc(vmThread, self);
    vmThread = NULL;

    /*
     * We're done manipulating objects, so it's okay if the GC runs in
     * parallel with us from here out.  It's important to do this if
     * profiling is enabled, since we can wait indefinitely.
     */
    volatile void* raw = reinterpret_cast<volatile void*>(&self->status);
    volatile int32_t* addr = reinterpret_cast<volatile int32_t*>(raw);
    android_atomic_release_store(THREAD_VMWAIT, addr);

    /*
     * If we're doing method trace profiling, we don't want threads to exit,
     * because if they do we'll end up reusing thread IDs.  This complicates
     * analysis and makes it impossible to have reasonable output in the
     * "threads" section of the "key" file.
     *
     * We need to do this after Thread.join() completes, or other threads
     * could get wedged.  Since self->threadObj is still valid, the Thread
     * object will not get GCed even though we're no longer in the ThreadGroup
     * list (which is important since the profiling thread needs to get
     * the thread's name).
     */
    MethodTraceState* traceState = &gDvm.methodTrace;

    dvmLockMutex(&traceState->startStopLock);
    if (traceState->traceEnabled) {
        ALOGI("threadid=%d: waiting for method trace to finish",
            self->threadId);
        while (traceState->traceEnabled) {
            dvmWaitCond(&traceState->threadExitCond,
                        &traceState->startStopLock);
        }
    }
    dvmUnlockMutex(&traceState->startStopLock);

    dvmLockThreadList(self);

    /*
     * Lose the JNI context.
     */
    dvmDestroyJNIEnv(self->jniEnv);
    self->jniEnv = NULL;

    self->status = THREAD_ZOMBIE;

    /*
     * Remove ourselves from the internal thread list.
     */
    unlinkThread(self);

    /*
     * If we're the last one standing, signal anybody waiting in
     * DestroyJavaVM that it's okay to exit.
     */
    if (!dvmGetFieldBoolean(self->threadObj, gDvm.offJavaLangThread_daemon)) {
        gDvm.nonDaemonThreadCount--;        // guarded by thread list lock

        if (gDvm.nonDaemonThreadCount == 0) {
            ALOGV("threadid=%d: last non-daemon thread", self->threadId);
            //dvmDumpAllThreads(false);
            // cond var guarded by threadListLock, which we already hold
            int cc = pthread_cond_signal(&gDvm.vmExitCond);
            if (cc != 0) {
                ALOGE("pthread_cond_signal(&gDvm.vmExitCond) failed: %s", strerror(cc));
                dvmAbort();
            }
        }
    }

    ALOGV("threadid=%d: bye!", self->threadId);
    releaseThreadId(self);
    dvmUnlockThreadList();

    setThreadSelf(NULL);

    freeThread(self);
}


/*
 * Suspend a single thread.  Do not use to suspend yourself.
 *
 * This is used primarily for debugger/DDMS activity.  Does not return
 * until the thread has suspended or is in a "safe" state (e.g. executing
 * native code outside the VM).
 *
 * The thread list lock should be held before calling here -- it's not
 * entirely safe to hang on to a Thread* from another thread otherwise.
 * (We'd need to grab it here anyway to avoid clashing with a suspend-all.)
 */
void dvmSuspendThread(Thread* thread)
{
    assert(thread != NULL);
    assert(thread != dvmThreadSelf());
    //assert(thread->handle != dvmJdwpGetDebugThread(gDvm.jdwpState));

    lockThreadSuspendCount();
    dvmAddToSuspendCounts(thread, 1, 1);

    LOG_THREAD("threadid=%d: suspend++, now=%d",
        thread->threadId, thread->suspendCount);
    unlockThreadSuspendCount();

    waitForThreadSuspend(dvmThreadSelf(), thread);
}

/*
 * Reduce the suspend count of a thread.  If it hits zero, tell it to
 * resume.
 *
 * Used primarily for debugger/DDMS activity.  The thread in question
 * might have been suspended singly or as part of a suspend-all operation.
 *
 * The thread list lock should be held before calling here -- it's not
 * entirely safe to hang on to a Thread* from another thread otherwise.
 * (We'd need to grab it here anyway to avoid clashing with a suspend-all.)
 */
void dvmResumeThread(Thread* thread)
{
    assert(thread != NULL);
    assert(thread != dvmThreadSelf());
    //assert(thread->handle != dvmJdwpGetDebugThread(gDvm.jdwpState));

    lockThreadSuspendCount();
    if (thread->suspendCount > 0) {
        dvmAddToSuspendCounts(thread, -1, -1);
    } else {
        LOG_THREAD("threadid=%d:  suspendCount already zero",
            thread->threadId);
    }

    LOG_THREAD("threadid=%d: suspend--, now=%d",
        thread->threadId, thread->suspendCount);

    if (thread->suspendCount == 0) {
        dvmBroadcastCond(&gDvm.threadSuspendCountCond);
    }

    unlockThreadSuspendCount();
}

/*
 * Suspend yourself, as a result of debugger activity.
 */
void dvmSuspendSelf(bool jdwpActivity)
{
    Thread* self = dvmThreadSelf();

    /* debugger thread must not suspend itself due to debugger activity! */
    assert(gDvm.jdwpState != NULL);
    if (self->handle == dvmJdwpGetDebugThread(gDvm.jdwpState)) {
        assert(false);
        return;
    }

    /*
     * Collisions with other suspends aren't really interesting.  We want
     * to ensure that we're the only one fiddling with the suspend count
     * though.
     */
    lockThreadSuspendCount();
    dvmAddToSuspendCounts(self, 1, 1);

    /*
     * Suspend ourselves.
     */
    assert(self->suspendCount > 0);
    self->status = THREAD_SUSPENDED;
    LOG_THREAD("threadid=%d: self-suspending (dbg)", self->threadId);

    /*
     * Tell JDWP that we've completed suspension.  The JDWP thread can't
     * tell us to resume before we're fully asleep because we hold the
     * suspend count lock.
     *
     * If we got here via waitForDebugger(), don't do this part.
     */
    if (jdwpActivity) {
        //ALOGI("threadid=%d: clearing wait-for-event (my handle=%08x)",
        //    self->threadId, (int) self->handle);
        dvmJdwpClearWaitForEventThread(gDvm.jdwpState);
    }

    while (self->suspendCount != 0) {
        dvmWaitCond(&gDvm.threadSuspendCountCond,
                    &gDvm.threadSuspendCountLock);
        if (self->suspendCount != 0) {
            /*
             * The condition was signaled but we're still suspended.  This
             * can happen if the debugger lets go while a SIGQUIT thread
             * dump event is pending (assuming SignalCatcher was resumed for
             * just long enough to try to grab the thread-suspend lock).
             */
            ALOGD("threadid=%d: still suspended after undo (sc=%d dc=%d)",
                self->threadId, self->suspendCount, self->dbgSuspendCount);
        }
    }
    assert(self->suspendCount == 0 && self->dbgSuspendCount == 0);
    self->status = THREAD_RUNNING;
    LOG_THREAD("threadid=%d: self-reviving (dbg), status=%d",
        self->threadId, self->status);

    unlockThreadSuspendCount();
}

/*
 * Dump the state of the current thread and that of another thread that
 * we think is wedged.
 */
static void dumpWedgedThread(Thread* thread)
{
    dvmDumpThread(dvmThreadSelf(), false);
    dvmPrintNativeBackTrace();

    // dumping a running thread is risky, but could be useful
    dvmDumpThread(thread, true);

    // stop now and get a core dump
    //abort();
}

/*
 * If the thread is running at below-normal priority, temporarily elevate
 * it to "normal".
 *
 * Returns zero if no changes were made.  Otherwise, returns bit flags
 * indicating what was changed, storing the previous values in the
 * provided locations.
 */
int dvmRaiseThreadPriorityIfNeeded(Thread* thread, int* pSavedThreadPrio,
    SchedPolicy* pSavedThreadPolicy)
{
    errno = 0;
    *pSavedThreadPrio = getpriority(PRIO_PROCESS, thread->systemTid);
    if (errno != 0) {
        ALOGW("Unable to get priority for threadid=%d sysTid=%d",
            thread->threadId, thread->systemTid);
        return 0;
    }
    if (get_sched_policy(thread->systemTid, pSavedThreadPolicy) != 0) {
        ALOGW("Unable to get policy for threadid=%d sysTid=%d",
            thread->threadId, thread->systemTid);
        return 0;
    }

    int changeFlags = 0;

    /*
     * Change the priority if we're in the background group.
     */
    if (*pSavedThreadPolicy == SP_BACKGROUND) {
        if (set_sched_policy(thread->systemTid, SP_FOREGROUND) != 0) {
            ALOGW("Couldn't set fg policy on tid %d", thread->systemTid);
        } else {
            changeFlags |= kChangedPolicy;
            ALOGD("Temporarily moving tid %d to fg (was %d)",
                thread->systemTid, *pSavedThreadPolicy);
        }
    }

    /*
     * getpriority() returns the "nice" value, so larger numbers indicate
     * lower priority, with 0 being normal.
     */
    if (*pSavedThreadPrio > 0) {
        const int kHigher = 0;
        if (setpriority(PRIO_PROCESS, thread->systemTid, kHigher) != 0) {
            ALOGW("Couldn't raise priority on tid %d to %d",
                thread->systemTid, kHigher);
        } else {
            changeFlags |= kChangedPriority;
            ALOGD("Temporarily raised priority on tid %d (%d -> %d)",
                thread->systemTid, *pSavedThreadPrio, kHigher);
        }
    }

    return changeFlags;
}

/*
 * Reset the priority values for the thread in question.
 */
void dvmResetThreadPriority(Thread* thread, int changeFlags,
    int savedThreadPrio, SchedPolicy savedThreadPolicy)
{
    if ((changeFlags & kChangedPolicy) != 0) {
        if (set_sched_policy(thread->systemTid, savedThreadPolicy) != 0) {
            ALOGW("NOTE: couldn't reset tid %d to (%d)",
                thread->systemTid, savedThreadPolicy);
        } else {
            ALOGD("Restored policy of %d to %d",
                thread->systemTid, savedThreadPolicy);
        }
    }

    if ((changeFlags & kChangedPriority) != 0) {
        if (setpriority(PRIO_PROCESS, thread->systemTid, savedThreadPrio) != 0)
        {
            ALOGW("NOTE: couldn't reset priority on thread %d to %d",
                thread->systemTid, savedThreadPrio);
        } else {
            ALOGD("Restored priority on %d to %d",
                thread->systemTid, savedThreadPrio);
        }
    }
}

/*
 * Wait for another thread to see the pending suspension and stop running.
 * It can either suspend itself or go into a non-running state such as
 * VMWAIT or NATIVE in which it cannot interact with the GC.
 *
 * If we're running at a higher priority, sched_yield() may not do anything,
 * so we need to sleep for "long enough" to guarantee that the other
 * thread has a chance to finish what it's doing.  Sleeping for too short
 * a period (e.g. less than the resolution of the sleep clock) might cause
 * the scheduler to return immediately, so we want to start with a
 * "reasonable" value and expand.
 *
 * This does not return until the other thread has stopped running.
 * Eventually we time out and the VM aborts.
 *
 * This does not try to detect the situation where two threads are
 * waiting for each other to suspend.  In normal use this is part of a
 * suspend-all, which implies that the suspend-all lock is held, or as
 * part of a debugger action in which the JDWP thread is always the one
 * doing the suspending.  (We may need to re-evaluate this now that
 * getThreadStackTrace is implemented as suspend-snapshot-resume.)
 *
 * TODO: track basic stats about time required to suspend VM.
 */
#define FIRST_SLEEP (250*1000)    /* 0.25s */
#define MORE_SLEEP  (750*1000)    /* 0.75s */
static void waitForThreadSuspend(Thread* self, Thread* thread)
{
    const int kMaxRetries = 10;
    int spinSleepTime = FIRST_SLEEP;
    bool complained = false;
    int priChangeFlags = 0;
    int savedThreadPrio = -500;
    SchedPolicy savedThreadPolicy = SP_FOREGROUND;

    int sleepIter = 0;
    int retryCount = 0;
    u8 startWhen = 0;       // init req'd to placate gcc
    u8 firstStartWhen = 0;

    while (thread->status == THREAD_RUNNING) {
        if (sleepIter == 0) {           // get current time on first iteration
            startWhen = dvmGetRelativeTimeUsec();
            if (firstStartWhen == 0)    // first iteration of first attempt
                firstStartWhen = startWhen;

            /*
             * After waiting for a bit, check to see if the target thread is
             * running at a reduced priority.  If so, bump it up temporarily
             * to give it more CPU time.
             */
            if (retryCount == 2) {
                assert(thread->systemTid != 0);
                priChangeFlags = dvmRaiseThreadPriorityIfNeeded(thread,
                    &savedThreadPrio, &savedThreadPolicy);
            }
        }

#if defined (WITH_JIT)
        /*
         * If we're still waiting after the first timeout, unchain all
         * translations iff:
         *   1) There are new chains formed since the last unchain
         *   2) The top VM frame of the running thread is running JIT'ed code
         */
        if (gDvmJit.pJitEntryTable && retryCount > 0 &&
            gDvmJit.hasNewChain && thread->inJitCodeCache) {
            ALOGD("JIT unchain all for threadid=%d", thread->threadId);
            dvmJitUnchainAll();
        }
#endif

        /*
         * Sleep briefly.  The iterative sleep call returns false if we've
         * exceeded the total time limit for this round of sleeping.
         */
        if (!dvmIterativeSleep(sleepIter++, spinSleepTime, startWhen)) {
            if (spinSleepTime != FIRST_SLEEP) {
                ALOGW("threadid=%d: spin on suspend #%d threadid=%d (pcf=%d)",
                    self->threadId, retryCount,
                    thread->threadId, priChangeFlags);
                if (retryCount > 1) {
                    /* stack trace logging is slow; skip on first iter */
                    dumpWedgedThread(thread);
                }
                complained = true;
            }

            // keep going; could be slow due to valgrind
            sleepIter = 0;
            spinSleepTime = MORE_SLEEP;

            if (retryCount++ == kMaxRetries) {
                ALOGE("Fatal spin-on-suspend, dumping threads");
                dvmDumpAllThreads(false);

                /* log this after -- long traces will scroll off log */
                ALOGE("threadid=%d: stuck on threadid=%d, giving up",
                    self->threadId, thread->threadId);

                /* try to get a debuggerd dump from the spinning thread */
                dvmNukeThread(thread);
                /* abort the VM */
                dvmAbort();
            }
        }
    }

    if (complained) {
        ALOGW("threadid=%d: spin on suspend resolved in %lld msec",
            self->threadId,
            (dvmGetRelativeTimeUsec() - firstStartWhen) / 1000);
        //dvmDumpThread(thread, false);   /* suspended, so dump is safe */
    }
    if (priChangeFlags != 0) {
        dvmResetThreadPriority(thread, priChangeFlags, savedThreadPrio,
            savedThreadPolicy);
    }
}

/*
 * Suspend all threads except the current one.  This is used by the GC,
 * the debugger, and by any thread that hits a "suspend all threads"
 * debugger event (e.g. breakpoint or exception).
 *
 * If thread N hits a "suspend all threads" breakpoint, we don't want it
 * to suspend the JDWP thread.  For the GC, we do, because the debugger can
 * create objects and even execute arbitrary code.  The "why" argument
 * allows the caller to say why the suspension is taking place.
 *
 * This can be called when a global suspend has already happened, due to
 * various debugger gymnastics, so keeping an "everybody is suspended" flag
 * doesn't work.
 *
 * DO NOT grab any locks before calling here.  We grab & release the thread
 * lock and suspend lock here (and we're not using recursive threads), and
 * we might have to self-suspend if somebody else beats us here.
 *
 * We know the current thread is in the thread list, because we attach the
 * thread before doing anything that could cause VM suspension (like object
 * allocation).
 */
void dvmSuspendAllThreads(SuspendCause why)
{
    Thread* self = dvmThreadSelf();
    Thread* thread;

    assert(why != 0);

    /*
     * Start by grabbing the thread suspend lock.  If we can't get it, most
     * likely somebody else is in the process of performing a suspend or
     * resume, so lockThreadSuspend() will cause us to self-suspend.
     *
     * We keep the lock until all other threads are suspended.
     */
    lockThreadSuspend("susp-all", why);

    LOG_THREAD("threadid=%d: SuspendAll starting", self->threadId);

    /*
     * This is possible if the current thread was in VMWAIT mode when a
     * suspend-all happened, and then decided to do its own suspend-all.
     * This can happen when a couple of threads have simultaneous events
     * of interest to the debugger.
     */
    //assert(self->suspendCount == 0);

    /*
     * Increment everybody's suspend count (except our own).
     */
    dvmLockThreadList(self);

    lockThreadSuspendCount();
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread == self)
            continue;

        /* debugger events don't suspend JDWP thread */
        if ((why == SUSPEND_FOR_DEBUG || why == SUSPEND_FOR_DEBUG_EVENT) &&
            thread->handle == dvmJdwpGetDebugThread(gDvm.jdwpState))
            continue;

        dvmAddToSuspendCounts(thread, 1,
                              (why == SUSPEND_FOR_DEBUG ||
                              why == SUSPEND_FOR_DEBUG_EVENT)
                              ? 1 : 0);
    }
    unlockThreadSuspendCount();

    /*
     * Wait for everybody in THREAD_RUNNING state to stop.  Other states
     * indicate the code is either running natively or sleeping quietly.
     * Any attempt to transition back to THREAD_RUNNING will cause a check
     * for suspension, so it should be impossible for anything to execute
     * interpreted code or modify objects (assuming native code plays nicely).
     *
     * It's also okay if the thread transitions to a non-RUNNING state.
     *
     * Note we released the threadSuspendCountLock before getting here,
     * so if another thread is fiddling with its suspend count (perhaps
     * self-suspending for the debugger) it won't block while we're waiting
     * in here.
     */
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread == self)
            continue;

        /* debugger events don't suspend JDWP thread */
        if ((why == SUSPEND_FOR_DEBUG || why == SUSPEND_FOR_DEBUG_EVENT) &&
            thread->handle == dvmJdwpGetDebugThread(gDvm.jdwpState))
            continue;

        /* wait for the other thread to see the pending suspend */
        waitForThreadSuspend(self, thread);

        LOG_THREAD("threadid=%d:   threadid=%d status=%d sc=%d dc=%d",
            self->threadId, thread->threadId, thread->status,
            thread->suspendCount, thread->dbgSuspendCount);
    }

    dvmUnlockThreadList();
    unlockThreadSuspend();

    LOG_THREAD("threadid=%d: SuspendAll complete", self->threadId);
}

/*
 * Resume all threads that are currently suspended.
 *
 * The "why" must match with the previous suspend.
 */
void dvmResumeAllThreads(SuspendCause why)
{
    Thread* self = dvmThreadSelf();
    Thread* thread;

    lockThreadSuspend("res-all", why);  /* one suspend/resume at a time */
    LOG_THREAD("threadid=%d: ResumeAll starting", self->threadId);

    /*
     * Decrement the suspend counts for all threads.  No need for atomic
     * writes, since nobody should be moving until we decrement the count.
     * We do need to hold the thread list because of JNI attaches.
     */
    dvmLockThreadList(self);
    lockThreadSuspendCount();
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread == self)
            continue;

        /* debugger events don't suspend JDWP thread */
        if ((why == SUSPEND_FOR_DEBUG || why == SUSPEND_FOR_DEBUG_EVENT) &&
            thread->handle == dvmJdwpGetDebugThread(gDvm.jdwpState))
        {
            continue;
        }

        if (thread->suspendCount > 0) {
            dvmAddToSuspendCounts(thread, -1,
                                  (why == SUSPEND_FOR_DEBUG ||
                                  why == SUSPEND_FOR_DEBUG_EVENT)
                                  ? -1 : 0);
        } else {
            LOG_THREAD("threadid=%d:  suspendCount already zero",
                thread->threadId);
        }
    }
    unlockThreadSuspendCount();
    dvmUnlockThreadList();

    /*
     * In some ways it makes sense to continue to hold the thread-suspend
     * lock while we issue the wakeup broadcast.  It allows us to complete
     * one operation before moving on to the next, which simplifies the
     * thread activity debug traces.
     *
     * This approach caused us some difficulty under Linux, because the
     * condition variable broadcast not only made the threads runnable,
     * but actually caused them to execute, and it was a while before
     * the thread performing the wakeup had an opportunity to release the
     * thread-suspend lock.
     *
     * This is a problem because, when a thread tries to acquire that
     * lock, it times out after 3 seconds.  If at some point the thread
     * is told to suspend, the clock resets; but since the VM is still
     * theoretically mid-resume, there's no suspend pending.  If, for
     * example, the GC was waking threads up while the SIGQUIT handler
     * was trying to acquire the lock, we would occasionally time out on
     * a busy system and SignalCatcher would abort.
     *
     * We now perform the unlock before the wakeup broadcast.  The next
     * suspend can't actually start until the broadcast completes and
     * returns, because we're holding the thread-suspend-count lock, but the
     * suspending thread is now able to make progress and we avoid the abort.
     *
     * (Technically there is a narrow window between when we release
     * the thread-suspend lock and grab the thread-suspend-count lock.
     * This could cause us to send a broadcast to threads with nonzero
     * suspend counts, but this is expected and they'll all just fall
     * right back to sleep.  It's probably safe to grab the suspend-count
     * lock before releasing thread-suspend, since we're still following
     * the correct order of acquisition, but it feels weird.)
     */

    LOG_THREAD("threadid=%d: ResumeAll waking others", self->threadId);
    unlockThreadSuspend();

    /*
     * Broadcast a notification to all suspended threads, some or all of
     * which may choose to wake up.  No need to wait for them.
     */
    lockThreadSuspendCount();
    int cc = pthread_cond_broadcast(&gDvm.threadSuspendCountCond);
    if (cc != 0) {
        ALOGE("pthread_cond_broadcast(&gDvm.threadSuspendCountCond) failed: %s", strerror(cc));
        dvmAbort();
    }
    unlockThreadSuspendCount();

    LOG_THREAD("threadid=%d: ResumeAll complete", self->threadId);
}

/*
 * Undo any debugger suspensions.  This is called when the debugger
 * disconnects.
 */
void dvmUndoDebuggerSuspensions()
{
    Thread* self = dvmThreadSelf();
    Thread* thread;

    lockThreadSuspend("undo", SUSPEND_FOR_DEBUG);
    LOG_THREAD("threadid=%d: UndoDebuggerSusp starting", self->threadId);

    /*
     * Decrement the suspend counts for all threads.  No need for atomic
     * writes, since nobody should be moving until we decrement the count.
     * We do need to hold the thread list because of JNI attaches.
     */
    dvmLockThreadList(self);
    lockThreadSuspendCount();
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread == self)
            continue;

        /* debugger events don't suspend JDWP thread */
        if (thread->handle == dvmJdwpGetDebugThread(gDvm.jdwpState)) {
            assert(thread->dbgSuspendCount == 0);
            continue;
        }

        assert(thread->suspendCount >= thread->dbgSuspendCount);
        dvmAddToSuspendCounts(thread, -thread->dbgSuspendCount,
                              -thread->dbgSuspendCount);
    }
    unlockThreadSuspendCount();
    dvmUnlockThreadList();

    /*
     * Broadcast a notification to all suspended threads, some or all of
     * which may choose to wake up.  No need to wait for them.
     */
    lockThreadSuspendCount();
    int cc = pthread_cond_broadcast(&gDvm.threadSuspendCountCond);
    if (cc != 0) {
        ALOGE("pthread_cond_broadcast(&gDvm.threadSuspendCountCond) failed: %s", strerror(cc));
        dvmAbort();
    }
    unlockThreadSuspendCount();

    unlockThreadSuspend();

    LOG_THREAD("threadid=%d: UndoDebuggerSusp complete", self->threadId);
}

/*
 * Determine if a thread is suspended.
 *
 * As with all operations on foreign threads, the caller should hold
 * the thread list lock before calling.
 *
 * If the thread is suspending or waking, these fields could be changing
 * out from under us (or the thread could change state right after we
 * examine it), making this generally unreliable.  This is chiefly
 * intended for use by the debugger.
 */
bool dvmIsSuspended(const Thread* thread)
{
    /*
     * The thread could be:
     *  (1) Running happily.  status is RUNNING, suspendCount is zero.
     *      Return "false".
     *  (2) Pending suspend.  status is RUNNING, suspendCount is nonzero.
     *      Return "false".
     *  (3) Suspended.  suspendCount is nonzero, and status is !RUNNING.
     *      Return "true".
     *  (4) Waking up.  suspendCount is zero, status is SUSPENDED
     *      Return "false" (since it could change out from under us, unless
     *      we hold suspendCountLock).
     */

    return (thread->suspendCount != 0 &&
            thread->status != THREAD_RUNNING);
}

/*
 * Wait until another thread self-suspends.  This is specifically for
 * synchronization between the JDWP thread and a thread that has decided
 * to suspend itself after sending an event to the debugger.
 *
 * Threads that encounter "suspend all" events work as well -- the thread
 * in question suspends everybody else and then itself.
 *
 * We can't hold a thread lock here or in the caller, because we could
 * get here just before the to-be-waited-for-thread issues a "suspend all".
 * There's an opportunity for badness if the thread we're waiting for exits
 * and gets cleaned up, but since the thread in question is processing a
 * debugger event, that's not really a possibility.  (To avoid deadlock,
 * it's important that we not be in THREAD_RUNNING while we wait.)
 */
void dvmWaitForSuspend(Thread* thread)
{
    Thread* self = dvmThreadSelf();

    LOG_THREAD("threadid=%d: waiting for threadid=%d to sleep",
        self->threadId, thread->threadId);

    assert(thread->handle != dvmJdwpGetDebugThread(gDvm.jdwpState));
    assert(thread != self);
    assert(self->status != THREAD_RUNNING);

    waitForThreadSuspend(self, thread);

    LOG_THREAD("threadid=%d: threadid=%d is now asleep",
        self->threadId, thread->threadId);
}

/*
 * Check to see if we need to suspend ourselves.  If so, go to sleep on
 * a condition variable.
 *
 * Returns "true" if we suspended ourselves.
 */
static bool fullSuspendCheck(Thread* self)
{
    assert(self != NULL);
    assert(self->suspendCount >= 0);

    /*
     * Grab gDvm.threadSuspendCountLock.  This gives us exclusive write
     * access to self->suspendCount.
     */
    lockThreadSuspendCount();   /* grab gDvm.threadSuspendCountLock */

    bool needSuspend = (self->suspendCount != 0);
    if (needSuspend) {
        LOG_THREAD("threadid=%d: self-suspending", self->threadId);
        ThreadStatus oldStatus = self->status;      /* should be RUNNING */
        self->status = THREAD_SUSPENDED;

        ATRACE_BEGIN("DVM Suspend");
        while (self->suspendCount != 0) {
            /*
             * Wait for wakeup signal, releasing lock.  The act of releasing
             * and re-acquiring the lock provides the memory barriers we
             * need for correct behavior on SMP.
             */
            dvmWaitCond(&gDvm.threadSuspendCountCond,
                    &gDvm.threadSuspendCountLock);
        }
        ATRACE_END();
        assert(self->suspendCount == 0 && self->dbgSuspendCount == 0);
        self->status = oldStatus;
        LOG_THREAD("threadid=%d: self-reviving, status=%d",
            self->threadId, self->status);
    }

    unlockThreadSuspendCount();

    return needSuspend;
}

/*
 * Check to see if a suspend is pending.  If so, suspend the current
 * thread, and return "true" after we have been resumed.
 */
bool dvmCheckSuspendPending(Thread* self)
{
    assert(self != NULL);
    if (self->suspendCount == 0) {
        return false;
    } else {
        return fullSuspendCheck(self);
    }
}

/*
 * Update our status.
 *
 * The "self" argument, which may be NULL, is accepted as an optimization.
 *
 * Returns the old status.
 */
ThreadStatus dvmChangeStatus(Thread* self, ThreadStatus newStatus)
{
    ThreadStatus oldStatus;

    if (self == NULL)
        self = dvmThreadSelf();

    LOGVV("threadid=%d: (status %d -> %d)",
        self->threadId, self->status, newStatus);

    oldStatus = self->status;
    if (oldStatus == newStatus)
        return oldStatus;

    if (newStatus == THREAD_RUNNING) {
        /*
         * Change our status to THREAD_RUNNING.  The transition requires
         * that we check for pending suspension, because the VM considers
         * us to be "asleep" in all other states, and another thread could
         * be performing a GC now.
         *
         * The order of operations is very significant here.  One way to
         * do this wrong is:
         *
         *   GCing thread                   Our thread (in NATIVE)
         *   ------------                   ----------------------
         *                                  check suspend count (== 0)
         *   dvmSuspendAllThreads()
         *   grab suspend-count lock
         *   increment all suspend counts
         *   release suspend-count lock
         *   check thread state (== NATIVE)
         *   all are suspended, begin GC
         *                                  set state to RUNNING
         *                                  (continue executing)
         *
         * We can correct this by grabbing the suspend-count lock and
         * performing both of our operations (check suspend count, set
         * state) while holding it, now we need to grab a mutex on every
         * transition to RUNNING.
         *
         * What we do instead is change the order of operations so that
         * the transition to RUNNING happens first.  If we then detect
         * that the suspend count is nonzero, we switch to SUSPENDED.
         *
         * Appropriate compiler and memory barriers are required to ensure
         * that the operations are observed in the expected order.
         *
         * This does create a small window of opportunity where a GC in
         * progress could observe what appears to be a running thread (if
         * it happens to look between when we set to RUNNING and when we
         * switch to SUSPENDED).  At worst this only affects assertions
         * and thread logging.  (We could work around it with some sort
         * of intermediate "pre-running" state that is generally treated
         * as equivalent to running, but that doesn't seem worthwhile.)
         *
         * We can also solve this by combining the "status" and "suspend
         * count" fields into a single 32-bit value.  This trades the
         * store/load barrier on transition to RUNNING for an atomic RMW
         * op on all transitions and all suspend count updates (also, all
         * accesses to status or the thread count require bit-fiddling).
         * It also eliminates the brief transition through RUNNING when
         * the thread is supposed to be suspended.  This is possibly faster
         * on SMP and slightly more correct, but less convenient.
         */
        volatile void* raw = reinterpret_cast<volatile void*>(&self->status);
        volatile int32_t* addr = reinterpret_cast<volatile int32_t*>(raw);
        android_atomic_acquire_store(newStatus, addr);
        if (self->suspendCount != 0) {
            fullSuspendCheck(self);
        }
    } else {
        /*
         * Not changing to THREAD_RUNNING.  No additional work required.
         *
         * We use a releasing store to ensure that, if we were RUNNING,
         * any updates we previously made to objects on the managed heap
         * will be observed before the state change.
         */
        assert(newStatus != THREAD_SUSPENDED);
        volatile void* raw = reinterpret_cast<volatile void*>(&self->status);
        volatile int32_t* addr = reinterpret_cast<volatile int32_t*>(raw);
        android_atomic_release_store(newStatus, addr);
    }

    return oldStatus;
}

/*
 * Get a statically defined thread group from a field in the ThreadGroup
 * Class object.  Expected arguments are "mMain" and "mSystem".
 */
static Object* getStaticThreadGroup(const char* fieldName)
{
    StaticField* groupField;
    Object* groupObj;

    groupField = dvmFindStaticField(gDvm.classJavaLangThreadGroup,
        fieldName, "Ljava/lang/ThreadGroup;");
    if (groupField == NULL) {
        ALOGE("java.lang.ThreadGroup does not have an '%s' field", fieldName);
        dvmThrowInternalError("bad definition for ThreadGroup");
        return NULL;
    }
    groupObj = dvmGetStaticFieldObject(groupField);
    if (groupObj == NULL) {
        ALOGE("java.lang.ThreadGroup.%s not initialized", fieldName);
        dvmThrowInternalError(NULL);
        return NULL;
    }

    return groupObj;
}
Object* dvmGetSystemThreadGroup()
{
    return getStaticThreadGroup("mSystem");
}
Object* dvmGetMainThreadGroup()
{
    return getStaticThreadGroup("mMain");
}

/*
 * Given a VMThread object, return the associated Thread*.
 *
 * NOTE: if the thread detaches, the struct Thread will disappear, and
 * we will be touching invalid data.  For safety, lock the thread list
 * before calling this.
 */
Thread* dvmGetThreadFromThreadObject(Object* vmThreadObj)
{
    int vmData;

    vmData = dvmGetFieldInt(vmThreadObj, gDvm.offJavaLangVMThread_vmData);

    if (false) {
        Thread* thread = gDvm.threadList;
        while (thread != NULL) {
            if ((Thread*)vmData == thread)
                break;

            thread = thread->next;
        }

        if (thread == NULL) {
            ALOGW("WARNING: vmThreadObj=%p has thread=%p, not in thread list",
                vmThreadObj, (Thread*)vmData);
            vmData = 0;
        }
    }

    return (Thread*) vmData;
}

/*
 * Given a pthread handle, return the associated Thread*.
 * Caller must hold the thread list lock.
 *
 * Returns NULL if the thread was not found.
 */
Thread* dvmGetThreadByHandle(pthread_t handle)
{
    Thread* thread;
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread->handle == handle)
            break;
    }
    return thread;
}

/*
 * Given a threadId, return the associated Thread*.
 * Caller must hold the thread list lock.
 *
 * Returns NULL if the thread was not found.
 */
Thread* dvmGetThreadByThreadId(u4 threadId)
{
    Thread* thread;
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread->threadId == threadId)
            break;
    }
    return thread;
}

void dvmChangeThreadPriority(Thread* thread, int newPriority)
{
    os_changeThreadPriority(thread, newPriority);
}

/*
 * Return true if the thread is on gDvm.threadList.
 * Caller should not hold gDvm.threadListLock.
 */
bool dvmIsOnThreadList(const Thread* thread)
{
    bool ret = false;

    dvmLockThreadList(NULL);
    if (thread == gDvm.threadList) {
        ret = true;
    } else {
        ret = thread->prev != NULL || thread->next != NULL;
    }
    dvmUnlockThreadList();

    return ret;
}

/*
 * Dump a thread to the log file -- just calls dvmDumpThreadEx() with an
 * output target.
 */
void dvmDumpThread(Thread* thread, bool isRunning)
{
    DebugOutputTarget target;

    dvmCreateLogOutputTarget(&target, ANDROID_LOG_INFO, LOG_TAG);
    dvmDumpThreadEx(&target, thread, isRunning);
}

/*
 * Try to get the scheduler group.
 *
 * The data from /proc/<pid>/cgroup looks (something) like:
 *  2:cpu:/bg_non_interactive
 *  1:cpuacct:/
 *
 * We return the part on the "cpu" line after the '/', which will be an
 * empty string for the default cgroup.  If the string is longer than
 * "bufLen", the string will be truncated.
 *
 * On error, -1 is returned, and an error description will be stored in
 * the buffer.
 */
static int getSchedulerGroup(int tid, char* buf, size_t bufLen)
{
#ifdef HAVE_ANDROID_OS
    char pathBuf[32];
    char lineBuf[256];
    FILE *fp;

    snprintf(pathBuf, sizeof(pathBuf), "/proc/%d/cgroup", tid);
    if ((fp = fopen(pathBuf, "r")) == NULL) {
        snprintf(buf, bufLen, "[fopen-error:%d]", errno);
        return -1;
    }

    while (fgets(lineBuf, sizeof(lineBuf) -1, fp) != NULL) {
        char* subsys;
        char* grp;
        size_t len;

        /* Junk the first field */
        subsys = strchr(lineBuf, ':');
        if (subsys == NULL) {
            goto out_bad_data;
        }

        if (strncmp(subsys, ":cpu:", 5) != 0) {
            /* Not the subsys we're looking for */
            continue;
        }

        grp = strchr(subsys, '/');
        if (grp == NULL) {
            goto out_bad_data;
        }
        grp++; /* Drop the leading '/' */

        len = strlen(grp);
        grp[len-1] = '\0'; /* Drop the trailing '\n' */

        if (bufLen <= len) {
            len = bufLen - 1;
        }
        strncpy(buf, grp, len);
        buf[len] = '\0';
        fclose(fp);
        return 0;
    }

    snprintf(buf, bufLen, "[no-cpu-subsys]");
    fclose(fp);
    return -1;

out_bad_data:
    ALOGE("Bad cgroup data {%s}", lineBuf);
    snprintf(buf, bufLen, "[data-parse-failed]");
    fclose(fp);
    return -1;

#else
    snprintf(buf, bufLen, "[n/a]");
    return -1;
#endif
}

/*
 * Convert ThreadStatus to a string.
 */
const char* dvmGetThreadStatusStr(ThreadStatus status)
{
    switch (status) {
    case THREAD_ZOMBIE:         return "ZOMBIE";
    case THREAD_RUNNING:        return "RUNNABLE";
    case THREAD_TIMED_WAIT:     return "TIMED_WAIT";
    case THREAD_MONITOR:        return "MONITOR";
    case THREAD_WAIT:           return "WAIT";
    case THREAD_INITIALIZING:   return "INITIALIZING";
    case THREAD_STARTING:       return "STARTING";
    case THREAD_NATIVE:         return "NATIVE";
    case THREAD_VMWAIT:         return "VMWAIT";
    case THREAD_SUSPENDED:      return "SUSPENDED";
    default:                    return "UNKNOWN";
    }
}

static void dumpSchedStat(const DebugOutputTarget* target, pid_t tid) {
#ifdef HAVE_ANDROID_OS
    /* get some bits from /proc/self/stat */
    ProcStatData procStatData;
    if (!dvmGetThreadStats(&procStatData, tid)) {
        /* failed, use zeroed values */
        memset(&procStatData, 0, sizeof(procStatData));
    }

    /* grab the scheduler stats for this thread */
    char schedstatBuf[64];
    snprintf(schedstatBuf, sizeof(schedstatBuf), "/proc/self/task/%d/schedstat", tid);
    int schedstatFd = open(schedstatBuf, O_RDONLY);
    strcpy(schedstatBuf, "0 0 0");          /* show this if open/read fails */
    if (schedstatFd >= 0) {
        ssize_t bytes;
        bytes = read(schedstatFd, schedstatBuf, sizeof(schedstatBuf) - 1);
        close(schedstatFd);
        if (bytes >= 1) {
            schedstatBuf[bytes - 1] = '\0';   /* remove trailing newline */
        }
    }

    /* show what we got */
    dvmPrintDebugMessage(target,
        "  | state=%c schedstat=( %s ) utm=%lu stm=%lu core=%d\n",
        procStatData.state, schedstatBuf, procStatData.utime,
        procStatData.stime, procStatData.processor);
#endif
}

struct SchedulerStats {
    int policy;
    int priority;
    char group[32];
};

/*
 * Get scheduler statistics.
 */
static void getSchedulerStats(SchedulerStats* stats, pid_t tid) {
    struct sched_param sp;
    if (pthread_getschedparam(pthread_self(), &stats->policy, &sp) != 0) {
        ALOGW("Warning: pthread_getschedparam failed");
        stats->policy = -1;
        stats->priority = -1;
    } else {
        stats->priority = sp.sched_priority;
    }
    if (getSchedulerGroup(tid, stats->group, sizeof(stats->group)) == 0 &&
            stats->group[0] == '\0') {
        strcpy(stats->group, "default");
    }
}

static bool shouldShowNativeStack(Thread* thread) {
    // In native code somewhere in the VM? That's interesting.
    if (thread->status == THREAD_VMWAIT) {
        return true;
    }

    // In an Object.wait variant? That's not interesting.
    if (thread->status == THREAD_TIMED_WAIT || thread->status == THREAD_WAIT) {
        return false;
    }

    // The Signal Catcher thread? That's not interesting.
    if (thread->status == THREAD_RUNNING) {
        return false;
    }

    // In some other native method? That's interesting.
    // We don't just check THREAD_NATIVE because native methods will be in
    // state THREAD_SUSPENDED if they're calling back into the VM, or THREAD_MONITOR
    // if they're blocked on a monitor, or one of the thread-startup states if
    // it's early enough in their life cycle (http://b/7432159).
    u4* fp = thread->interpSave.curFrame;
    if (fp == NULL) {
        // The thread has no managed frames, so native frames are all there is.
        return true;
    }
    const Method* currentMethod = SAVEAREA_FROM_FP(fp)->method;
    return currentMethod != NULL && dvmIsNativeMethod(currentMethod);
}

/*
 * Print information about the specified thread.
 *
 * Works best when the thread in question is "self" or has been suspended.
 * When dumping a separate thread that's still running, set "isRunning" to
 * use a more cautious thread dump function.
 */
void dvmDumpThreadEx(const DebugOutputTarget* target, Thread* thread,
    bool isRunning)
{
    Object* threadObj;
    Object* groupObj;
    StringObject* nameStr;
    char* threadName = NULL;
    char* groupName = NULL;
    bool isDaemon;
    int priority;               // java.lang.Thread priority

    /*
     * Get the java.lang.Thread object.  This function gets called from
     * some weird debug contexts, so it's possible that there's a GC in
     * progress on some other thread.  To decrease the chances of the
     * thread object being moved out from under us, we add the reference
     * to the tracked allocation list, which pins it in place.
     *
     * If threadObj is NULL, the thread is still in the process of being
     * attached to the VM, and there's really nothing interesting to
     * say about it yet.
     */
    threadObj = thread->threadObj;
    if (threadObj == NULL) {
        ALOGI("Can't dump thread %d: threadObj not set", thread->threadId);
        return;
    }
    dvmAddTrackedAlloc(threadObj, NULL);

    nameStr = (StringObject*) dvmGetFieldObject(threadObj,
                gDvm.offJavaLangThread_name);
    threadName = dvmCreateCstrFromString(nameStr);

    priority = dvmGetFieldInt(threadObj, gDvm.offJavaLangThread_priority);
    isDaemon = dvmGetFieldBoolean(threadObj, gDvm.offJavaLangThread_daemon);

    /* a null value for group is not expected, but deal with it anyway */
    groupObj = (Object*) dvmGetFieldObject(threadObj,
                gDvm.offJavaLangThread_group);
    if (groupObj != NULL) {
        nameStr = (StringObject*)
            dvmGetFieldObject(groupObj, gDvm.offJavaLangThreadGroup_name);
        groupName = dvmCreateCstrFromString(nameStr);
    }
    if (groupName == NULL)
        groupName = strdup("(null; initializing?)");

    SchedulerStats schedStats;
    getSchedulerStats(&schedStats, thread->systemTid);

    dvmPrintDebugMessage(target,
        "\"%s\"%s prio=%d tid=%d %s%s\n",
        threadName, isDaemon ? " daemon" : "",
        priority, thread->threadId, dvmGetThreadStatusStr(thread->status),
#if defined(WITH_JIT)
        thread->inJitCodeCache ? " JIT" : ""
#else
        ""
#endif
        );
    dvmPrintDebugMessage(target,
        "  | group=\"%s\" sCount=%d dsCount=%d obj=%p self=%p\n",
        groupName, thread->suspendCount, thread->dbgSuspendCount,
        thread->threadObj, thread);
    dvmPrintDebugMessage(target,
        "  | sysTid=%d nice=%d sched=%d/%d cgrp=%s handle=%d\n",
        thread->systemTid, getpriority(PRIO_PROCESS, thread->systemTid),
        schedStats.policy, schedStats.priority, schedStats.group, (int)thread->handle);

    dumpSchedStat(target, thread->systemTid);

    if (shouldShowNativeStack(thread)) {
        dvmDumpNativeStack(target, thread->systemTid);
    }

    if (isRunning)
        dvmDumpRunningThreadStack(target, thread);
    else
        dvmDumpThreadStack(target, thread);

    dvmPrintDebugMessage(target, "\n");

    dvmReleaseTrackedAlloc(threadObj, NULL);
    free(threadName);
    free(groupName);
}

std::string dvmGetThreadName(Thread* thread) {
    if (thread->threadObj == NULL) {
        ALOGW("threadObj is NULL, name not available");
        return "-unknown-";
    }

    StringObject* nameObj = (StringObject*)
        dvmGetFieldObject(thread->threadObj, gDvm.offJavaLangThread_name);
    char* name = dvmCreateCstrFromString(nameObj);
    std::string result(name);
    free(name);
    return result;
}

#ifdef HAVE_ANDROID_OS
/*
 * Dumps information about a non-Dalvik thread.
 */
static void dumpNativeThread(const DebugOutputTarget* target, pid_t tid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", tid);

    int fd = open(path, O_RDONLY);
    char name[64];
    ssize_t n = 0;
    if (fd >= 0) {
        n = read(fd, name, sizeof(name) - 1);
        close(fd);
    }
    if (n > 0 && name[n - 1] == '\n') {
        n -= 1;
    }
    if (n <= 0) {
        strcpy(name, "<no name>");
    } else {
        name[n] = '\0';
    }

    SchedulerStats schedStats;
    getSchedulerStats(&schedStats, tid);

    dvmPrintDebugMessage(target,
        "\"%s\" sysTid=%d nice=%d sched=%d/%d cgrp=%s\n",
        name, tid, getpriority(PRIO_PROCESS, tid),
        schedStats.policy, schedStats.priority, schedStats.group);
    dumpSchedStat(target, tid);
    // Temporarily disabled collecting native stacks from non-Dalvik
    // threads because sometimes they misbehave.
    //dvmDumpNativeStack(target, tid);

    dvmPrintDebugMessage(target, "\n");
}

/*
 * Returns true if the specified tid is a Dalvik thread.
 * Assumes the thread list lock is held.
 */
static bool isDalvikThread(pid_t tid) {
    for (Thread* thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread->systemTid == tid) {
            return true;
        }
    }
    return false;
}
#endif

/*
 * Dump all threads to the log file -- just calls dvmDumpAllThreadsEx() with
 * an output target.
 */
void dvmDumpAllThreads(bool grabLock)
{
    DebugOutputTarget target;

    dvmCreateLogOutputTarget(&target, ANDROID_LOG_INFO, LOG_TAG);
    dvmDumpAllThreadsEx(&target, grabLock);
}

/*
 * Print information about all known threads.  Assumes they have been
 * suspended (or are in a non-interpreting state, e.g. WAIT or NATIVE).
 *
 * If "grabLock" is true, we grab the thread lock list.  This is important
 * to do unless the caller already holds the lock.
 */
void dvmDumpAllThreadsEx(const DebugOutputTarget* target, bool grabLock)
{
    Thread* thread;

    dvmPrintDebugMessage(target, "DALVIK THREADS:\n");

#ifdef HAVE_ANDROID_OS
    dvmPrintDebugMessage(target,
        "(mutexes: tll=%x tsl=%x tscl=%x ghl=%x)\n\n",
        gDvm.threadListLock.value,
        gDvm._threadSuspendLock.value,
        gDvm.threadSuspendCountLock.value,
        gDvm.gcHeapLock.value);
#endif

    if (grabLock)
        dvmLockThreadList(dvmThreadSelf());

    thread = gDvm.threadList;
    while (thread != NULL) {
        dvmDumpThreadEx(target, thread, false);

        /* verify link */
        assert(thread->next == NULL || thread->next->prev == thread);

        thread = thread->next;
    }

#ifdef HAVE_ANDROID_OS
    DIR* d = opendir("/proc/self/task");
    if (d != NULL) {
        dirent* entry = NULL;
        bool first = true;
        while ((entry = readdir(d)) != NULL) {
            char* end;
            pid_t tid = strtol(entry->d_name, &end, 10);
            if (!*end && !isDalvikThread(tid)) {
                if (first) {
                    dvmPrintDebugMessage(target, "NATIVE THREADS:\n");
                    first = false;
                }
                dumpNativeThread(target, tid);
            }
        }
        closedir(d);
    }
#endif

    if (grabLock)
        dvmUnlockThreadList();
}

/*
 * Nuke the target thread from orbit.
 *
 * The idea is to send a "crash" signal to the target thread so that
 * debuggerd will take notice and dump an appropriate stack trace.
 * Because of the way debuggerd works, we have to throw the same signal
 * at it twice.
 *
 * This does not necessarily cause the entire process to stop, but once a
 * thread has been nuked the rest of the system is likely to be unstable.
 * This returns so that some limited set of additional operations may be
 * performed, but it's advisable (and expected) to call dvmAbort soon.
 * (This is NOT a way to simply cancel a thread.)
 */
void dvmNukeThread(Thread* thread)
{
    int killResult;

    /* suppress the heapworker watchdog to assist anyone using a debugger */
    gDvm.nativeDebuggerActive = true;

    /*
     * Send the signals, separated by a brief interval to allow debuggerd
     * to work its magic.  An uncommon signal like SIGFPE or SIGSTKFLT
     * can be used instead of SIGSEGV to avoid making it look like the
     * code actually crashed at the current point of execution.
     *
     * (Observed behavior: with SIGFPE, debuggerd will dump the target
     * thread and then the thread that calls dvmAbort.  With SIGSEGV,
     * you don't get the second stack trace; possibly something in the
     * kernel decides that a signal has already been sent and it's time
     * to just kill the process.  The position in the current thread is
     * generally known, so the second dump is not useful.)
     *
     * The target thread can continue to execute between the two signals.
     * (The first just causes debuggerd to attach to it.)
     */
#ifdef SIGSTKFLT
#define SIG SIGSTKFLT
#define SIGNAME "SIGSTKFLT"
#elif defined(SIGEMT)
#define SIG SIGEMT
#define SIGNAME "SIGEMT"
#else
#error No signal available for dvmNukeThread
#endif

    ALOGD("threadid=%d: sending two " SIGNAME "s to threadid=%d (tid=%d) to"
          " cause debuggerd dump",
          dvmThreadSelf()->threadId, thread->threadId, thread->systemTid);
    killResult = pthread_kill(thread->handle, SIG);
    if (killResult != 0) {
        ALOGD("NOTE: pthread_kill #1 failed: %s", strerror(killResult));
    }
    usleep(2 * 1000 * 1000);    // TODO: timed-wait until debuggerd attaches
    killResult = pthread_kill(thread->handle, SIG);
    if (killResult != 0) {
        ALOGD("NOTE: pthread_kill #2 failed: %s", strerror(killResult));
    }
    ALOGD("Sent, pausing to let debuggerd run");
    usleep(8 * 1000 * 1000);    // TODO: timed-wait until debuggerd finishes

    /* ignore SIGSEGV so the eventual dvmAbort() doesn't notify debuggerd */
    signal(SIGSEGV, SIG_IGN);
    ALOGD("Continuing");
}
