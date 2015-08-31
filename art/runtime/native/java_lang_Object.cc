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

#include "jni_internal.h"
#include "mirror/object-inl.h"
#include "scoped_thread_state_change.h"

// TODO: better support for overloading.
#undef NATIVE_METHOD
#define NATIVE_METHOD(className, functionName, signature, identifier) \
    { #functionName, signature, reinterpret_cast<void*>(className ## _ ## identifier) }

namespace art {

static jobject Object_internalClone(JNIEnv* env, jobject java_this) {
  ScopedObjectAccess soa(env);
  mirror::Object* o = soa.Decode<mirror::Object*>(java_this);
  return soa.AddLocalReference<jobject>(o->Clone(soa.Self()));
}

static void Object_notify(JNIEnv* env, jobject java_this) {
  ScopedObjectAccess soa(env);
  mirror::Object* o = soa.Decode<mirror::Object*>(java_this);
  o->Notify(soa.Self());
}

static void Object_notifyAll(JNIEnv* env, jobject java_this) {
  ScopedObjectAccess soa(env);
  mirror::Object* o = soa.Decode<mirror::Object*>(java_this);
  o->NotifyAll(soa.Self());
}

static void Object_wait(JNIEnv* env, jobject java_this) {
  ScopedObjectAccess soa(env);
  mirror::Object* o = soa.Decode<mirror::Object*>(java_this);
  o->Wait(soa.Self());
}

static void Object_waitJI(JNIEnv* env, jobject java_this, jlong ms, jint ns) {
  ScopedObjectAccess soa(env);
  mirror::Object* o = soa.Decode<mirror::Object*>(java_this);
  o->Wait(soa.Self(), ms, ns);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Object, internalClone, "()Ljava/lang/Object;", internalClone),
  NATIVE_METHOD(Object, notify, "()V", notify),
  NATIVE_METHOD(Object, notifyAll, "()V", notifyAll),
  NATIVE_METHOD(Object, wait, "()V", wait),
  NATIVE_METHOD(Object, wait, "(JI)V", waitJI),
};

void register_java_lang_Object(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/Object");
}

}  // namespace art
