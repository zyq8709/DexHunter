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

#include "common_throws.h"
#include "debugger.h"
#include "jni_internal.h"
#include "monitor.h"
#include "mirror/object.h"
#include "scoped_thread_state_change.h"
#include "ScopedUtfChars.h"
#include "thread.h"
#include "thread_list.h"

namespace art {

static jobject Thread_currentThread(JNIEnv* env, jclass) {
  ScopedObjectAccess soa(env);
  return soa.AddLocalReference<jobject>(soa.Self()->GetPeer());
}

static jboolean Thread_interrupted(JNIEnv* env, jclass) {
  return static_cast<JNIEnvExt*>(env)->self->Interrupted() ? JNI_TRUE : JNI_FALSE;
}

static jboolean Thread_isInterrupted(JNIEnv* env, jobject java_thread) {
  ScopedObjectAccess soa(env);
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread = Thread::FromManagedThread(soa, java_thread);
  return (thread != NULL) ? thread->IsInterrupted() : JNI_FALSE;
}

static void Thread_nativeCreate(JNIEnv* env, jclass, jobject java_thread, jlong stack_size,
                                jboolean daemon) {
  Thread::CreateNativeThread(env, java_thread, stack_size, daemon == JNI_TRUE);
}

static jint Thread_nativeGetStatus(JNIEnv* env, jobject java_thread, jboolean has_been_started) {
  // Ordinals from Java's Thread.State.
  const jint kJavaNew = 0;
  const jint kJavaRunnable = 1;
  const jint kJavaBlocked = 2;
  const jint kJavaWaiting = 3;
  const jint kJavaTimedWaiting = 4;
  const jint kJavaTerminated = 5;

  ScopedObjectAccess soa(env);
  ThreadState internal_thread_state = (has_been_started ? kTerminated : kStarting);
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread = Thread::FromManagedThread(soa, java_thread);
  if (thread != NULL) {
    internal_thread_state = thread->GetState();
  }
  switch (internal_thread_state) {
    case kTerminated:                     return kJavaTerminated;
    case kRunnable:                       return kJavaRunnable;
    case kTimedWaiting:                   return kJavaTimedWaiting;
    case kSleeping:                       return kJavaTimedWaiting;
    case kBlocked:                        return kJavaBlocked;
    case kWaiting:                        return kJavaWaiting;
    case kStarting:                       return kJavaNew;
    case kNative:                         return kJavaRunnable;
    case kWaitingForGcToComplete:         return kJavaWaiting;
    case kWaitingPerformingGc:            return kJavaWaiting;
    case kWaitingForCheckPointsToRun:     return kJavaWaiting;
    case kWaitingForDebuggerSend:         return kJavaWaiting;
    case kWaitingForDebuggerToAttach:     return kJavaWaiting;
    case kWaitingInMainDebuggerLoop:      return kJavaWaiting;
    case kWaitingForDebuggerSuspension:   return kJavaWaiting;
    case kWaitingForJniOnLoad:            return kJavaWaiting;
    case kWaitingForSignalCatcherOutput:  return kJavaWaiting;
    case kWaitingInMainSignalCatcherLoop: return kJavaWaiting;
    case kSuspended:                      return kJavaRunnable;
    // Don't add a 'default' here so the compiler can spot incompatible enum changes.
  }
  return -1;  // Unreachable.
}

static jboolean Thread_nativeHoldsLock(JNIEnv* env, jobject java_thread, jobject java_object) {
  ScopedObjectAccess soa(env);
  mirror::Object* object = soa.Decode<mirror::Object*>(java_object);
  if (object == NULL) {
    ThrowNullPointerException(NULL, "object == null");
    return JNI_FALSE;
  }
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread = Thread::FromManagedThread(soa, java_thread);
  return thread->HoldsLock(object);
}

static void Thread_nativeInterrupt(JNIEnv* env, jobject java_thread) {
  ScopedObjectAccess soa(env);
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread = Thread::FromManagedThread(soa, java_thread);
  if (thread != NULL) {
    thread->Interrupt();
  }
}

static void Thread_nativeSetName(JNIEnv* env, jobject peer, jstring java_name) {
  ScopedUtfChars name(env, java_name);
  {
    ScopedObjectAccess soa(env);
    if (soa.Decode<mirror::Object*>(peer) == soa.Self()->GetPeer()) {
      soa.Self()->SetThreadName(name.c_str());
      return;
    }
  }
  // Suspend thread to avoid it from killing itself while we set its name. We don't just hold the
  // thread list lock to avoid this, as setting the thread name causes mutator to lock/unlock
  // in the DDMS send code.
  bool timed_out;
  Thread* thread = Thread::SuspendForDebugger(peer, true, &timed_out);
  if (thread != NULL) {
    {
      ScopedObjectAccess soa(env);
      thread->SetThreadName(name.c_str());
    }
    Runtime::Current()->GetThreadList()->Resume(thread, true);
  } else if (timed_out) {
    LOG(ERROR) << "Trying to set thread name to '" << name.c_str() << "' failed as the thread "
        "failed to suspend within a generous timeout.";
  }
}

/*
 * Alter the priority of the specified thread.  "new_priority" will range
 * from Thread.MIN_PRIORITY to Thread.MAX_PRIORITY (1-10), with "normal"
 * threads at Thread.NORM_PRIORITY (5).
 */
static void Thread_nativeSetPriority(JNIEnv* env, jobject java_thread, jint new_priority) {
  ScopedObjectAccess soa(env);
  MutexLock mu(soa.Self(), *Locks::thread_list_lock_);
  Thread* thread = Thread::FromManagedThread(soa, java_thread);
  if (thread != NULL) {
    thread->SetNativePriority(new_priority);
  }
}

static void Thread_sleep(JNIEnv* env, jclass, jobject java_lock, jlong ms, jint ns) {
  ScopedObjectAccess soa(env);
  mirror::Object* lock = soa.Decode<mirror::Object*>(java_lock);
  Monitor::Wait(Thread::Current(), lock, ms, ns, true, kSleeping);
}

/*
 * Causes the thread to temporarily pause and allow other threads to execute.
 *
 * The exact behavior is poorly defined.  Some discussion here:
 *   http://www.cs.umd.edu/~pugh/java/memoryModel/archive/0944.html
 */
static void Thread_yield(JNIEnv*, jobject) {
  sched_yield();
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Thread, currentThread, "()Ljava/lang/Thread;"),
  NATIVE_METHOD(Thread, interrupted, "()Z"),
  NATIVE_METHOD(Thread, isInterrupted, "()Z"),
  NATIVE_METHOD(Thread, nativeCreate, "(Ljava/lang/Thread;JZ)V"),
  NATIVE_METHOD(Thread, nativeGetStatus, "(Z)I"),
  NATIVE_METHOD(Thread, nativeHoldsLock, "(Ljava/lang/Object;)Z"),
  NATIVE_METHOD(Thread, nativeInterrupt, "()V"),
  NATIVE_METHOD(Thread, nativeSetName, "(Ljava/lang/String;)V"),
  NATIVE_METHOD(Thread, nativeSetPriority, "(I)V"),
  NATIVE_METHOD(Thread, sleep, "(Ljava/lang/Object;JI)V"),
  NATIVE_METHOD(Thread, yield, "()V"),
};

void register_java_lang_Thread(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/Thread");
}

}  // namespace art
