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
#include "jni_internal.h"
#include "mirror/string.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"

namespace art {

static jint String_compareTo(JNIEnv* env, jobject javaThis, jobject javaRhs) {
  ScopedObjectAccess soa(env);
  if (UNLIKELY(javaRhs == NULL)) {
    ThrowNullPointerException(NULL, "rhs == null");
    return -1;
  } else {
    return soa.Decode<mirror::String*>(javaThis)->CompareTo(soa.Decode<mirror::String*>(javaRhs));
  }
}

static jint String_fastIndexOf(JNIEnv* env, jobject java_this, jint ch, jint start) {
  ScopedObjectAccess soa(env);
  // This method does not handle supplementary characters. They're dealt with in managed code.
  DCHECK_LE(ch, 0xffff);

  mirror::String* s = soa.Decode<mirror::String*>(java_this);
  return s->FastIndexOf(ch, start);
}

static jstring String_intern(JNIEnv* env, jobject javaThis) {
  ScopedObjectAccess soa(env);
  mirror::String* s = soa.Decode<mirror::String*>(javaThis);
  mirror::String* result = s->Intern();
  return soa.AddLocalReference<jstring>(result);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(String, compareTo, "(Ljava/lang/String;)I"),
  NATIVE_METHOD(String, fastIndexOf, "(II)I"),
  NATIVE_METHOD(String, intern, "()Ljava/lang/String;"),
};

void register_java_lang_String(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/String");
}

}  // namespace art
