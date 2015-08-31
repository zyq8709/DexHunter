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

#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"
#include "thread-inl.h"

namespace art {

// Called on entry to JNI, transition out of Runnable and release share of mutator_lock_.
extern "C" uint32_t art_portable_jni_method_start(Thread* self)
    UNLOCK_FUNCTION(GlobalSynchronizatio::mutator_lock_) {
  JNIEnvExt* env = self->GetJniEnv();
  uint32_t saved_local_ref_cookie = env->local_ref_cookie;
  env->local_ref_cookie = env->locals.GetSegmentState();
  self->TransitionFromRunnableToSuspended(kNative);
  return saved_local_ref_cookie;
}

extern "C" uint32_t art_portable_jni_method_start_synchronized(jobject to_lock, Thread* self)
    UNLOCK_FUNCTION(Locks::mutator_lock_) {
  self->DecodeJObject(to_lock)->MonitorEnter(self);
  return art_portable_jni_method_start(self);
}

static void PopLocalReferences(uint32_t saved_local_ref_cookie, Thread* self) {
  JNIEnvExt* env = self->GetJniEnv();
  env->locals.SetSegmentState(env->local_ref_cookie);
  env->local_ref_cookie = saved_local_ref_cookie;
}

extern "C" void art_portable_jni_method_end(uint32_t saved_local_ref_cookie, Thread* self)
    SHARED_LOCK_FUNCTION(Locks::mutator_lock_) {
  self->TransitionFromSuspendedToRunnable();
  PopLocalReferences(saved_local_ref_cookie, self);
}


extern "C" void art_portable_jni_method_end_synchronized(uint32_t saved_local_ref_cookie,
                                              jobject locked,
                                              Thread* self)
    SHARED_LOCK_FUNCTION(Locks::mutator_lock_) {
  self->TransitionFromSuspendedToRunnable();
  UnlockJniSynchronizedMethod(locked, self);  // Must decode before pop.
  PopLocalReferences(saved_local_ref_cookie, self);
}

extern "C" mirror::Object* art_portable_jni_method_end_with_reference(jobject result,
                                                                      uint32_t saved_local_ref_cookie,
                                                                      Thread* self)
    SHARED_LOCK_FUNCTION(Locks::mutator_lock_) {
  self->TransitionFromSuspendedToRunnable();
  mirror::Object* o = self->DecodeJObject(result);  // Must decode before pop.
  PopLocalReferences(saved_local_ref_cookie, self);
  // Process result.
  if (UNLIKELY(self->GetJniEnv()->check_jni)) {
    if (self->IsExceptionPending()) {
      return NULL;
    }
    CheckReferenceResult(o, self);
  }
  return o;
}

extern "C" mirror::Object* art_portable_jni_method_end_with_reference_synchronized(jobject result,
                                                                                   uint32_t saved_local_ref_cookie,
                                                                                   jobject locked,
                                                                                   Thread* self)
    SHARED_LOCK_FUNCTION(Locks::mutator_lock_) {
  self->TransitionFromSuspendedToRunnable();
  UnlockJniSynchronizedMethod(locked, self);  // Must decode before pop.
  mirror::Object* o = self->DecodeJObject(result);
  PopLocalReferences(saved_local_ref_cookie, self);
  // Process result.
  if (UNLIKELY(self->GetJniEnv()->check_jni)) {
    if (self->IsExceptionPending()) {
      return NULL;
    }
    CheckReferenceResult(o, self);
  }
  return o;
}

}  // namespace art
