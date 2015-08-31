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

#define ATRACE_TAG ATRACE_TAG_DALVIK

#include "thread.h"

#include <cutils/trace.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <algorithm>
#include <bitset>
#include <cerrno>
#include <iostream>
#include <list>

#include "arch/context.h"
#include "base/mutex.h"
#include "class_linker.h"
#include "class_linker-inl.h"
#include "cutils/atomic.h"
#include "cutils/atomic-inline.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "entrypoints/entrypoint_utils.h"
#include "gc_map.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "gc/space/space.h"
#include "invoke_arg_array_builder.h"
#include "jni_internal.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object_array-inl.h"
#include "mirror/stack_trace_element.h"
#include "monitor.h"
#include "object_utils.h"
#include "reflection.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "ScopedUtfChars.h"
#include "sirt_ref.h"
#include "stack.h"
#include "stack_indirect_reference_table.h"
#include "thread-inl.h"
#include "thread_list.h"
#include "utils.h"
#include "verifier/dex_gc_map.h"
#include "verifier/method_verifier.h"
#include "vmap_table.h"
#include "well_known_classes.h"

namespace art {

bool Thread::is_started_ = false;
pthread_key_t Thread::pthread_key_self_;
ConditionVariable* Thread::resume_cond_ = NULL;

static const char* kThreadNameDuringStartup = "<native thread without managed peer>";

void Thread::InitCardTable() {
  card_table_ = Runtime::Current()->GetHeap()->GetCardTable()->GetBiasedBegin();
}

#if !defined(__APPLE__)
static void UnimplementedEntryPoint() {
  UNIMPLEMENTED(FATAL);
}
#endif

void InitEntryPoints(InterpreterEntryPoints* ipoints, JniEntryPoints* jpoints,
                     PortableEntryPoints* ppoints, QuickEntryPoints* qpoints);

void Thread::InitTlsEntryPoints() {
#if !defined(__APPLE__)  // The Mac GCC is too old to accept this code.
  // Insert a placeholder so we can easily tell if we call an unimplemented entry point.
  uintptr_t* begin = reinterpret_cast<uintptr_t*>(&interpreter_entrypoints_);
  uintptr_t* end = reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(begin) + sizeof(quick_entrypoints_));
  for (uintptr_t* it = begin; it != end; ++it) {
    *it = reinterpret_cast<uintptr_t>(UnimplementedEntryPoint);
  }
  begin = reinterpret_cast<uintptr_t*>(&interpreter_entrypoints_);
  end = reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(begin) + sizeof(portable_entrypoints_));
  for (uintptr_t* it = begin; it != end; ++it) {
    *it = reinterpret_cast<uintptr_t>(UnimplementedEntryPoint);
  }
#endif
  InitEntryPoints(&interpreter_entrypoints_, &jni_entrypoints_, &portable_entrypoints_,
                  &quick_entrypoints_);
}

void Thread::SetDeoptimizationShadowFrame(ShadowFrame* sf) {
  deoptimization_shadow_frame_ = sf;
}

void Thread::SetDeoptimizationReturnValue(const JValue& ret_val) {
  deoptimization_return_value_.SetJ(ret_val.GetJ());
}

ShadowFrame* Thread::GetAndClearDeoptimizationShadowFrame(JValue* ret_val) {
  ShadowFrame* sf = deoptimization_shadow_frame_;
  deoptimization_shadow_frame_ = NULL;
  ret_val->SetJ(deoptimization_return_value_.GetJ());
  return sf;
}

void Thread::InitTid() {
  tid_ = ::art::GetTid();
}

void Thread::InitAfterFork() {
  // One thread (us) survived the fork, but we have a new tid so we need to
  // update the value stashed in this Thread*.
  InitTid();
}

void* Thread::CreateCallback(void* arg) {
  Thread* self = reinterpret_cast<Thread*>(arg);
  Runtime* runtime = Runtime::Current();
  if (runtime == NULL) {
    LOG(ERROR) << "Thread attaching to non-existent runtime: " << *self;
    return NULL;
  }
  {
    // TODO: pass self to MutexLock - requires self to equal Thread::Current(), which is only true
    //       after self->Init().
    MutexLock mu(NULL, *Locks::runtime_shutdown_lock_);
    // Check that if we got here we cannot be shutting down (as shutdown should never have started
    // while threads are being born).
    CHECK(!runtime->IsShuttingDown());
    self->Init(runtime->GetThreadList(), runtime->GetJavaVM());
    Runtime::Current()->EndThreadBirth();
  }
  {
    ScopedObjectAccess soa(self);

    // Copy peer into self, deleting global reference when done.
    CHECK(self->jpeer_ != NULL);
    self->opeer_ = soa.Decode<mirror::Object*>(self->jpeer_);
    self->GetJniEnv()->DeleteGlobalRef(self->jpeer_);
    self->jpeer_ = NULL;

    {
      SirtRef<mirror::String> thread_name(self, self->GetThreadName(soa));
      self->SetThreadName(thread_name->ToModifiedUtf8().c_str());
    }
    Dbg::PostThreadStart(self);

    // Invoke the 'run' method of our java.lang.Thread.
    mirror::Object* receiver = self->opeer_;
    jmethodID mid = WellKnownClasses::java_lang_Thread_run;
    mirror::ArtMethod* m =
        receiver->GetClass()->FindVirtualMethodForVirtualOrInterface(soa.DecodeMethod(mid));
    JValue result;
    ArgArray arg_array(NULL, 0);
    arg_array.Append(reinterpret_cast<uint32_t>(receiver));
    m->Invoke(self, arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'V');
  }
  // Detach and delete self.
  Runtime::Current()->GetThreadList()->Unregister(self);

  return NULL;
}

Thread* Thread::FromManagedThread(const ScopedObjectAccessUnchecked& soa,
                                  mirror::Object* thread_peer) {
  mirror::ArtField* f = soa.DecodeField(WellKnownClasses::java_lang_Thread_nativePeer);
  Thread* result = reinterpret_cast<Thread*>(static_cast<uintptr_t>(f->GetInt(thread_peer)));
  // Sanity check that if we have a result it is either suspended or we hold the thread_list_lock_
  // to stop it from going away.
  if (kIsDebugBuild) {
    MutexLock mu(soa.Self(), *Locks::thread_suspend_count_lock_);
    if (result != NULL && !result->IsSuspended()) {
      Locks::thread_list_lock_->AssertHeld(soa.Self());
    }
  }
  return result;
}

Thread* Thread::FromManagedThread(const ScopedObjectAccessUnchecked& soa, jobject java_thread) {
  return FromManagedThread(soa, soa.Decode<mirror::Object*>(java_thread));
}

static size_t FixStackSize(size_t stack_size) {
  // A stack size of zero means "use the default".
  if (stack_size == 0) {
    stack_size = Runtime::Current()->GetDefaultStackSize();
  }

  // Dalvik used the bionic pthread default stack size for native threads,
  // so include that here to support apps that expect large native stacks.
  stack_size += 1 * MB;

  // It's not possible to request a stack smaller than the system-defined PTHREAD_STACK_MIN.
  if (stack_size < PTHREAD_STACK_MIN) {
    stack_size = PTHREAD_STACK_MIN;
  }

  // It's likely that callers are trying to ensure they have at least a certain amount of
  // stack space, so we should add our reserved space on top of what they requested, rather
  // than implicitly take it away from them.
  stack_size += Thread::kStackOverflowReservedBytes;

  // Some systems require the stack size to be a multiple of the system page size, so round up.
  stack_size = RoundUp(stack_size, kPageSize);

  return stack_size;
}

void Thread::CreateNativeThread(JNIEnv* env, jobject java_peer, size_t stack_size, bool is_daemon) {
  CHECK(java_peer != NULL);
  Thread* self = static_cast<JNIEnvExt*>(env)->self;
  Runtime* runtime = Runtime::Current();

  // Atomically start the birth of the thread ensuring the runtime isn't shutting down.
  bool thread_start_during_shutdown = false;
  {
    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
    if (runtime->IsShuttingDown()) {
      thread_start_during_shutdown = true;
    } else {
      runtime->StartThreadBirth();
    }
  }
  if (thread_start_during_shutdown) {
    ScopedLocalRef<jclass> error_class(env, env->FindClass("java/lang/InternalError"));
    env->ThrowNew(error_class.get(), "Thread starting during runtime shutdown");
    return;
  }

  Thread* child_thread = new Thread(is_daemon);
  // Use global JNI ref to hold peer live while child thread starts.
  child_thread->jpeer_ = env->NewGlobalRef(java_peer);
  stack_size = FixStackSize(stack_size);

  // Thread.start is synchronized, so we know that nativePeer is 0, and know that we're not racing to
  // assign it.
  env->SetIntField(java_peer, WellKnownClasses::java_lang_Thread_nativePeer,
                   reinterpret_cast<jint>(child_thread));

  pthread_t new_pthread;
  pthread_attr_t attr;
  CHECK_PTHREAD_CALL(pthread_attr_init, (&attr), "new thread");
  CHECK_PTHREAD_CALL(pthread_attr_setdetachstate, (&attr, PTHREAD_CREATE_DETACHED), "PTHREAD_CREATE_DETACHED");
  CHECK_PTHREAD_CALL(pthread_attr_setstacksize, (&attr, stack_size), stack_size);
  int pthread_create_result = pthread_create(&new_pthread, &attr, Thread::CreateCallback, child_thread);
  CHECK_PTHREAD_CALL(pthread_attr_destroy, (&attr), "new thread");

  if (pthread_create_result != 0) {
    // pthread_create(3) failed, so clean up.
    {
      MutexLock mu(self, *Locks::runtime_shutdown_lock_);
      runtime->EndThreadBirth();
    }
    // Manually delete the global reference since Thread::Init will not have been run.
    env->DeleteGlobalRef(child_thread->jpeer_);
    child_thread->jpeer_ = NULL;
    delete child_thread;
    child_thread = NULL;
    // TODO: remove from thread group?
    env->SetIntField(java_peer, WellKnownClasses::java_lang_Thread_nativePeer, 0);
    {
      std::string msg(StringPrintf("pthread_create (%s stack) failed: %s",
                                   PrettySize(stack_size).c_str(), strerror(pthread_create_result)));
      ScopedObjectAccess soa(env);
      soa.Self()->ThrowOutOfMemoryError(msg.c_str());
    }
  }
}

void Thread::Init(ThreadList* thread_list, JavaVMExt* java_vm) {
  // This function does all the initialization that must be run by the native thread it applies to.
  // (When we create a new thread from managed code, we allocate the Thread* in Thread::Create so
  // we can handshake with the corresponding native thread when it's ready.) Check this native
  // thread hasn't been through here already...
  CHECK(Thread::Current() == NULL);
  SetUpAlternateSignalStack();
  InitCpu();
  InitTlsEntryPoints();
  InitCardTable();
  InitTid();
  // Set pthread_self_ ahead of pthread_setspecific, that makes Thread::Current function, this
  // avoids pthread_self_ ever being invalid when discovered from Thread::Current().
  pthread_self_ = pthread_self();
  CHECK(is_started_);
  CHECK_PTHREAD_CALL(pthread_setspecific, (Thread::pthread_key_self_, this), "attach self");
  DCHECK_EQ(Thread::Current(), this);

  thin_lock_id_ = thread_list->AllocThreadId(this);
  InitStackHwm();

  jni_env_ = new JNIEnvExt(this, java_vm);
  thread_list->Register(this);
}

Thread* Thread::Attach(const char* thread_name, bool as_daemon, jobject thread_group,
                       bool create_peer) {
  Thread* self;
  Runtime* runtime = Runtime::Current();
  if (runtime == NULL) {
    LOG(ERROR) << "Thread attaching to non-existent runtime: " << thread_name;
    return NULL;
  }
  {
    MutexLock mu(NULL, *Locks::runtime_shutdown_lock_);
    if (runtime->IsShuttingDown()) {
      LOG(ERROR) << "Thread attaching while runtime is shutting down: " << thread_name;
      return NULL;
    } else {
      Runtime::Current()->StartThreadBirth();
      self = new Thread(as_daemon);
      self->Init(runtime->GetThreadList(), runtime->GetJavaVM());
      Runtime::Current()->EndThreadBirth();
    }
  }

  CHECK_NE(self->GetState(), kRunnable);
  self->SetState(kNative);

  // If we're the main thread, ClassLinker won't be created until after we're attached,
  // so that thread needs a two-stage attach. Regular threads don't need this hack.
  // In the compiler, all threads need this hack, because no-one's going to be getting
  // a native peer!
  if (create_peer) {
    self->CreatePeer(thread_name, as_daemon, thread_group);
  } else {
    // These aren't necessary, but they improve diagnostics for unit tests & command-line tools.
    if (thread_name != NULL) {
      self->name_->assign(thread_name);
      ::art::SetThreadName(thread_name);
    }
  }

  return self;
}

void Thread::CreatePeer(const char* name, bool as_daemon, jobject thread_group) {
  Runtime* runtime = Runtime::Current();
  CHECK(runtime->IsStarted());
  JNIEnv* env = jni_env_;

  if (thread_group == NULL) {
    thread_group = runtime->GetMainThreadGroup();
  }
  ScopedLocalRef<jobject> thread_name(env, env->NewStringUTF(name));
  jint thread_priority = GetNativePriority();
  jboolean thread_is_daemon = as_daemon;

  ScopedLocalRef<jobject> peer(env, env->AllocObject(WellKnownClasses::java_lang_Thread));
  if (peer.get() == NULL) {
    CHECK(IsExceptionPending());
    return;
  }
  {
    ScopedObjectAccess soa(this);
    opeer_ = soa.Decode<mirror::Object*>(peer.get());
  }
  env->CallNonvirtualVoidMethod(peer.get(),
                                WellKnownClasses::java_lang_Thread,
                                WellKnownClasses::java_lang_Thread_init,
                                thread_group, thread_name.get(), thread_priority, thread_is_daemon);
  AssertNoPendingException();

  Thread* self = this;
  DCHECK_EQ(self, Thread::Current());
  jni_env_->SetIntField(peer.get(), WellKnownClasses::java_lang_Thread_nativePeer,
                        reinterpret_cast<jint>(self));

  ScopedObjectAccess soa(self);
  SirtRef<mirror::String> peer_thread_name(soa.Self(), GetThreadName(soa));
  if (peer_thread_name.get() == NULL) {
    // The Thread constructor should have set the Thread.name to a
    // non-null value. However, because we can run without code
    // available (in the compiler, in tests), we manually assign the
    // fields the constructor should have set.
    soa.DecodeField(WellKnownClasses::java_lang_Thread_daemon)->
        SetBoolean(opeer_, thread_is_daemon);
    soa.DecodeField(WellKnownClasses::java_lang_Thread_group)->
        SetObject(opeer_, soa.Decode<mirror::Object*>(thread_group));
    soa.DecodeField(WellKnownClasses::java_lang_Thread_name)->
        SetObject(opeer_, soa.Decode<mirror::Object*>(thread_name.get()));
    soa.DecodeField(WellKnownClasses::java_lang_Thread_priority)->
        SetInt(opeer_, thread_priority);
    peer_thread_name.reset(GetThreadName(soa));
  }
  // 'thread_name' may have been null, so don't trust 'peer_thread_name' to be non-null.
  if (peer_thread_name.get() != NULL) {
    SetThreadName(peer_thread_name->ToModifiedUtf8().c_str());
  }
}

void Thread::SetThreadName(const char* name) {
  name_->assign(name);
  ::art::SetThreadName(name);
  Dbg::DdmSendThreadNotification(this, CHUNK_TYPE("THNM"));
}

void Thread::InitStackHwm() {
  void* stack_base;
  size_t stack_size;
  GetThreadStack(pthread_self_, stack_base, stack_size);

  // TODO: include this in the thread dumps; potentially useful in SIGQUIT output?
  VLOG(threads) << StringPrintf("Native stack is at %p (%s)", stack_base, PrettySize(stack_size).c_str());

  stack_begin_ = reinterpret_cast<byte*>(stack_base);
  stack_size_ = stack_size;

  if (stack_size_ <= kStackOverflowReservedBytes) {
    LOG(FATAL) << "Attempt to attach a thread with a too-small stack (" << stack_size_ << " bytes)";
  }

  // TODO: move this into the Linux GetThreadStack implementation.
#if !defined(__APPLE__)
  // If we're the main thread, check whether we were run with an unlimited stack. In that case,
  // glibc will have reported a 2GB stack for our 32-bit process, and our stack overflow detection
  // will be broken because we'll die long before we get close to 2GB.
  bool is_main_thread = (::art::GetTid() == getpid());
  if (is_main_thread) {
    rlimit stack_limit;
    if (getrlimit(RLIMIT_STACK, &stack_limit) == -1) {
      PLOG(FATAL) << "getrlimit(RLIMIT_STACK) failed";
    }
    if (stack_limit.rlim_cur == RLIM_INFINITY) {
      // Find the default stack size for new threads...
      pthread_attr_t default_attributes;
      size_t default_stack_size;
      CHECK_PTHREAD_CALL(pthread_attr_init, (&default_attributes), "default stack size query");
      CHECK_PTHREAD_CALL(pthread_attr_getstacksize, (&default_attributes, &default_stack_size),
                         "default stack size query");
      CHECK_PTHREAD_CALL(pthread_attr_destroy, (&default_attributes), "default stack size query");

      // ...and use that as our limit.
      size_t old_stack_size = stack_size_;
      stack_size_ = default_stack_size;
      stack_begin_ += (old_stack_size - stack_size_);
      VLOG(threads) << "Limiting unlimited stack (reported as " << PrettySize(old_stack_size) << ")"
                    << " to " << PrettySize(stack_size_)
                    << " with base " << reinterpret_cast<void*>(stack_begin_);
    }
  }
#endif

  // Set stack_end_ to the bottom of the stack saving space of stack overflows
  ResetDefaultStackEnd();

  // Sanity check.
  int stack_variable;
  CHECK_GT(&stack_variable, reinterpret_cast<void*>(stack_end_));
}

void Thread::ShortDump(std::ostream& os) const {
  os << "Thread[";
  if (GetThinLockId() != 0) {
    // If we're in kStarting, we won't have a thin lock id or tid yet.
    os << GetThinLockId()
             << ",tid=" << GetTid() << ',';
  }
  os << GetState()
           << ",Thread*=" << this
           << ",peer=" << opeer_
           << ",\"" << *name_ << "\""
           << "]";
}

void Thread::Dump(std::ostream& os) const {
  DumpState(os);
  DumpStack(os);
}

mirror::String* Thread::GetThreadName(const ScopedObjectAccessUnchecked& soa) const {
  mirror::ArtField* f = soa.DecodeField(WellKnownClasses::java_lang_Thread_name);
  return (opeer_ != NULL) ? reinterpret_cast<mirror::String*>(f->GetObject(opeer_)) : NULL;
}

void Thread::GetThreadName(std::string& name) const {
  name.assign(*name_);
}

uint64_t Thread::GetCpuMicroTime() const {
#if defined(HAVE_POSIX_CLOCKS)
  clockid_t cpu_clock_id;
  pthread_getcpuclockid(pthread_self_, &cpu_clock_id);
  timespec now;
  clock_gettime(cpu_clock_id, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000000LL + now.tv_nsec / 1000LL;
#else
  UNIMPLEMENTED(WARNING);
  return -1;
#endif
}

void Thread::AtomicSetFlag(ThreadFlag flag) {
  android_atomic_or(flag, &state_and_flags_.as_int);
}

void Thread::AtomicClearFlag(ThreadFlag flag) {
  android_atomic_and(-1 ^ flag, &state_and_flags_.as_int);
}

// Attempt to rectify locks so that we dump thread list with required locks before exiting.
static void UnsafeLogFatalForSuspendCount(Thread* self, Thread* thread) NO_THREAD_SAFETY_ANALYSIS {
  LOG(ERROR) << *thread << " suspend count already zero.";
  Locks::thread_suspend_count_lock_->Unlock(self);
  if (!Locks::mutator_lock_->IsSharedHeld(self)) {
    Locks::mutator_lock_->SharedTryLock(self);
    if (!Locks::mutator_lock_->IsSharedHeld(self)) {
      LOG(WARNING) << "Dumping thread list without holding mutator_lock_";
    }
  }
  if (!Locks::thread_list_lock_->IsExclusiveHeld(self)) {
    Locks::thread_list_lock_->TryLock(self);
    if (!Locks::thread_list_lock_->IsExclusiveHeld(self)) {
      LOG(WARNING) << "Dumping thread list without holding thread_list_lock_";
    }
  }
  std::ostringstream ss;
  Runtime::Current()->GetThreadList()->DumpLocked(ss);
  LOG(FATAL) << ss.str();
}

void Thread::ModifySuspendCount(Thread* self, int delta, bool for_debugger) {
  DCHECK(delta == -1 || delta == +1 || delta == -debug_suspend_count_)
      << delta << " " << debug_suspend_count_ << " " << this;
  DCHECK_GE(suspend_count_, debug_suspend_count_) << this;
  Locks::thread_suspend_count_lock_->AssertHeld(self);
  if (this != self && !IsSuspended()) {
    Locks::thread_list_lock_->AssertHeld(self);
  }
  if (UNLIKELY(delta < 0 && suspend_count_ <= 0)) {
    UnsafeLogFatalForSuspendCount(self, this);
    return;
  }

  suspend_count_ += delta;
  if (for_debugger) {
    debug_suspend_count_ += delta;
  }

  if (suspend_count_ == 0) {
    AtomicClearFlag(kSuspendRequest);
  } else {
    AtomicSetFlag(kSuspendRequest);
  }
}

void Thread::RunCheckpointFunction() {
  CHECK(checkpoint_function_ != NULL);
  ATRACE_BEGIN("Checkpoint function");
  checkpoint_function_->Run(this);
  ATRACE_END();
}

bool Thread::RequestCheckpoint(Closure* function) {
  CHECK(!ReadFlag(kCheckpointRequest)) << "Already have a pending checkpoint request";
  checkpoint_function_ = function;
  union StateAndFlags old_state_and_flags = state_and_flags_;
  // We must be runnable to request a checkpoint.
  old_state_and_flags.as_struct.state = kRunnable;
  union StateAndFlags new_state_and_flags = old_state_and_flags;
  new_state_and_flags.as_struct.flags |= kCheckpointRequest;
  int succeeded = android_atomic_cmpxchg(old_state_and_flags.as_int, new_state_and_flags.as_int,
                                         &state_and_flags_.as_int);
  return succeeded == 0;
}

void Thread::FullSuspendCheck() {
  VLOG(threads) << this << " self-suspending";
  ATRACE_BEGIN("Full suspend check");
  // Make thread appear suspended to other threads, release mutator_lock_.
  TransitionFromRunnableToSuspended(kSuspended);
  // Transition back to runnable noting requests to suspend, re-acquire share on mutator_lock_.
  TransitionFromSuspendedToRunnable();
  ATRACE_END();
  VLOG(threads) << this << " self-reviving";
}

Thread* Thread::SuspendForDebugger(jobject peer, bool request_suspension, bool* timed_out) {
  static const useconds_t kTimeoutUs = 30 * 1000000;  // 30s.
  useconds_t total_delay_us = 0;
  useconds_t delay_us = 0;
  bool did_suspend_request = false;
  *timed_out = false;
  while (true) {
    Thread* thread;
    {
      ScopedObjectAccess soa(Thread::Current());
      Thread* self = soa.Self();
      MutexLock mu(self, *Locks::thread_list_lock_);
      thread = Thread::FromManagedThread(soa, peer);
      if (thread == NULL) {
        JNIEnv* env = self->GetJniEnv();
        ScopedLocalRef<jstring> scoped_name_string(env,
                                                   (jstring)env->GetObjectField(peer,
                                                              WellKnownClasses::java_lang_Thread_name));
        ScopedUtfChars scoped_name_chars(env, scoped_name_string.get());
        if (scoped_name_chars.c_str() == NULL) {
            LOG(WARNING) << "No such thread for suspend: " << peer;
            env->ExceptionClear();
        } else {
            LOG(WARNING) << "No such thread for suspend: " << peer << ":" << scoped_name_chars.c_str();
        }

        return NULL;
      }
      {
        MutexLock mu(soa.Self(), *Locks::thread_suspend_count_lock_);
        if (request_suspension) {
          thread->ModifySuspendCount(soa.Self(), +1, true /* for_debugger */);
          request_suspension = false;
          did_suspend_request = true;
        }
        // IsSuspended on the current thread will fail as the current thread is changed into
        // Runnable above. As the suspend count is now raised if this is the current thread
        // it will self suspend on transition to Runnable, making it hard to work with. It's simpler
        // to just explicitly handle the current thread in the callers to this code.
        CHECK_NE(thread, soa.Self()) << "Attempt to suspend the current thread for the debugger";
        // If thread is suspended (perhaps it was already not Runnable but didn't have a suspend
        // count, or else we've waited and it has self suspended) or is the current thread, we're
        // done.
        if (thread->IsSuspended()) {
          return thread;
        }
        if (total_delay_us >= kTimeoutUs) {
          LOG(ERROR) << "Thread suspension timed out: " << peer;
          if (did_suspend_request) {
            thread->ModifySuspendCount(soa.Self(), -1, true /* for_debugger */);
          }
          *timed_out = true;
          return NULL;
        }
      }
      // Release locks and come out of runnable state.
    }
    for (int i = kLockLevelCount - 1; i >= 0; --i) {
      BaseMutex* held_mutex = Thread::Current()->GetHeldMutex(static_cast<LockLevel>(i));
      if (held_mutex != NULL) {
        LOG(FATAL) << "Holding " << held_mutex->GetName()
            << " while sleeping for thread suspension";
      }
    }
    {
      useconds_t new_delay_us = delay_us * 2;
      CHECK_GE(new_delay_us, delay_us);
      if (new_delay_us < 500000) {  // Don't allow sleeping to be more than 0.5s.
        delay_us = new_delay_us;
      }
    }
    if (delay_us == 0) {
      sched_yield();
      // Default to 1 milliseconds (note that this gets multiplied by 2 before the first sleep).
      delay_us = 500;
    } else {
      usleep(delay_us);
      total_delay_us += delay_us;
    }
  }
}

void Thread::DumpState(std::ostream& os, const Thread* thread, pid_t tid) {
  std::string group_name;
  int priority;
  bool is_daemon = false;
  Thread* self = Thread::Current();

  if (self != NULL && thread != NULL && thread->opeer_ != NULL) {
    ScopedObjectAccessUnchecked soa(self);
    priority = soa.DecodeField(WellKnownClasses::java_lang_Thread_priority)->GetInt(thread->opeer_);
    is_daemon = soa.DecodeField(WellKnownClasses::java_lang_Thread_daemon)->GetBoolean(thread->opeer_);

    mirror::Object* thread_group =
        soa.DecodeField(WellKnownClasses::java_lang_Thread_group)->GetObject(thread->opeer_);

    if (thread_group != NULL) {
      mirror::ArtField* group_name_field =
          soa.DecodeField(WellKnownClasses::java_lang_ThreadGroup_name);
      mirror::String* group_name_string =
          reinterpret_cast<mirror::String*>(group_name_field->GetObject(thread_group));
      group_name = (group_name_string != NULL) ? group_name_string->ToModifiedUtf8() : "<null>";
    }
  } else {
    priority = GetNativePriority();
  }

  std::string scheduler_group_name(GetSchedulerGroupName(tid));
  if (scheduler_group_name.empty()) {
    scheduler_group_name = "default";
  }

  if (thread != NULL) {
    os << '"' << *thread->name_ << '"';
    if (is_daemon) {
      os << " daemon";
    }
    os << " prio=" << priority
       << " tid=" << thread->GetThinLockId()
       << " " << thread->GetState();
    if (thread->IsStillStarting()) {
      os << " (still starting up)";
    }
    os << "\n";
  } else {
    os << '"' << ::art::GetThreadName(tid) << '"'
       << " prio=" << priority
       << " (not attached)\n";
  }

  if (thread != NULL) {
    MutexLock mu(self, *Locks::thread_suspend_count_lock_);
    os << "  | group=\"" << group_name << "\""
       << " sCount=" << thread->suspend_count_
       << " dsCount=" << thread->debug_suspend_count_
       << " obj=" << reinterpret_cast<void*>(thread->opeer_)
       << " self=" << reinterpret_cast<const void*>(thread) << "\n";
  }

  os << "  | sysTid=" << tid
     << " nice=" << getpriority(PRIO_PROCESS, tid)
     << " cgrp=" << scheduler_group_name;
  if (thread != NULL) {
    int policy;
    sched_param sp;
    CHECK_PTHREAD_CALL(pthread_getschedparam, (thread->pthread_self_, &policy, &sp), __FUNCTION__);
    os << " sched=" << policy << "/" << sp.sched_priority
       << " handle=" << reinterpret_cast<void*>(thread->pthread_self_);
  }
  os << "\n";

  // Grab the scheduler stats for this thread.
  std::string scheduler_stats;
  if (ReadFileToString(StringPrintf("/proc/self/task/%d/schedstat", tid), &scheduler_stats)) {
    scheduler_stats.resize(scheduler_stats.size() - 1);  // Lose the trailing '\n'.
  } else {
    scheduler_stats = "0 0 0";
  }

  char native_thread_state = '?';
  int utime = 0;
  int stime = 0;
  int task_cpu = 0;
  GetTaskStats(tid, native_thread_state, utime, stime, task_cpu);

  os << "  | state=" << native_thread_state
     << " schedstat=( " << scheduler_stats << " )"
     << " utm=" << utime
     << " stm=" << stime
     << " core=" << task_cpu
     << " HZ=" << sysconf(_SC_CLK_TCK) << "\n";
  if (thread != NULL) {
    os << "  | stack=" << reinterpret_cast<void*>(thread->stack_begin_) << "-" << reinterpret_cast<void*>(thread->stack_end_)
       << " stackSize=" << PrettySize(thread->stack_size_) << "\n";
  }
}

void Thread::DumpState(std::ostream& os) const {
  Thread::DumpState(os, this, GetTid());
}

struct StackDumpVisitor : public StackVisitor {
  StackDumpVisitor(std::ostream& os, Thread* thread, Context* context, bool can_allocate)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, context), os(os), thread(thread), can_allocate(can_allocate),
        last_method(NULL), last_line_number(0), repetition_count(0), frame_count(0) {
  }

  virtual ~StackDumpVisitor() {
    if (frame_count == 0) {
      os << "  (no managed stack frames)\n";
    }
  }

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* m = GetMethod();
    if (m->IsRuntimeMethod()) {
      return true;
    }
    const int kMaxRepetition = 3;
    mirror::Class* c = m->GetDeclaringClass();
    const mirror::DexCache* dex_cache = c->GetDexCache();
    int line_number = -1;
    if (dex_cache != NULL) {  // be tolerant of bad input
      const DexFile& dex_file = *dex_cache->GetDexFile();
      line_number = dex_file.GetLineNumFromPC(m, GetDexPc());
    }
    if (line_number == last_line_number && last_method == m) {
      repetition_count++;
    } else {
      if (repetition_count >= kMaxRepetition) {
        os << "  ... repeated " << (repetition_count - kMaxRepetition) << " times\n";
      }
      repetition_count = 0;
      last_line_number = line_number;
      last_method = m;
    }
    if (repetition_count < kMaxRepetition) {
      os << "  at " << PrettyMethod(m, false);
      if (m->IsNative()) {
        os << "(Native method)";
      } else {
        mh.ChangeMethod(m);
        const char* source_file(mh.GetDeclaringClassSourceFile());
        os << "(" << (source_file != NULL ? source_file : "unavailable")
           << ":" << line_number << ")";
      }
      os << "\n";
      if (frame_count == 0) {
        Monitor::DescribeWait(os, thread);
      }
      if (can_allocate) {
        Monitor::VisitLocks(this, DumpLockedObject, &os);
      }
    }

    ++frame_count;
    return true;
  }

  static void DumpLockedObject(mirror::Object* o, void* context)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    std::ostream& os = *reinterpret_cast<std::ostream*>(context);
    os << "  - locked <" << o << "> (a " << PrettyTypeOf(o) << ")\n";
  }

  std::ostream& os;
  const Thread* thread;
  const bool can_allocate;
  MethodHelper mh;
  mirror::ArtMethod* last_method;
  int last_line_number;
  int repetition_count;
  int frame_count;
};

static bool ShouldShowNativeStack(const Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ThreadState state = thread->GetState();

  // In native code somewhere in the VM (one of the kWaitingFor* states)? That's interesting.
  if (state > kWaiting && state < kStarting) {
    return true;
  }

  // In an Object.wait variant or Thread.sleep? That's not interesting.
  if (state == kTimedWaiting || state == kSleeping || state == kWaiting) {
    return false;
  }

  // In some other native method? That's interesting.
  // We don't just check kNative because native methods will be in state kSuspended if they're
  // calling back into the VM, or kBlocked if they're blocked on a monitor, or one of the
  // thread-startup states if it's early enough in their life cycle (http://b/7432159).
  mirror::ArtMethod* current_method = thread->GetCurrentMethod(NULL);
  return current_method != NULL && current_method->IsNative();
}

void Thread::DumpStack(std::ostream& os) const {
  // TODO: we call this code when dying but may not have suspended the thread ourself. The
  //       IsSuspended check is therefore racy with the use for dumping (normally we inhibit
  //       the race with the thread_suspend_count_lock_).
  // No point dumping for an abort in debug builds where we'll hit the not suspended check in stack.
  bool dump_for_abort = (gAborting > 0) && !kIsDebugBuild;
  if (this == Thread::Current() || IsSuspended() || dump_for_abort) {
    // If we're currently in native code, dump that stack before dumping the managed stack.
    if (dump_for_abort || ShouldShowNativeStack(this)) {
      DumpKernelStack(os, GetTid(), "  kernel: ", false);
      DumpNativeStack(os, GetTid(), "  native: ", false);
    }
    UniquePtr<Context> context(Context::Create());
    StackDumpVisitor dumper(os, const_cast<Thread*>(this), context.get(), !throwing_OutOfMemoryError_);
    dumper.WalkStack();
  } else {
    os << "Not able to dump stack of thread that isn't suspended";
  }
}

void Thread::ThreadExitCallback(void* arg) {
  Thread* self = reinterpret_cast<Thread*>(arg);
  if (self->thread_exit_check_count_ == 0) {
    LOG(WARNING) << "Native thread exiting without having called DetachCurrentThread (maybe it's going to use a pthread_key_create destructor?): " << *self;
    CHECK(is_started_);
    CHECK_PTHREAD_CALL(pthread_setspecific, (Thread::pthread_key_self_, self), "reattach self");
    self->thread_exit_check_count_ = 1;
  } else {
    LOG(FATAL) << "Native thread exited without calling DetachCurrentThread: " << *self;
  }
}

void Thread::Startup() {
  CHECK(!is_started_);
  is_started_ = true;
  {
    // MutexLock to keep annotalysis happy.
    //
    // Note we use NULL for the thread because Thread::Current can
    // return garbage since (is_started_ == true) and
    // Thread::pthread_key_self_ is not yet initialized.
    // This was seen on glibc.
    MutexLock mu(NULL, *Locks::thread_suspend_count_lock_);
    resume_cond_ = new ConditionVariable("Thread resumption condition variable",
                                         *Locks::thread_suspend_count_lock_);
  }

  // Allocate a TLS slot.
  CHECK_PTHREAD_CALL(pthread_key_create, (&Thread::pthread_key_self_, Thread::ThreadExitCallback), "self key");

  // Double-check the TLS slot allocation.
  if (pthread_getspecific(pthread_key_self_) != NULL) {
    LOG(FATAL) << "Newly-created pthread TLS slot is not NULL";
  }
}

void Thread::FinishStartup() {
  Runtime* runtime = Runtime::Current();
  CHECK(runtime->IsStarted());

  // Finish attaching the main thread.
  ScopedObjectAccess soa(Thread::Current());
  Thread::Current()->CreatePeer("main", false, runtime->GetMainThreadGroup());

  Runtime::Current()->GetClassLinker()->RunRootClinits();
}

void Thread::Shutdown() {
  CHECK(is_started_);
  is_started_ = false;
  CHECK_PTHREAD_CALL(pthread_key_delete, (Thread::pthread_key_self_), "self key");
  MutexLock mu(Thread::Current(), *Locks::thread_suspend_count_lock_);
  if (resume_cond_ != NULL) {
    delete resume_cond_;
    resume_cond_ = NULL;
  }
}

Thread::Thread(bool daemon)
    : suspend_count_(0),
      card_table_(NULL),
      exception_(NULL),
      stack_end_(NULL),
      managed_stack_(),
      jni_env_(NULL),
      self_(NULL),
      opeer_(NULL),
      jpeer_(NULL),
      stack_begin_(NULL),
      stack_size_(0),
      stack_trace_sample_(NULL),
      trace_clock_base_(0),
      thin_lock_id_(0),
      tid_(0),
      wait_mutex_(new Mutex("a thread wait mutex")),
      wait_cond_(new ConditionVariable("a thread wait condition variable", *wait_mutex_)),
      wait_monitor_(NULL),
      interrupted_(false),
      wait_next_(NULL),
      monitor_enter_object_(NULL),
      top_sirt_(NULL),
      runtime_(NULL),
      class_loader_override_(NULL),
      long_jump_context_(NULL),
      throwing_OutOfMemoryError_(false),
      debug_suspend_count_(0),
      debug_invoke_req_(new DebugInvokeReq),
      deoptimization_shadow_frame_(NULL),
      instrumentation_stack_(new std::deque<instrumentation::InstrumentationStackFrame>),
      name_(new std::string(kThreadNameDuringStartup)),
      daemon_(daemon),
      pthread_self_(0),
      no_thread_suspension_(0),
      last_no_thread_suspension_cause_(NULL),
      checkpoint_function_(0),
      thread_exit_check_count_(0) {
  CHECK_EQ((sizeof(Thread) % 4), 0U) << sizeof(Thread);
  state_and_flags_.as_struct.flags = 0;
  state_and_flags_.as_struct.state = kNative;
  memset(&held_mutexes_[0], 0, sizeof(held_mutexes_));
}

bool Thread::IsStillStarting() const {
  // You might think you can check whether the state is kStarting, but for much of thread startup,
  // the thread is in kNative; it might also be in kVmWait.
  // You might think you can check whether the peer is NULL, but the peer is actually created and
  // assigned fairly early on, and needs to be.
  // It turns out that the last thing to change is the thread name; that's a good proxy for "has
  // this thread _ever_ entered kRunnable".
  return (jpeer_ == NULL && opeer_ == NULL) || (*name_ == kThreadNameDuringStartup);
}

void Thread::AssertNoPendingException() const {
  if (UNLIKELY(IsExceptionPending())) {
    ScopedObjectAccess soa(Thread::Current());
    mirror::Throwable* exception = GetException(NULL);
    LOG(FATAL) << "No pending exception expected: " << exception->Dump();
  }
}

static void MonitorExitVisitor(const mirror::Object* object, void* arg) NO_THREAD_SAFETY_ANALYSIS {
  Thread* self = reinterpret_cast<Thread*>(arg);
  mirror::Object* entered_monitor = const_cast<mirror::Object*>(object);
  if (self->HoldsLock(entered_monitor)) {
    LOG(WARNING) << "Calling MonitorExit on object "
                 << object << " (" << PrettyTypeOf(object) << ")"
                 << " left locked by native thread "
                 << *Thread::Current() << " which is detaching";
    entered_monitor->MonitorExit(self);
  }
}

void Thread::Destroy() {
  Thread* self = this;
  DCHECK_EQ(self, Thread::Current());

  if (opeer_ != NULL) {
    ScopedObjectAccess soa(self);
    // We may need to call user-supplied managed code, do this before final clean-up.
    HandleUncaughtExceptions(soa);
    RemoveFromThreadGroup(soa);

    // this.nativePeer = 0;
    soa.DecodeField(WellKnownClasses::java_lang_Thread_nativePeer)->SetInt(opeer_, 0);
    Dbg::PostThreadDeath(self);

    // Thread.join() is implemented as an Object.wait() on the Thread.lock object. Signal anyone
    // who is waiting.
    mirror::Object* lock =
        soa.DecodeField(WellKnownClasses::java_lang_Thread_lock)->GetObject(opeer_);
    // (This conditional is only needed for tests, where Thread.lock won't have been set.)
    if (lock != NULL) {
      ObjectLock locker(self, lock);
      locker.Notify();
    }
  }

  // On thread detach, all monitors entered with JNI MonitorEnter are automatically exited.
  if (jni_env_ != NULL) {
    jni_env_->monitors.VisitRoots(MonitorExitVisitor, self);
  }
}

Thread::~Thread() {
  if (jni_env_ != NULL && jpeer_ != NULL) {
    // If pthread_create fails we don't have a jni env here.
    jni_env_->DeleteGlobalRef(jpeer_);
    jpeer_ = NULL;
  }
  opeer_ = NULL;

  delete jni_env_;
  jni_env_ = NULL;

  CHECK_NE(GetState(), kRunnable);
  // We may be deleting a still born thread.
  SetStateUnsafe(kTerminated);

  delete wait_cond_;
  delete wait_mutex_;

  if (long_jump_context_ != NULL) {
    delete long_jump_context_;
  }

  delete debug_invoke_req_;
  delete instrumentation_stack_;
  delete name_;
  delete stack_trace_sample_;

  TearDownAlternateSignalStack();
}

void Thread::HandleUncaughtExceptions(ScopedObjectAccess& soa) {
  if (!IsExceptionPending()) {
    return;
  }
  ScopedLocalRef<jobject> peer(jni_env_, soa.AddLocalReference<jobject>(opeer_));
  ScopedThreadStateChange tsc(this, kNative);

  // Get and clear the exception.
  ScopedLocalRef<jthrowable> exception(jni_env_, jni_env_->ExceptionOccurred());
  jni_env_->ExceptionClear();

  // If the thread has its own handler, use that.
  ScopedLocalRef<jobject> handler(jni_env_,
                                  jni_env_->GetObjectField(peer.get(),
                                                           WellKnownClasses::java_lang_Thread_uncaughtHandler));
  if (handler.get() == NULL) {
    // Otherwise use the thread group's default handler.
    handler.reset(jni_env_->GetObjectField(peer.get(), WellKnownClasses::java_lang_Thread_group));
  }

  // Call the handler.
  jni_env_->CallVoidMethod(handler.get(),
                           WellKnownClasses::java_lang_Thread$UncaughtExceptionHandler_uncaughtException,
                           peer.get(), exception.get());

  // If the handler threw, clear that exception too.
  jni_env_->ExceptionClear();
}

void Thread::RemoveFromThreadGroup(ScopedObjectAccess& soa) {
  // this.group.removeThread(this);
  // group can be null if we're in the compiler or a test.
  mirror::Object* ogroup = soa.DecodeField(WellKnownClasses::java_lang_Thread_group)->GetObject(opeer_);
  if (ogroup != NULL) {
    ScopedLocalRef<jobject> group(soa.Env(), soa.AddLocalReference<jobject>(ogroup));
    ScopedLocalRef<jobject> peer(soa.Env(), soa.AddLocalReference<jobject>(opeer_));
    ScopedThreadStateChange tsc(soa.Self(), kNative);
    jni_env_->CallVoidMethod(group.get(), WellKnownClasses::java_lang_ThreadGroup_removeThread,
                             peer.get());
  }
}

size_t Thread::NumSirtReferences() {
  size_t count = 0;
  for (StackIndirectReferenceTable* cur = top_sirt_; cur; cur = cur->GetLink()) {
    count += cur->NumberOfReferences();
  }
  return count;
}

bool Thread::SirtContains(jobject obj) const {
  mirror::Object** sirt_entry = reinterpret_cast<mirror::Object**>(obj);
  for (StackIndirectReferenceTable* cur = top_sirt_; cur; cur = cur->GetLink()) {
    if (cur->Contains(sirt_entry)) {
      return true;
    }
  }
  // JNI code invoked from portable code uses shadow frames rather than the SIRT.
  return managed_stack_.ShadowFramesContain(sirt_entry);
}

void Thread::SirtVisitRoots(RootVisitor* visitor, void* arg) {
  for (StackIndirectReferenceTable* cur = top_sirt_; cur; cur = cur->GetLink()) {
    size_t num_refs = cur->NumberOfReferences();
    for (size_t j = 0; j < num_refs; j++) {
      mirror::Object* object = cur->GetReference(j);
      if (object != NULL) {
        visitor(object, arg);
      }
    }
  }
}

mirror::Object* Thread::DecodeJObject(jobject obj) const {
  Locks::mutator_lock_->AssertSharedHeld(this);
  if (obj == NULL) {
    return NULL;
  }
  IndirectRef ref = reinterpret_cast<IndirectRef>(obj);
  IndirectRefKind kind = GetIndirectRefKind(ref);
  mirror::Object* result;
  // The "kinds" below are sorted by the frequency we expect to encounter them.
  if (kind == kLocal) {
    IndirectReferenceTable& locals = jni_env_->locals;
    result = const_cast<mirror::Object*>(locals.Get(ref));
  } else if (kind == kSirtOrInvalid) {
    // TODO: make stack indirect reference table lookup more efficient
    // Check if this is a local reference in the SIRT
    if (LIKELY(SirtContains(obj))) {
      result = *reinterpret_cast<mirror::Object**>(obj);  // Read from SIRT
    } else if (Runtime::Current()->GetJavaVM()->work_around_app_jni_bugs) {
      // Assume an invalid local reference is actually a direct pointer.
      result = reinterpret_cast<mirror::Object*>(obj);
    } else {
      result = kInvalidIndirectRefObject;
    }
  } else if (kind == kGlobal) {
    JavaVMExt* vm = Runtime::Current()->GetJavaVM();
    IndirectReferenceTable& globals = vm->globals;
    ReaderMutexLock mu(const_cast<Thread*>(this), vm->globals_lock);
    result = const_cast<mirror::Object*>(globals.Get(ref));
  } else {
    DCHECK_EQ(kind, kWeakGlobal);
    result = Runtime::Current()->GetJavaVM()->DecodeWeakGlobal(const_cast<Thread*>(this), ref);
    if (result == kClearedJniWeakGlobal) {
      // This is a special case where it's okay to return NULL.
      return nullptr;
    }
  }

  if (UNLIKELY(result == NULL)) {
    JniAbortF(NULL, "use of deleted %s %p", ToStr<IndirectRefKind>(kind).c_str(), obj);
  } else {
    if (kIsDebugBuild && (result != kInvalidIndirectRefObject)) {
      Runtime::Current()->GetHeap()->VerifyObject(result);
    }
  }
  return result;
}

// Implements java.lang.Thread.interrupted.
bool Thread::Interrupted() {
  MutexLock mu(Thread::Current(), *wait_mutex_);
  bool interrupted = interrupted_;
  interrupted_ = false;
  return interrupted;
}

// Implements java.lang.Thread.isInterrupted.
bool Thread::IsInterrupted() {
  MutexLock mu(Thread::Current(), *wait_mutex_);
  return interrupted_;
}

void Thread::Interrupt() {
  Thread* self = Thread::Current();
  MutexLock mu(self, *wait_mutex_);
  if (interrupted_) {
    return;
  }
  interrupted_ = true;
  NotifyLocked(self);
}

void Thread::Notify() {
  Thread* self = Thread::Current();
  MutexLock mu(self, *wait_mutex_);
  NotifyLocked(self);
}

void Thread::NotifyLocked(Thread* self) {
  if (wait_monitor_ != NULL) {
    wait_cond_->Signal(self);
  }
}

class CountStackDepthVisitor : public StackVisitor {
 public:
  explicit CountStackDepthVisitor(Thread* thread)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, NULL),
        depth_(0), skip_depth_(0), skipping_(true) {}

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // We want to skip frames up to and including the exception's constructor.
    // Note we also skip the frame if it doesn't have a method (namely the callee
    // save frame)
    mirror::ArtMethod* m = GetMethod();
    if (skipping_ && !m->IsRuntimeMethod() &&
        !mirror::Throwable::GetJavaLangThrowable()->IsAssignableFrom(m->GetDeclaringClass())) {
      skipping_ = false;
    }
    if (!skipping_) {
      if (!m->IsRuntimeMethod()) {  // Ignore runtime frames (in particular callee save).
        ++depth_;
      }
    } else {
      ++skip_depth_;
    }
    return true;
  }

  int GetDepth() const {
    return depth_;
  }

  int GetSkipDepth() const {
    return skip_depth_;
  }

 private:
  uint32_t depth_;
  uint32_t skip_depth_;
  bool skipping_;
};

class BuildInternalStackTraceVisitor : public StackVisitor {
 public:
  explicit BuildInternalStackTraceVisitor(Thread* self, Thread* thread, int skip_depth)
      : StackVisitor(thread, NULL), self_(self),
        skip_depth_(skip_depth), count_(0), dex_pc_trace_(NULL), method_trace_(NULL) {}

  bool Init(int depth)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Allocate method trace with an extra slot that will hold the PC trace
    SirtRef<mirror::ObjectArray<mirror::Object> >
        method_trace(self_,
                     Runtime::Current()->GetClassLinker()->AllocObjectArray<mirror::Object>(self_,
                                                                                            depth + 1));
    if (method_trace.get() == NULL) {
      return false;
    }
    mirror::IntArray* dex_pc_trace = mirror::IntArray::Alloc(self_, depth);
    if (dex_pc_trace == NULL) {
      return false;
    }
    // Save PC trace in last element of method trace, also places it into the
    // object graph.
    method_trace->Set(depth, dex_pc_trace);
    // Set the Object*s and assert that no thread suspension is now possible.
    const char* last_no_suspend_cause =
        self_->StartAssertNoThreadSuspension("Building internal stack trace");
    CHECK(last_no_suspend_cause == NULL) << last_no_suspend_cause;
    method_trace_ = method_trace.get();
    dex_pc_trace_ = dex_pc_trace;
    return true;
  }

  virtual ~BuildInternalStackTraceVisitor() {
    if (method_trace_ != NULL) {
      self_->EndAssertNoThreadSuspension(NULL);
    }
  }

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (method_trace_ == NULL || dex_pc_trace_ == NULL) {
      return true;  // We're probably trying to fillInStackTrace for an OutOfMemoryError.
    }
    if (skip_depth_ > 0) {
      skip_depth_--;
      return true;
    }
    mirror::ArtMethod* m = GetMethod();
    if (m->IsRuntimeMethod()) {
      return true;  // Ignore runtime frames (in particular callee save).
    }
    method_trace_->Set(count_, m);
    dex_pc_trace_->Set(count_, m->IsProxyMethod() ? DexFile::kDexNoIndex : GetDexPc());
    ++count_;
    return true;
  }

  mirror::ObjectArray<mirror::Object>* GetInternalStackTrace() const {
    return method_trace_;
  }

 private:
  Thread* const self_;
  // How many more frames to skip.
  int32_t skip_depth_;
  // Current position down stack trace.
  uint32_t count_;
  // Array of dex PC values.
  mirror::IntArray* dex_pc_trace_;
  // An array of the methods on the stack, the last entry is a reference to the PC trace.
  mirror::ObjectArray<mirror::Object>* method_trace_;
};

jobject Thread::CreateInternalStackTrace(const ScopedObjectAccessUnchecked& soa) const {
  // Compute depth of stack
  CountStackDepthVisitor count_visitor(const_cast<Thread*>(this));
  count_visitor.WalkStack();
  int32_t depth = count_visitor.GetDepth();
  int32_t skip_depth = count_visitor.GetSkipDepth();

  // Build internal stack trace.
  BuildInternalStackTraceVisitor build_trace_visitor(soa.Self(), const_cast<Thread*>(this),
                                                     skip_depth);
  if (!build_trace_visitor.Init(depth)) {
    return NULL;  // Allocation failed.
  }
  build_trace_visitor.WalkStack();
  mirror::ObjectArray<mirror::Object>* trace = build_trace_visitor.GetInternalStackTrace();
  if (kIsDebugBuild) {
    for (int32_t i = 0; i < trace->GetLength(); ++i) {
      CHECK(trace->Get(i) != NULL);
    }
  }
  return soa.AddLocalReference<jobjectArray>(trace);
}

jobjectArray Thread::InternalStackTraceToStackTraceElementArray(JNIEnv* env, jobject internal,
    jobjectArray output_array, int* stack_depth) {
  // Transition into runnable state to work on Object*/Array*
  ScopedObjectAccess soa(env);
  // Decode the internal stack trace into the depth, method trace and PC trace
  mirror::ObjectArray<mirror::Object>* method_trace =
      soa.Decode<mirror::ObjectArray<mirror::Object>*>(internal);
  int32_t depth = method_trace->GetLength() - 1;

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  jobjectArray result;
  mirror::ObjectArray<mirror::StackTraceElement>* java_traces;
  if (output_array != NULL) {
    // Reuse the array we were given.
    result = output_array;
    java_traces = soa.Decode<mirror::ObjectArray<mirror::StackTraceElement>*>(output_array);
    // ...adjusting the number of frames we'll write to not exceed the array length.
    depth = std::min(depth, java_traces->GetLength());
  } else {
    // Create java_trace array and place in local reference table
    java_traces = class_linker->AllocStackTraceElementArray(soa.Self(), depth);
    if (java_traces == NULL) {
      return NULL;
    }
    result = soa.AddLocalReference<jobjectArray>(java_traces);
  }

  if (stack_depth != NULL) {
    *stack_depth = depth;
  }

  MethodHelper mh;
  for (int32_t i = 0; i < depth; ++i) {
    // Prepare parameters for StackTraceElement(String cls, String method, String file, int line)
    mirror::ArtMethod* method = down_cast<mirror::ArtMethod*>(method_trace->Get(i));
    MethodHelper mh(method);
    int32_t line_number;
    SirtRef<mirror::String> class_name_object(soa.Self(), nullptr);
    SirtRef<mirror::String> source_name_object(soa.Self(), nullptr);
    if (method->IsProxyMethod()) {
      line_number = -1;
      class_name_object.reset(method->GetDeclaringClass()->GetName());
      // source_name_object intentionally left null for proxy methods
    } else {
      mirror::IntArray* pc_trace = down_cast<mirror::IntArray*>(method_trace->Get(depth));
      uint32_t dex_pc = pc_trace->Get(i);
      line_number = mh.GetLineNumFromDexPC(dex_pc);
      // Allocate element, potentially triggering GC
      // TODO: reuse class_name_object via Class::name_?
      const char* descriptor = mh.GetDeclaringClassDescriptor();
      CHECK(descriptor != nullptr);
      std::string class_name(PrettyDescriptor(descriptor));
      class_name_object.reset(mirror::String::AllocFromModifiedUtf8(soa.Self(), class_name.c_str()));
      if (class_name_object.get() == nullptr) {
        return nullptr;
      }
      const char* source_file = mh.GetDeclaringClassSourceFile();
      if (source_file != nullptr) {
        source_name_object.reset(mirror::String::AllocFromModifiedUtf8(soa.Self(), source_file));
        if (source_name_object.get() == nullptr) {
          return nullptr;
        }
      }
    }
    const char* method_name = mh.GetName();
    CHECK(method_name != NULL);
    SirtRef<mirror::String> method_name_object(soa.Self(),
                                               mirror::String::AllocFromModifiedUtf8(soa.Self(),
                                                                                     method_name));
    if (method_name_object.get() == NULL) {
      return NULL;
    }
    mirror::StackTraceElement* obj = mirror::StackTraceElement::Alloc(soa.Self(),
        class_name_object.get(), method_name_object.get(), source_name_object.get(), line_number);
    if (obj == NULL) {
      return NULL;
    }
#ifdef MOVING_GARBAGE_COLLECTOR
    // Re-read after potential GC
    java_traces = Decode<ObjectArray<Object>*>(soa.Env(), result);
    method_trace = down_cast<ObjectArray<Object>*>(Decode<Object*>(soa.Env(), internal));
    pc_trace = down_cast<IntArray*>(method_trace->Get(depth));
#endif
    java_traces->Set(i, obj);
  }
  return result;
}

void Thread::ThrowNewExceptionF(const ThrowLocation& throw_location,
                                const char* exception_class_descriptor, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ThrowNewExceptionV(throw_location, exception_class_descriptor,
                     fmt, args);
  va_end(args);
}

void Thread::ThrowNewExceptionV(const ThrowLocation& throw_location,
                                const char* exception_class_descriptor,
                                const char* fmt, va_list ap) {
  std::string msg;
  StringAppendV(&msg, fmt, ap);
  ThrowNewException(throw_location, exception_class_descriptor, msg.c_str());
}

void Thread::ThrowNewException(const ThrowLocation& throw_location, const char* exception_class_descriptor,
                               const char* msg) {
  AssertNoPendingException();  // Callers should either clear or call ThrowNewWrappedException.
  ThrowNewWrappedException(throw_location, exception_class_descriptor, msg);
}

void Thread::ThrowNewWrappedException(const ThrowLocation& throw_location,
                                      const char* exception_class_descriptor,
                                      const char* msg) {
  DCHECK_EQ(this, Thread::Current());
  // Ensure we don't forget arguments over object allocation.
  SirtRef<mirror::Object> saved_throw_this(this, throw_location.GetThis());
  SirtRef<mirror::ArtMethod> saved_throw_method(this, throw_location.GetMethod());
  // Ignore the cause throw location. TODO: should we report this as a re-throw?
  SirtRef<mirror::Throwable> cause(this, GetException(NULL));
  ClearException();
  Runtime* runtime = Runtime::Current();

  mirror::ClassLoader* cl = NULL;
  if (throw_location.GetMethod() != NULL) {
    cl = throw_location.GetMethod()->GetDeclaringClass()->GetClassLoader();
  }
  SirtRef<mirror::Class>
      exception_class(this, runtime->GetClassLinker()->FindClass(exception_class_descriptor, cl));
  if (UNLIKELY(exception_class.get() == NULL)) {
    CHECK(IsExceptionPending());
    LOG(ERROR) << "No exception class " << PrettyDescriptor(exception_class_descriptor);
    return;
  }

  if (UNLIKELY(!runtime->GetClassLinker()->EnsureInitialized(exception_class.get(), true, true))) {
    DCHECK(IsExceptionPending());
    return;
  }
  DCHECK(!runtime->IsStarted() || exception_class->IsThrowableClass());
  SirtRef<mirror::Throwable> exception(this,
                                down_cast<mirror::Throwable*>(exception_class->AllocObject(this)));

  // Choose an appropriate constructor and set up the arguments.
  const char* signature;
  SirtRef<mirror::String> msg_string(this, NULL);
  if (msg != NULL) {
    // Ensure we remember this and the method over the String allocation.
    msg_string.reset(mirror::String::AllocFromModifiedUtf8(this, msg));
    if (UNLIKELY(msg_string.get() == NULL)) {
      CHECK(IsExceptionPending());  // OOME.
      return;
    }
    if (cause.get() == NULL) {
      signature = "(Ljava/lang/String;)V";
    } else {
      signature = "(Ljava/lang/String;Ljava/lang/Throwable;)V";
    }
  } else {
    if (cause.get() == NULL) {
      signature = "()V";
    } else {
      signature = "(Ljava/lang/Throwable;)V";
    }
  }
  mirror::ArtMethod* exception_init_method =
      exception_class->FindDeclaredDirectMethod("<init>", signature);

  CHECK(exception_init_method != NULL) << "No <init>" << signature << " in "
      << PrettyDescriptor(exception_class_descriptor);

  if (UNLIKELY(!runtime->IsStarted())) {
    // Something is trying to throw an exception without a started runtime, which is the common
    // case in the compiler. We won't be able to invoke the constructor of the exception, so set
    // the exception fields directly.
    if (msg != NULL) {
      exception->SetDetailMessage(msg_string.get());
    }
    if (cause.get() != NULL) {
      exception->SetCause(cause.get());
    }
    ThrowLocation gc_safe_throw_location(saved_throw_this.get(), saved_throw_method.get(),
                                         throw_location.GetDexPc());
    SetException(gc_safe_throw_location, exception.get());
  } else {
    ArgArray args("VLL", 3);
    args.Append(reinterpret_cast<uint32_t>(exception.get()));
    if (msg != NULL) {
      args.Append(reinterpret_cast<uint32_t>(msg_string.get()));
    }
    if (cause.get() != NULL) {
      args.Append(reinterpret_cast<uint32_t>(cause.get()));
    }
    JValue result;
    exception_init_method->Invoke(this, args.GetArray(), args.GetNumBytes(), &result, 'V');
    if (LIKELY(!IsExceptionPending())) {
      ThrowLocation gc_safe_throw_location(saved_throw_this.get(), saved_throw_method.get(),
                                           throw_location.GetDexPc());
      SetException(gc_safe_throw_location, exception.get());
    }
  }
}

void Thread::ThrowOutOfMemoryError(const char* msg) {
  LOG(ERROR) << StringPrintf("Throwing OutOfMemoryError \"%s\"%s",
      msg, (throwing_OutOfMemoryError_ ? " (recursive case)" : ""));
  ThrowLocation throw_location = GetCurrentLocationForThrow();
  if (!throwing_OutOfMemoryError_) {
    throwing_OutOfMemoryError_ = true;
    ThrowNewException(throw_location, "Ljava/lang/OutOfMemoryError;", msg);
    throwing_OutOfMemoryError_ = false;
  } else {
    Dump(LOG(ERROR));  // The pre-allocated OOME has no stack, so help out and log one.
    SetException(throw_location, Runtime::Current()->GetPreAllocatedOutOfMemoryError());
  }
}

Thread* Thread::CurrentFromGdb() {
  return Thread::Current();
}

void Thread::DumpFromGdb() const {
  std::ostringstream ss;
  Dump(ss);
  std::string str(ss.str());
  // log to stderr for debugging command line processes
  std::cerr << str;
#ifdef HAVE_ANDROID_OS
  // log to logcat for debugging frameworks processes
  LOG(INFO) << str;
#endif
}

struct EntryPointInfo {
  uint32_t offset;
  const char* name;
};
#define INTERPRETER_ENTRY_POINT_INFO(x) { INTERPRETER_ENTRYPOINT_OFFSET(x).Uint32Value(), #x }
#define JNI_ENTRY_POINT_INFO(x)         { JNI_ENTRYPOINT_OFFSET(x).Uint32Value(), #x }
#define PORTABLE_ENTRY_POINT_INFO(x)    { PORTABLE_ENTRYPOINT_OFFSET(x).Uint32Value(), #x }
#define QUICK_ENTRY_POINT_INFO(x)       { QUICK_ENTRYPOINT_OFFSET(x).Uint32Value(), #x }
static const EntryPointInfo gThreadEntryPointInfo[] = {
  INTERPRETER_ENTRY_POINT_INFO(pInterpreterToInterpreterBridge),
  INTERPRETER_ENTRY_POINT_INFO(pInterpreterToCompiledCodeBridge),
  JNI_ENTRY_POINT_INFO(pDlsymLookup),
  PORTABLE_ENTRY_POINT_INFO(pPortableResolutionTrampoline),
  PORTABLE_ENTRY_POINT_INFO(pPortableToInterpreterBridge),
  QUICK_ENTRY_POINT_INFO(pAllocArray),
  QUICK_ENTRY_POINT_INFO(pAllocArrayWithAccessCheck),
  QUICK_ENTRY_POINT_INFO(pAllocObject),
  QUICK_ENTRY_POINT_INFO(pAllocObjectWithAccessCheck),
  QUICK_ENTRY_POINT_INFO(pCheckAndAllocArray),
  QUICK_ENTRY_POINT_INFO(pCheckAndAllocArrayWithAccessCheck),
  QUICK_ENTRY_POINT_INFO(pInstanceofNonTrivial),
  QUICK_ENTRY_POINT_INFO(pCanPutArrayElement),
  QUICK_ENTRY_POINT_INFO(pCheckCast),
  QUICK_ENTRY_POINT_INFO(pInitializeStaticStorage),
  QUICK_ENTRY_POINT_INFO(pInitializeTypeAndVerifyAccess),
  QUICK_ENTRY_POINT_INFO(pInitializeType),
  QUICK_ENTRY_POINT_INFO(pResolveString),
  QUICK_ENTRY_POINT_INFO(pSet32Instance),
  QUICK_ENTRY_POINT_INFO(pSet32Static),
  QUICK_ENTRY_POINT_INFO(pSet64Instance),
  QUICK_ENTRY_POINT_INFO(pSet64Static),
  QUICK_ENTRY_POINT_INFO(pSetObjInstance),
  QUICK_ENTRY_POINT_INFO(pSetObjStatic),
  QUICK_ENTRY_POINT_INFO(pGet32Instance),
  QUICK_ENTRY_POINT_INFO(pGet32Static),
  QUICK_ENTRY_POINT_INFO(pGet64Instance),
  QUICK_ENTRY_POINT_INFO(pGet64Static),
  QUICK_ENTRY_POINT_INFO(pGetObjInstance),
  QUICK_ENTRY_POINT_INFO(pGetObjStatic),
  QUICK_ENTRY_POINT_INFO(pHandleFillArrayData),
  QUICK_ENTRY_POINT_INFO(pJniMethodStart),
  QUICK_ENTRY_POINT_INFO(pJniMethodStartSynchronized),
  QUICK_ENTRY_POINT_INFO(pJniMethodEnd),
  QUICK_ENTRY_POINT_INFO(pJniMethodEndSynchronized),
  QUICK_ENTRY_POINT_INFO(pJniMethodEndWithReference),
  QUICK_ENTRY_POINT_INFO(pJniMethodEndWithReferenceSynchronized),
  QUICK_ENTRY_POINT_INFO(pLockObject),
  QUICK_ENTRY_POINT_INFO(pUnlockObject),
  QUICK_ENTRY_POINT_INFO(pCmpgDouble),
  QUICK_ENTRY_POINT_INFO(pCmpgFloat),
  QUICK_ENTRY_POINT_INFO(pCmplDouble),
  QUICK_ENTRY_POINT_INFO(pCmplFloat),
  QUICK_ENTRY_POINT_INFO(pFmod),
  QUICK_ENTRY_POINT_INFO(pSqrt),
  QUICK_ENTRY_POINT_INFO(pL2d),
  QUICK_ENTRY_POINT_INFO(pFmodf),
  QUICK_ENTRY_POINT_INFO(pL2f),
  QUICK_ENTRY_POINT_INFO(pD2iz),
  QUICK_ENTRY_POINT_INFO(pF2iz),
  QUICK_ENTRY_POINT_INFO(pIdivmod),
  QUICK_ENTRY_POINT_INFO(pD2l),
  QUICK_ENTRY_POINT_INFO(pF2l),
  QUICK_ENTRY_POINT_INFO(pLdiv),
  QUICK_ENTRY_POINT_INFO(pLdivmod),
  QUICK_ENTRY_POINT_INFO(pLmul),
  QUICK_ENTRY_POINT_INFO(pShlLong),
  QUICK_ENTRY_POINT_INFO(pShrLong),
  QUICK_ENTRY_POINT_INFO(pUshrLong),
  QUICK_ENTRY_POINT_INFO(pIndexOf),
  QUICK_ENTRY_POINT_INFO(pMemcmp16),
  QUICK_ENTRY_POINT_INFO(pStringCompareTo),
  QUICK_ENTRY_POINT_INFO(pMemcpy),
  QUICK_ENTRY_POINT_INFO(pQuickResolutionTrampoline),
  QUICK_ENTRY_POINT_INFO(pQuickToInterpreterBridge),
  QUICK_ENTRY_POINT_INFO(pInvokeDirectTrampolineWithAccessCheck),
  QUICK_ENTRY_POINT_INFO(pInvokeInterfaceTrampoline),
  QUICK_ENTRY_POINT_INFO(pInvokeInterfaceTrampolineWithAccessCheck),
  QUICK_ENTRY_POINT_INFO(pInvokeStaticTrampolineWithAccessCheck),
  QUICK_ENTRY_POINT_INFO(pInvokeSuperTrampolineWithAccessCheck),
  QUICK_ENTRY_POINT_INFO(pInvokeVirtualTrampolineWithAccessCheck),
  QUICK_ENTRY_POINT_INFO(pCheckSuspend),
  QUICK_ENTRY_POINT_INFO(pTestSuspend),
  QUICK_ENTRY_POINT_INFO(pDeliverException),
  QUICK_ENTRY_POINT_INFO(pThrowArrayBounds),
  QUICK_ENTRY_POINT_INFO(pThrowDivZero),
  QUICK_ENTRY_POINT_INFO(pThrowNoSuchMethod),
  QUICK_ENTRY_POINT_INFO(pThrowNullPointer),
  QUICK_ENTRY_POINT_INFO(pThrowStackOverflow),
};
#undef QUICK_ENTRY_POINT_INFO

void Thread::DumpThreadOffset(std::ostream& os, uint32_t offset, size_t size_of_pointers) {
  CHECK_EQ(size_of_pointers, 4U);  // TODO: support 64-bit targets.

#define DO_THREAD_OFFSET(x) \
    if (offset == static_cast<uint32_t>(OFFSETOF_VOLATILE_MEMBER(Thread, x))) { \
      os << # x; \
      return; \
    }
  DO_THREAD_OFFSET(state_and_flags_);
  DO_THREAD_OFFSET(card_table_);
  DO_THREAD_OFFSET(exception_);
  DO_THREAD_OFFSET(opeer_);
  DO_THREAD_OFFSET(jni_env_);
  DO_THREAD_OFFSET(self_);
  DO_THREAD_OFFSET(stack_end_);
  DO_THREAD_OFFSET(suspend_count_);
  DO_THREAD_OFFSET(thin_lock_id_);
  // DO_THREAD_OFFSET(top_of_managed_stack_);
  // DO_THREAD_OFFSET(top_of_managed_stack_pc_);
  DO_THREAD_OFFSET(top_sirt_);
#undef DO_THREAD_OFFSET

  size_t entry_point_count = arraysize(gThreadEntryPointInfo);
  CHECK_EQ(entry_point_count * size_of_pointers,
           sizeof(InterpreterEntryPoints) + sizeof(JniEntryPoints) + sizeof(PortableEntryPoints) +
           sizeof(QuickEntryPoints));
  uint32_t expected_offset = OFFSETOF_MEMBER(Thread, interpreter_entrypoints_);
  for (size_t i = 0; i < entry_point_count; ++i) {
    CHECK_EQ(gThreadEntryPointInfo[i].offset, expected_offset) << gThreadEntryPointInfo[i].name;
    expected_offset += size_of_pointers;
    if (gThreadEntryPointInfo[i].offset == offset) {
      os << gThreadEntryPointInfo[i].name;
      return;
    }
  }
  os << offset;
}

static const bool kDebugExceptionDelivery = false;
class CatchBlockStackVisitor : public StackVisitor {
 public:
  CatchBlockStackVisitor(Thread* self, const ThrowLocation& throw_location,
                         mirror::Throwable* exception, bool is_deoptimization)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(self, self->GetLongJumpContext()),
        self_(self), exception_(exception), is_deoptimization_(is_deoptimization),
        to_find_(is_deoptimization ? NULL : exception->GetClass()), throw_location_(throw_location),
        handler_quick_frame_(NULL), handler_quick_frame_pc_(0), handler_dex_pc_(0),
        native_method_count_(0), clear_exception_(false),
        method_tracing_active_(is_deoptimization ||
                               Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled()),
        instrumentation_frames_to_pop_(0), top_shadow_frame_(NULL), prev_shadow_frame_(NULL) {
    // Exception not in root sets, can't allow GC.
    last_no_assert_suspension_cause_ = self->StartAssertNoThreadSuspension("Finding catch block");
  }

  ~CatchBlockStackVisitor() {
    LOG(FATAL) << "UNREACHABLE";  // Expected to take long jump.
  }

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method = GetMethod();
    if (method == NULL) {
      // This is the upcall, we remember the frame and last pc so that we may long jump to them.
      handler_quick_frame_pc_ = GetCurrentQuickFramePc();
      handler_quick_frame_ = GetCurrentQuickFrame();
      return false;  // End stack walk.
    } else {
      if (UNLIKELY(method_tracing_active_ &&
                   GetQuickInstrumentationExitPc() == GetReturnPc())) {
        // Keep count of the number of unwinds during instrumentation.
        instrumentation_frames_to_pop_++;
      }
      if (method->IsRuntimeMethod()) {
        // Ignore callee save method.
        DCHECK(method->IsCalleeSaveMethod());
        return true;
      } else if (is_deoptimization_) {
        return HandleDeoptimization(method);
      } else {
        return HandleTryItems(method);
      }
    }
  }

  bool HandleTryItems(mirror::ArtMethod* method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    uint32_t dex_pc = DexFile::kDexNoIndex;
    if (method->IsNative()) {
      native_method_count_++;
    } else {
      dex_pc = GetDexPc();
    }
    if (dex_pc != DexFile::kDexNoIndex) {
      uint32_t found_dex_pc = method->FindCatchBlock(to_find_, dex_pc, &clear_exception_);
      if (found_dex_pc != DexFile::kDexNoIndex) {
        handler_dex_pc_ = found_dex_pc;
        handler_quick_frame_pc_ = method->ToNativePc(found_dex_pc);
        handler_quick_frame_ = GetCurrentQuickFrame();
        return false;  // End stack walk.
      }
    }
    return true;  // Continue stack walk.
  }

  bool HandleDeoptimization(mirror::ArtMethod* m) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    MethodHelper mh(m);
    const DexFile::CodeItem* code_item = mh.GetCodeItem();
    CHECK(code_item != NULL);
    uint16_t num_regs =  code_item->registers_size_;
    uint32_t dex_pc = GetDexPc();
    const Instruction* inst = Instruction::At(code_item->insns_ + dex_pc);
    uint32_t new_dex_pc = dex_pc + inst->SizeInCodeUnits();
    ShadowFrame* new_frame = ShadowFrame::Create(num_regs, NULL, m, new_dex_pc);
    verifier::MethodVerifier verifier(&mh.GetDexFile(), mh.GetDexCache(), mh.GetClassLoader(),
                                      &mh.GetClassDef(), code_item,
                                      m->GetDexMethodIndex(), m, m->GetAccessFlags(), false, true);
    verifier.Verify();
    std::vector<int32_t> kinds = verifier.DescribeVRegs(dex_pc);
    for (uint16_t reg = 0; reg < num_regs; reg++) {
      VRegKind kind = static_cast<VRegKind>(kinds.at(reg * 2));
      switch (kind) {
        case kUndefined:
          new_frame->SetVReg(reg, 0xEBADDE09);
          break;
        case kConstant:
          new_frame->SetVReg(reg, kinds.at((reg * 2) + 1));
          break;
        case kReferenceVReg:
          new_frame->SetVRegReference(reg,
                                      reinterpret_cast<mirror::Object*>(GetVReg(m, reg, kind)));
          break;
        default:
          new_frame->SetVReg(reg, GetVReg(m, reg, kind));
          break;
      }
    }
    if (prev_shadow_frame_ != NULL) {
      prev_shadow_frame_->SetLink(new_frame);
    } else {
      top_shadow_frame_ = new_frame;
    }
    prev_shadow_frame_ = new_frame;
    return true;
  }

  void DoLongJump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* catch_method = *handler_quick_frame_;
    if (catch_method == NULL) {
      if (kDebugExceptionDelivery) {
        LOG(INFO) << "Handler is upcall";
      }
    } else {
      CHECK(!is_deoptimization_);
      if (kDebugExceptionDelivery) {
        const DexFile& dex_file = *catch_method->GetDeclaringClass()->GetDexCache()->GetDexFile();
        int line_number = dex_file.GetLineNumFromPC(catch_method, handler_dex_pc_);
        LOG(INFO) << "Handler: " << PrettyMethod(catch_method) << " (line: " << line_number << ")";
      }
    }
    if (clear_exception_) {
      // Exception was cleared as part of delivery.
      DCHECK(!self_->IsExceptionPending());
    } else {
      // Put exception back in root set with clear throw location.
      self_->SetException(ThrowLocation(), exception_);
    }
    self_->EndAssertNoThreadSuspension(last_no_assert_suspension_cause_);
    // Do instrumentation events after allowing thread suspension again.
    instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
    for (size_t i = 0; i < instrumentation_frames_to_pop_; ++i) {
      // We pop the instrumentation stack here so as not to corrupt it during the stack walk.
      if (i != instrumentation_frames_to_pop_ - 1 || self_->GetInstrumentationStack()->front().method_ != catch_method) {
        // Don't pop the instrumentation frame of the catch handler.
        instrumentation->PopMethodForUnwind(self_, is_deoptimization_);
      }
    }
    if (!is_deoptimization_) {
      instrumentation->ExceptionCaughtEvent(self_, throw_location_, catch_method, handler_dex_pc_,
                                            exception_);
    } else {
      // TODO: proper return value.
      self_->SetDeoptimizationShadowFrame(top_shadow_frame_);
    }
    // Place context back on thread so it will be available when we continue.
    self_->ReleaseLongJumpContext(context_);
    context_->SetSP(reinterpret_cast<uintptr_t>(handler_quick_frame_));
    CHECK_NE(handler_quick_frame_pc_, 0u);
    context_->SetPC(handler_quick_frame_pc_);
    context_->SmashCallerSaves();
    context_->DoLongJump();
  }

 private:
  Thread* const self_;
  mirror::Throwable* const exception_;
  const bool is_deoptimization_;
  // The type of the exception catch block to find.
  mirror::Class* const to_find_;
  // Location of the throw.
  const ThrowLocation& throw_location_;
  // Quick frame with found handler or last frame if no handler found.
  mirror::ArtMethod** handler_quick_frame_;
  // PC to branch to for the handler.
  uintptr_t handler_quick_frame_pc_;
  // Associated dex PC.
  uint32_t handler_dex_pc_;
  // Number of native methods passed in crawl (equates to number of SIRTs to pop)
  uint32_t native_method_count_;
  // Should the exception be cleared as the catch block has no move-exception?
  bool clear_exception_;
  // Is method tracing active?
  const bool method_tracing_active_;
  // Support for nesting no thread suspension checks.
  const char* last_no_assert_suspension_cause_;
  // Number of frames to pop in long jump.
  size_t instrumentation_frames_to_pop_;
  ShadowFrame* top_shadow_frame_;
  ShadowFrame* prev_shadow_frame_;
};

void Thread::QuickDeliverException() {
  // Get exception from thread.
  ThrowLocation throw_location;
  mirror::Throwable* exception = GetException(&throw_location);
  CHECK(exception != NULL);
  // Don't leave exception visible while we try to find the handler, which may cause class
  // resolution.
  ClearException();
  bool is_deoptimization = (exception == reinterpret_cast<mirror::Throwable*>(-1));
  if (kDebugExceptionDelivery) {
    if (!is_deoptimization) {
      mirror::String* msg = exception->GetDetailMessage();
      std::string str_msg(msg != NULL ? msg->ToModifiedUtf8() : "");
      DumpStack(LOG(INFO) << "Delivering exception: " << PrettyTypeOf(exception)
                << ": " << str_msg << "\n");
    } else {
      DumpStack(LOG(INFO) << "Deoptimizing: ");
    }
  }
  CatchBlockStackVisitor catch_finder(this, throw_location, exception, is_deoptimization);
  catch_finder.WalkStack(true);
  catch_finder.DoLongJump();
  LOG(FATAL) << "UNREACHABLE";
}

Context* Thread::GetLongJumpContext() {
  Context* result = long_jump_context_;
  if (result == NULL) {
    result = Context::Create();
  } else {
    long_jump_context_ = NULL;  // Avoid context being shared.
    result->Reset();
  }
  return result;
}

struct CurrentMethodVisitor : public StackVisitor {
  CurrentMethodVisitor(Thread* thread, Context* context)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, context), this_object_(NULL), method_(NULL), dex_pc_(0) {}
  virtual bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* m = GetMethod();
    if (m->IsRuntimeMethod()) {
      // Continue if this is a runtime method.
      return true;
    }
    if (context_ != NULL) {
      this_object_ = GetThisObject();
    }
    method_ = m;
    dex_pc_ = GetDexPc();
    return false;
  }
  mirror::Object* this_object_;
  mirror::ArtMethod* method_;
  uint32_t dex_pc_;
};

mirror::ArtMethod* Thread::GetCurrentMethod(uint32_t* dex_pc) const {
  CurrentMethodVisitor visitor(const_cast<Thread*>(this), NULL);
  visitor.WalkStack(false);
  if (dex_pc != NULL) {
    *dex_pc = visitor.dex_pc_;
  }
  return visitor.method_;
}

ThrowLocation Thread::GetCurrentLocationForThrow() {
  Context* context = GetLongJumpContext();
  CurrentMethodVisitor visitor(this, context);
  visitor.WalkStack(false);
  ReleaseLongJumpContext(context);
  return ThrowLocation(visitor.this_object_, visitor.method_, visitor.dex_pc_);
}

bool Thread::HoldsLock(mirror::Object* object) {
  if (object == NULL) {
    return false;
  }
  return object->GetThinLockId() == thin_lock_id_;
}

// RootVisitor parameters are: (const Object* obj, size_t vreg, const StackVisitor* visitor).
template <typename RootVisitor>
class ReferenceMapVisitor : public StackVisitor {
 public:
  ReferenceMapVisitor(Thread* thread, Context* context, const RootVisitor& visitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, context), visitor_(visitor) {}

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (false) {
      LOG(INFO) << "Visiting stack roots in " << PrettyMethod(GetMethod())
          << StringPrintf("@ PC:%04x", GetDexPc());
    }
    ShadowFrame* shadow_frame = GetCurrentShadowFrame();
    if (shadow_frame != NULL) {
      mirror::ArtMethod* m = shadow_frame->GetMethod();
      size_t num_regs = shadow_frame->NumberOfVRegs();
      if (m->IsNative() || shadow_frame->HasReferenceArray()) {
        // SIRT for JNI or References for interpreter.
        for (size_t reg = 0; reg < num_regs; ++reg) {
          mirror::Object* ref = shadow_frame->GetVRegReference(reg);
          if (ref != NULL) {
            visitor_(ref, reg, this);
          }
        }
      } else {
        // Java method.
        // Portable path use DexGcMap and store in Method.native_gc_map_.
        const uint8_t* gc_map = m->GetNativeGcMap();
        CHECK(gc_map != NULL) << PrettyMethod(m);
        uint32_t gc_map_length = static_cast<uint32_t>((gc_map[0] << 24) |
                                                       (gc_map[1] << 16) |
                                                       (gc_map[2] << 8) |
                                                       (gc_map[3] << 0));
        verifier::DexPcToReferenceMap dex_gc_map(gc_map + 4, gc_map_length);
        uint32_t dex_pc = GetDexPc();
        const uint8_t* reg_bitmap = dex_gc_map.FindBitMap(dex_pc);
        DCHECK(reg_bitmap != NULL);
        num_regs = std::min(dex_gc_map.RegWidth() * 8, num_regs);
        for (size_t reg = 0; reg < num_regs; ++reg) {
          if (TestBitmap(reg, reg_bitmap)) {
            mirror::Object* ref = shadow_frame->GetVRegReference(reg);
            if (ref != NULL) {
              visitor_(ref, reg, this);
            }
          }
        }
      }
    } else {
      mirror::ArtMethod* m = GetMethod();
      // Process register map (which native and runtime methods don't have)
      if (!m->IsNative() && !m->IsRuntimeMethod() && !m->IsProxyMethod()) {
        const uint8_t* native_gc_map = m->GetNativeGcMap();
        CHECK(native_gc_map != NULL) << PrettyMethod(m);
        mh_.ChangeMethod(m);
        const DexFile::CodeItem* code_item = mh_.GetCodeItem();
        DCHECK(code_item != NULL) << PrettyMethod(m);  // Can't be NULL or how would we compile its instructions?
        NativePcOffsetToReferenceMap map(native_gc_map);
        size_t num_regs = std::min(map.RegWidth() * 8,
                                   static_cast<size_t>(code_item->registers_size_));
        if (num_regs > 0) {
          const uint8_t* reg_bitmap = map.FindBitMap(GetNativePcOffset());
          DCHECK(reg_bitmap != NULL);
          const VmapTable vmap_table(m->GetVmapTable());
          uint32_t core_spills = m->GetCoreSpillMask();
          uint32_t fp_spills = m->GetFpSpillMask();
          size_t frame_size = m->GetFrameSizeInBytes();
          // For all dex registers in the bitmap
          mirror::ArtMethod** cur_quick_frame = GetCurrentQuickFrame();
          DCHECK(cur_quick_frame != NULL);
          for (size_t reg = 0; reg < num_regs; ++reg) {
            // Does this register hold a reference?
            if (TestBitmap(reg, reg_bitmap)) {
              uint32_t vmap_offset;
              mirror::Object* ref;
              if (vmap_table.IsInContext(reg, kReferenceVReg, &vmap_offset)) {
                uintptr_t val = GetGPR(vmap_table.ComputeRegister(core_spills, vmap_offset,
                                                                  kReferenceVReg));
                ref = reinterpret_cast<mirror::Object*>(val);
              } else {
                ref = reinterpret_cast<mirror::Object*>(GetVReg(cur_quick_frame, code_item,
                                                                core_spills, fp_spills, frame_size,
                                                                reg));
              }

              if (ref != NULL) {
                visitor_(ref, reg, this);
              }
            }
          }
        }
      }
    }
    return true;
  }

 private:
  static bool TestBitmap(int reg, const uint8_t* reg_vector) {
    return ((reg_vector[reg / 8] >> (reg % 8)) & 0x01) != 0;
  }

  // Visitor for when we visit a root.
  const RootVisitor& visitor_;

  // A method helper we keep around to avoid dex file/cache re-computations.
  MethodHelper mh_;
};

class RootCallbackVisitor {
 public:
  RootCallbackVisitor(RootVisitor* visitor, void* arg) : visitor_(visitor), arg_(arg) {}

  void operator()(const mirror::Object* obj, size_t, const StackVisitor*) const {
    visitor_(obj, arg_);
  }

 private:
  RootVisitor* visitor_;
  void* arg_;
};

class VerifyCallbackVisitor {
 public:
  VerifyCallbackVisitor(VerifyRootVisitor* visitor, void* arg)
      : visitor_(visitor),
        arg_(arg) {
  }

  void operator()(const mirror::Object* obj, size_t vreg, const StackVisitor* visitor) const {
    visitor_(obj, arg_, vreg, visitor);
  }

 private:
  VerifyRootVisitor* const visitor_;
  void* const arg_;
};

struct VerifyRootWrapperArg {
  VerifyRootVisitor* visitor;
  void* arg;
};

static void VerifyRootWrapperCallback(const mirror::Object* root, void* arg) {
  VerifyRootWrapperArg* wrapperArg = reinterpret_cast<VerifyRootWrapperArg*>(arg);
  wrapperArg->visitor(root, wrapperArg->arg, 0, NULL);
}

void Thread::VerifyRoots(VerifyRootVisitor* visitor, void* arg) {
  // We need to map from a RootVisitor to VerifyRootVisitor, so pass in nulls for arguments we
  // don't have.
  VerifyRootWrapperArg wrapperArg;
  wrapperArg.arg = arg;
  wrapperArg.visitor = visitor;

  if (opeer_ != NULL) {
    VerifyRootWrapperCallback(opeer_, &wrapperArg);
  }
  if (exception_ != NULL) {
    VerifyRootWrapperCallback(exception_, &wrapperArg);
  }
  throw_location_.VisitRoots(VerifyRootWrapperCallback, &wrapperArg);
  if (class_loader_override_ != NULL) {
    VerifyRootWrapperCallback(class_loader_override_, &wrapperArg);
  }
  jni_env_->locals.VisitRoots(VerifyRootWrapperCallback, &wrapperArg);
  jni_env_->monitors.VisitRoots(VerifyRootWrapperCallback, &wrapperArg);

  SirtVisitRoots(VerifyRootWrapperCallback, &wrapperArg);

  // Visit roots on this thread's stack
  Context* context = GetLongJumpContext();
  VerifyCallbackVisitor visitorToCallback(visitor, arg);
  ReferenceMapVisitor<VerifyCallbackVisitor> mapper(this, context, visitorToCallback);
  mapper.WalkStack();
  ReleaseLongJumpContext(context);

  std::deque<instrumentation::InstrumentationStackFrame>* instrumentation_stack = GetInstrumentationStack();
  typedef std::deque<instrumentation::InstrumentationStackFrame>::const_iterator It;
  for (It it = instrumentation_stack->begin(), end = instrumentation_stack->end(); it != end; ++it) {
    mirror::Object* this_object = (*it).this_object_;
    if (this_object != NULL) {
      VerifyRootWrapperCallback(this_object, &wrapperArg);
    }
    mirror::ArtMethod* method = (*it).method_;
    VerifyRootWrapperCallback(method, &wrapperArg);
  }
}

void Thread::VisitRoots(RootVisitor* visitor, void* arg) {
  if (opeer_ != NULL) {
    visitor(opeer_, arg);
  }
  if (exception_ != NULL) {
    visitor(exception_, arg);
  }
  throw_location_.VisitRoots(visitor, arg);
  if (class_loader_override_ != NULL) {
    visitor(class_loader_override_, arg);
  }
  jni_env_->locals.VisitRoots(visitor, arg);
  jni_env_->monitors.VisitRoots(visitor, arg);

  SirtVisitRoots(visitor, arg);

  // Visit roots on this thread's stack
  Context* context = GetLongJumpContext();
  RootCallbackVisitor visitorToCallback(visitor, arg);
  ReferenceMapVisitor<RootCallbackVisitor> mapper(this, context, visitorToCallback);
  mapper.WalkStack();
  ReleaseLongJumpContext(context);

  for (const instrumentation::InstrumentationStackFrame& frame : *GetInstrumentationStack()) {
    mirror::Object* this_object = frame.this_object_;
    if (this_object != NULL) {
      visitor(this_object, arg);
    }
    mirror::ArtMethod* method = frame.method_;
    visitor(method, arg);
  }
}

static void VerifyObject(const mirror::Object* root, void* arg) {
  gc::Heap* heap = reinterpret_cast<gc::Heap*>(arg);
  heap->VerifyObject(root);
}

void Thread::VerifyStackImpl() {
  UniquePtr<Context> context(Context::Create());
  RootCallbackVisitor visitorToCallback(VerifyObject, Runtime::Current()->GetHeap());
  ReferenceMapVisitor<RootCallbackVisitor> mapper(this, context.get(), visitorToCallback);
  mapper.WalkStack();
}

// Set the stack end to that to be used during a stack overflow
void Thread::SetStackEndForStackOverflow() {
  // During stack overflow we allow use of the full stack.
  if (stack_end_ == stack_begin_) {
    // However, we seem to have already extended to use the full stack.
    LOG(ERROR) << "Need to increase kStackOverflowReservedBytes (currently "
               << kStackOverflowReservedBytes << ")?";
    DumpStack(LOG(ERROR));
    LOG(FATAL) << "Recursive stack overflow.";
  }

  stack_end_ = stack_begin_;
}

std::ostream& operator<<(std::ostream& os, const Thread& thread) {
  thread.ShortDump(os);
  return os;
}

}  // namespace art
