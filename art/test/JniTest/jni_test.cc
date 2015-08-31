/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <assert.h>
#include <stdio.h>
#include <pthread.h>

#include "jni.h"

#if defined(NDEBUG)
#error test code compiled without NDEBUG
#endif

static JavaVM* jvm = NULL;

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
  assert(vm != NULL);
  assert(jvm == NULL);
  jvm = vm;
  return JNI_VERSION_1_6;
}

static void* testFindClassOnAttachedNativeThread(void*) {
  assert(jvm != NULL);

  JNIEnv* env = NULL;
  JavaVMAttachArgs args = { JNI_VERSION_1_6, __FUNCTION__, NULL };
  int attach_result = jvm->AttachCurrentThread(&env, &args);
  assert(attach_result == 0);

  jclass clazz = env->FindClass("JniTest");
  assert(clazz != NULL);
  assert(!env->ExceptionCheck());

  jobjectArray array = env->NewObjectArray(0, clazz, NULL);
  assert(array != NULL);
  assert(!env->ExceptionCheck());

  int detach_result = jvm->DetachCurrentThread();
  assert(detach_result == 0);
  return NULL;
}

// http://b/10994325
extern "C" JNIEXPORT void JNICALL Java_JniTest_testFindClassOnAttachedNativeThread(JNIEnv*,
                                                                                   jclass) {
  pthread_t pthread;
  int pthread_create_result = pthread_create(&pthread,
                                             NULL,
                                             testFindClassOnAttachedNativeThread,
                                             NULL);
  assert(pthread_create_result == 0);
  int pthread_join_result = pthread_join(pthread, NULL);
  assert(pthread_join_result == 0);
}

static void* testFindFieldOnAttachedNativeThread(void*) {
  assert(jvm != NULL);

  JNIEnv* env = NULL;
  JavaVMAttachArgs args = { JNI_VERSION_1_6, __FUNCTION__, NULL };
  int attach_result = jvm->AttachCurrentThread(&env, &args);
  assert(attach_result == 0);

  jclass clazz = env->FindClass("JniTest");
  assert(clazz != NULL);
  assert(!env->ExceptionCheck());

  jfieldID field = env->GetStaticFieldID(clazz, "testFindFieldOnAttachedNativeThreadField", "Z");
  assert(field != NULL);
  assert(!env->ExceptionCheck());

  env->SetStaticBooleanField(clazz, field, JNI_TRUE);

  int detach_result = jvm->DetachCurrentThread();
  assert(detach_result == 0);
  return NULL;
}

extern "C" JNIEXPORT void JNICALL Java_JniTest_testFindFieldOnAttachedNativeThreadNative(JNIEnv*,
                                                                                         jclass) {
  pthread_t pthread;
  int pthread_create_result = pthread_create(&pthread,
                                             NULL,
                                             testFindFieldOnAttachedNativeThread,
                                             NULL);
  assert(pthread_create_result == 0);
  int pthread_join_result = pthread_join(pthread, NULL);
  assert(pthread_join_result == 0);
}


// http://b/11243757
extern "C" JNIEXPORT void JNICALL Java_JniTest_testCallStaticVoidMethodOnSubClassNative(JNIEnv* env,
                                                                                        jclass) {
  jclass super_class = env->FindClass("JniTest$testCallStaticVoidMethodOnSubClass_SuperClass");
  assert(super_class != NULL);

  jmethodID execute = env->GetStaticMethodID(super_class, "execute", "()V");
  assert(execute != NULL);

  jclass sub_class = env->FindClass("JniTest$testCallStaticVoidMethodOnSubClass_SubClass");
  assert(sub_class != NULL);

  env->CallStaticVoidMethod(sub_class, execute);
}

extern "C" JNIEXPORT jobject JNICALL Java_JniTest_testGetMirandaMethodNative(JNIEnv* env, jclass) {
  jclass abstract_class = env->FindClass("JniTest$testGetMirandaMethod_MirandaAbstract");
  assert(abstract_class != NULL);
  jmethodID miranda_method = env->GetMethodID(abstract_class, "inInterface", "()Z");
  assert(miranda_method != NULL);
  return env->ToReflectedMethod(abstract_class, miranda_method, JNI_FALSE);
}
