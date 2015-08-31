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

#include "class_linker.h"
#include "common_test.h"
#include "dex_file.h"
#include "gtest/gtest.h"
#include "indirect_reference_table.h"
#include "jni_internal.h"
#include "mem_map.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "mirror/stack_trace_element.h"
#include "runtime.h"
#include "ScopedLocalRef.h"
#include "scoped_thread_state_change.h"
#include "thread.h"
#include "UniquePtr.h"

extern "C" JNIEXPORT jint JNICALL Java_MyClassNatives_bar(JNIEnv*, jobject, jint count) {
  return count + 1;
}

extern "C" JNIEXPORT jint JNICALL Java_MyClassNatives_sbar(JNIEnv*, jclass, jint count) {
  return count + 1;
}

namespace art {

class JniCompilerTest : public CommonTest {
 protected:
  void CompileForTest(jobject class_loader, bool direct,
                      const char* method_name, const char* method_sig) {
    ScopedObjectAccess soa(Thread::Current());
    // Compile the native method before starting the runtime
    mirror::Class* c = class_linker_->FindClass("LMyClassNatives;",
                                                soa.Decode<mirror::ClassLoader*>(class_loader));
    mirror::ArtMethod* method;
    if (direct) {
      method = c->FindDirectMethod(method_name, method_sig);
    } else {
      method = c->FindVirtualMethod(method_name, method_sig);
    }
    ASSERT_TRUE(method != NULL) << method_name << " " << method_sig;
    if (method->GetEntryPointFromCompiledCode() != NULL) {
      return;
    }
    CompileMethod(method);
    ASSERT_TRUE(method->GetEntryPointFromCompiledCode() != NULL) << method_name << " " << method_sig;
  }

  void SetUpForTest(bool direct, const char* method_name, const char* method_sig,
                    void* native_fnptr) {
    // Initialize class loader and compile method when runtime not started.
    if (!runtime_->IsStarted()) {
      {
        ScopedObjectAccess soa(Thread::Current());
        class_loader_ = LoadDex("MyClassNatives");
      }
      CompileForTest(class_loader_, direct, method_name, method_sig);
      // Start runtime.
      Thread::Current()->TransitionFromSuspendedToRunnable();
      bool started = runtime_->Start();
      CHECK(started);
    }
    // JNI operations after runtime start.
    env_ = Thread::Current()->GetJniEnv();
    jklass_ = env_->FindClass("MyClassNatives");
    ASSERT_TRUE(jklass_ != NULL) << method_name << " " << method_sig;

    if (direct) {
      jmethod_ = env_->GetStaticMethodID(jklass_, method_name, method_sig);
    } else {
      jmethod_ = env_->GetMethodID(jklass_, method_name, method_sig);
    }
    ASSERT_TRUE(jmethod_ != NULL) << method_name << " " << method_sig;

    if (native_fnptr != NULL) {
      JNINativeMethod methods[] = { { method_name, method_sig, native_fnptr } };
      ASSERT_EQ(JNI_OK, env_->RegisterNatives(jklass_, methods, 1))
              << method_name << " " << method_sig;
    } else {
      env_->UnregisterNatives(jklass_);
    }

    jmethodID constructor = env_->GetMethodID(jklass_, "<init>", "()V");
    jobj_ = env_->NewObject(jklass_, constructor);
    ASSERT_TRUE(jobj_ != NULL) << method_name << " " << method_sig;
  }

 public:
  static jclass jklass_;
  static jobject jobj_;
  static jobject class_loader_;


 protected:
  JNIEnv* env_;
  jmethodID jmethod_;
};

jclass JniCompilerTest::jklass_;
jobject JniCompilerTest::jobj_;
jobject JniCompilerTest::class_loader_;

int gJava_MyClassNatives_foo_calls = 0;
void Java_MyClassNatives_foo(JNIEnv* env, jobject thisObj) {
  // 1 = thisObj
  EXPECT_EQ(1U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  Locks::mutator_lock_->AssertNotHeld(Thread::Current());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(thisObj != NULL);
  EXPECT_TRUE(env->IsInstanceOf(thisObj, JniCompilerTest::jklass_));
  gJava_MyClassNatives_foo_calls++;
}

TEST_F(JniCompilerTest, CompileAndRunNoArgMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "foo", "()V",
               reinterpret_cast<void*>(&Java_MyClassNatives_foo));

  EXPECT_EQ(0, gJava_MyClassNatives_foo_calls);
  env_->CallNonvirtualVoidMethod(jobj_, jklass_, jmethod_);
  EXPECT_EQ(1, gJava_MyClassNatives_foo_calls);
  env_->CallNonvirtualVoidMethod(jobj_, jklass_, jmethod_);
  EXPECT_EQ(2, gJava_MyClassNatives_foo_calls);
}

TEST_F(JniCompilerTest, CompileAndRunIntMethodThroughStub) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "bar", "(I)I",
               NULL /* calling through stub will link with &Java_MyClassNatives_bar */);

  ScopedObjectAccess soa(Thread::Current());
  std::string reason;
  ASSERT_TRUE(
      Runtime::Current()->GetJavaVM()->LoadNativeLibrary("", soa.Decode<mirror::ClassLoader*>(class_loader_),
                                                         reason)) << reason;

  jint result = env_->CallNonvirtualIntMethod(jobj_, jklass_, jmethod_, 24);
  EXPECT_EQ(25, result);
}

TEST_F(JniCompilerTest, CompileAndRunStaticIntMethodThroughStub) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "sbar", "(I)I",
               NULL /* calling through stub will link with &Java_MyClassNatives_sbar */);

  ScopedObjectAccess soa(Thread::Current());
  std::string reason;
  ASSERT_TRUE(
      Runtime::Current()->GetJavaVM()->LoadNativeLibrary("", soa.Decode<mirror::ClassLoader*>(class_loader_),
                                                         reason)) << reason;

  jint result = env_->CallStaticIntMethod(jklass_, jmethod_, 42);
  EXPECT_EQ(43, result);
}

int gJava_MyClassNatives_fooI_calls = 0;
jint Java_MyClassNatives_fooI(JNIEnv* env, jobject thisObj, jint x) {
  // 1 = thisObj
  EXPECT_EQ(1U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(thisObj != NULL);
  EXPECT_TRUE(env->IsInstanceOf(thisObj, JniCompilerTest::jklass_));
  gJava_MyClassNatives_fooI_calls++;
  return x;
}

TEST_F(JniCompilerTest, CompileAndRunIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooI", "(I)I",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooI));

  EXPECT_EQ(0, gJava_MyClassNatives_fooI_calls);
  jint result = env_->CallNonvirtualIntMethod(jobj_, jklass_, jmethod_, 42);
  EXPECT_EQ(42, result);
  EXPECT_EQ(1, gJava_MyClassNatives_fooI_calls);
  result = env_->CallNonvirtualIntMethod(jobj_, jklass_, jmethod_, 0xCAFED00D);
  EXPECT_EQ(static_cast<jint>(0xCAFED00D), result);
  EXPECT_EQ(2, gJava_MyClassNatives_fooI_calls);
}

int gJava_MyClassNatives_fooII_calls = 0;
jint Java_MyClassNatives_fooII(JNIEnv* env, jobject thisObj, jint x, jint y) {
  // 1 = thisObj
  EXPECT_EQ(1U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(thisObj != NULL);
  EXPECT_TRUE(env->IsInstanceOf(thisObj, JniCompilerTest::jklass_));
  gJava_MyClassNatives_fooII_calls++;
  return x - y;  // non-commutative operator
}

TEST_F(JniCompilerTest, CompileAndRunIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooII", "(II)I",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooII));

  EXPECT_EQ(0, gJava_MyClassNatives_fooII_calls);
  jint result = env_->CallNonvirtualIntMethod(jobj_, jklass_, jmethod_, 99, 10);
  EXPECT_EQ(99 - 10, result);
  EXPECT_EQ(1, gJava_MyClassNatives_fooII_calls);
  result = env_->CallNonvirtualIntMethod(jobj_, jklass_, jmethod_, 0xCAFEBABE,
                                         0xCAFED00D);
  EXPECT_EQ(static_cast<jint>(0xCAFEBABE - 0xCAFED00D), result);
  EXPECT_EQ(2, gJava_MyClassNatives_fooII_calls);
}

int gJava_MyClassNatives_fooJJ_calls = 0;
jlong Java_MyClassNatives_fooJJ(JNIEnv* env, jobject thisObj, jlong x, jlong y) {
  // 1 = thisObj
  EXPECT_EQ(1U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(thisObj != NULL);
  EXPECT_TRUE(env->IsInstanceOf(thisObj, JniCompilerTest::jklass_));
  gJava_MyClassNatives_fooJJ_calls++;
  return x - y;  // non-commutative operator
}

TEST_F(JniCompilerTest, CompileAndRunLongLongMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooJJ", "(JJ)J",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooJJ));

  EXPECT_EQ(0, gJava_MyClassNatives_fooJJ_calls);
  jlong a = 0x1234567890ABCDEFll;
  jlong b = 0xFEDCBA0987654321ll;
  jlong result = env_->CallNonvirtualLongMethod(jobj_, jklass_, jmethod_, a, b);
  EXPECT_EQ(a - b, result);
  EXPECT_EQ(1, gJava_MyClassNatives_fooJJ_calls);
  result = env_->CallNonvirtualLongMethod(jobj_, jklass_, jmethod_, b, a);
  EXPECT_EQ(b - a, result);
  EXPECT_EQ(2, gJava_MyClassNatives_fooJJ_calls);
}

int gJava_MyClassNatives_fooDD_calls = 0;
jdouble Java_MyClassNatives_fooDD(JNIEnv* env, jobject thisObj, jdouble x, jdouble y) {
  // 1 = thisObj
  EXPECT_EQ(1U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(thisObj != NULL);
  EXPECT_TRUE(env->IsInstanceOf(thisObj, JniCompilerTest::jklass_));
  gJava_MyClassNatives_fooDD_calls++;
  return x - y;  // non-commutative operator
}

TEST_F(JniCompilerTest, CompileAndRunDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooDD", "(DD)D",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooDD));

  EXPECT_EQ(0, gJava_MyClassNatives_fooDD_calls);
  jdouble result = env_->CallNonvirtualDoubleMethod(jobj_, jklass_, jmethod_,
                                                    99.0, 10.0);
  EXPECT_EQ(99.0 - 10.0, result);
  EXPECT_EQ(1, gJava_MyClassNatives_fooDD_calls);
  jdouble a = 3.14159265358979323846;
  jdouble b = 0.69314718055994530942;
  result = env_->CallNonvirtualDoubleMethod(jobj_, jklass_, jmethod_, a, b);
  EXPECT_EQ(a - b, result);
  EXPECT_EQ(2, gJava_MyClassNatives_fooDD_calls);
}

int gJava_MyClassNatives_fooJJ_synchronized_calls = 0;
jlong Java_MyClassNatives_fooJJ_synchronized(JNIEnv* env, jobject thisObj, jlong x, jlong y) {
  // 1 = thisObj
  EXPECT_EQ(1U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(thisObj != NULL);
  EXPECT_TRUE(env->IsInstanceOf(thisObj, JniCompilerTest::jklass_));
  gJava_MyClassNatives_fooJJ_synchronized_calls++;
  return x | y;
}

TEST_F(JniCompilerTest, CompileAndRun_fooJJ_synchronized) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooJJ_synchronized", "(JJ)J",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooJJ_synchronized));

  EXPECT_EQ(0, gJava_MyClassNatives_fooJJ_synchronized_calls);
  jlong a = 0x1000000020000000ULL;
  jlong b = 0x00ff000000aa0000ULL;
  jlong result = env_->CallNonvirtualLongMethod(jobj_, jklass_, jmethod_, a, b);
  EXPECT_EQ(a | b, result);
  EXPECT_EQ(1, gJava_MyClassNatives_fooJJ_synchronized_calls);
}

int gJava_MyClassNatives_fooIOO_calls = 0;
jobject Java_MyClassNatives_fooIOO(JNIEnv* env, jobject thisObj, jint x, jobject y,
                            jobject z) {
  // 3 = this + y + z
  EXPECT_EQ(3U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(thisObj != NULL);
  EXPECT_TRUE(env->IsInstanceOf(thisObj, JniCompilerTest::jklass_));
  gJava_MyClassNatives_fooIOO_calls++;
  switch (x) {
    case 1:
      return y;
    case 2:
      return z;
    default:
      return thisObj;
  }
}

TEST_F(JniCompilerTest, CompileAndRunIntObjectObjectMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooIOO",
               "(ILjava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooIOO));

  EXPECT_EQ(0, gJava_MyClassNatives_fooIOO_calls);
  jobject result = env_->CallNonvirtualObjectMethod(jobj_, jklass_, jmethod_, 0, NULL, NULL);
  EXPECT_TRUE(env_->IsSameObject(jobj_, result));
  EXPECT_EQ(1, gJava_MyClassNatives_fooIOO_calls);

  result = env_->CallNonvirtualObjectMethod(jobj_, jklass_, jmethod_, 0, NULL, jklass_);
  EXPECT_TRUE(env_->IsSameObject(jobj_, result));
  EXPECT_EQ(2, gJava_MyClassNatives_fooIOO_calls);
  result = env_->CallNonvirtualObjectMethod(jobj_, jklass_, jmethod_, 1, NULL, jklass_);
  EXPECT_TRUE(env_->IsSameObject(NULL, result));
  EXPECT_EQ(3, gJava_MyClassNatives_fooIOO_calls);
  result = env_->CallNonvirtualObjectMethod(jobj_, jklass_, jmethod_, 2, NULL, jklass_);
  EXPECT_TRUE(env_->IsSameObject(jklass_, result));
  EXPECT_EQ(4, gJava_MyClassNatives_fooIOO_calls);

  result = env_->CallNonvirtualObjectMethod(jobj_, jklass_, jmethod_, 0, jklass_, NULL);
  EXPECT_TRUE(env_->IsSameObject(jobj_, result));
  EXPECT_EQ(5, gJava_MyClassNatives_fooIOO_calls);
  result = env_->CallNonvirtualObjectMethod(jobj_, jklass_, jmethod_, 1, jklass_, NULL);
  EXPECT_TRUE(env_->IsSameObject(jklass_, result));
  EXPECT_EQ(6, gJava_MyClassNatives_fooIOO_calls);
  result = env_->CallNonvirtualObjectMethod(jobj_, jklass_, jmethod_, 2, jklass_, NULL);
  EXPECT_TRUE(env_->IsSameObject(NULL, result));
  EXPECT_EQ(7, gJava_MyClassNatives_fooIOO_calls);
}

int gJava_MyClassNatives_fooSII_calls = 0;
jint Java_MyClassNatives_fooSII(JNIEnv* env, jclass klass, jint x, jint y) {
  // 1 = klass
  EXPECT_EQ(1U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(klass != NULL);
  EXPECT_TRUE(env->IsInstanceOf(JniCompilerTest::jobj_, klass));
  gJava_MyClassNatives_fooSII_calls++;
  return x + y;
}

TEST_F(JniCompilerTest, CompileAndRunStaticIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "fooSII", "(II)I",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooSII));

  EXPECT_EQ(0, gJava_MyClassNatives_fooSII_calls);
  jint result = env_->CallStaticIntMethod(jklass_, jmethod_, 20, 30);
  EXPECT_EQ(50, result);
  EXPECT_EQ(1, gJava_MyClassNatives_fooSII_calls);
}

int gJava_MyClassNatives_fooSDD_calls = 0;
jdouble Java_MyClassNatives_fooSDD(JNIEnv* env, jclass klass, jdouble x, jdouble y) {
  // 1 = klass
  EXPECT_EQ(1U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(klass != NULL);
  EXPECT_TRUE(env->IsInstanceOf(JniCompilerTest::jobj_, klass));
  gJava_MyClassNatives_fooSDD_calls++;
  return x - y;  // non-commutative operator
}

TEST_F(JniCompilerTest, CompileAndRunStaticDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "fooSDD", "(DD)D",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooSDD));

  EXPECT_EQ(0, gJava_MyClassNatives_fooSDD_calls);
  jdouble result = env_->CallStaticDoubleMethod(jklass_, jmethod_, 99.0, 10.0);
  EXPECT_EQ(99.0 - 10.0, result);
  EXPECT_EQ(1, gJava_MyClassNatives_fooSDD_calls);
  jdouble a = 3.14159265358979323846;
  jdouble b = 0.69314718055994530942;
  result = env_->CallStaticDoubleMethod(jklass_, jmethod_, a, b);
  EXPECT_EQ(a - b, result);
  EXPECT_EQ(2, gJava_MyClassNatives_fooSDD_calls);
}

int gJava_MyClassNatives_fooSIOO_calls = 0;
jobject Java_MyClassNatives_fooSIOO(JNIEnv* env, jclass klass, jint x, jobject y,
                             jobject z) {
  // 3 = klass + y + z
  EXPECT_EQ(3U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(klass != NULL);
  EXPECT_TRUE(env->IsInstanceOf(JniCompilerTest::jobj_, klass));
  gJava_MyClassNatives_fooSIOO_calls++;
  switch (x) {
    case 1:
      return y;
    case 2:
      return z;
    default:
      return klass;
  }
}


TEST_F(JniCompilerTest, CompileAndRunStaticIntObjectObjectMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "fooSIOO",
               "(ILjava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooSIOO));

  EXPECT_EQ(0, gJava_MyClassNatives_fooSIOO_calls);
  jobject result = env_->CallStaticObjectMethod(jklass_, jmethod_, 0, NULL, NULL);
  EXPECT_TRUE(env_->IsSameObject(jklass_, result));
  EXPECT_EQ(1, gJava_MyClassNatives_fooSIOO_calls);

  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 0, NULL, jobj_);
  EXPECT_TRUE(env_->IsSameObject(jklass_, result));
  EXPECT_EQ(2, gJava_MyClassNatives_fooSIOO_calls);
  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 1, NULL, jobj_);
  EXPECT_TRUE(env_->IsSameObject(NULL, result));
  EXPECT_EQ(3, gJava_MyClassNatives_fooSIOO_calls);
  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 2, NULL, jobj_);
  EXPECT_TRUE(env_->IsSameObject(jobj_, result));
  EXPECT_EQ(4, gJava_MyClassNatives_fooSIOO_calls);

  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 0, jobj_, NULL);
  EXPECT_TRUE(env_->IsSameObject(jklass_, result));
  EXPECT_EQ(5, gJava_MyClassNatives_fooSIOO_calls);
  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 1, jobj_, NULL);
  EXPECT_TRUE(env_->IsSameObject(jobj_, result));
  EXPECT_EQ(6, gJava_MyClassNatives_fooSIOO_calls);
  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 2, jobj_, NULL);
  EXPECT_TRUE(env_->IsSameObject(NULL, result));
  EXPECT_EQ(7, gJava_MyClassNatives_fooSIOO_calls);
}

int gJava_MyClassNatives_fooSSIOO_calls = 0;
jobject Java_MyClassNatives_fooSSIOO(JNIEnv* env, jclass klass, jint x, jobject y, jobject z) {
  // 3 = klass + y + z
  EXPECT_EQ(3U, Thread::Current()->NumStackReferences());
  EXPECT_EQ(kNative, Thread::Current()->GetState());
  EXPECT_EQ(Thread::Current()->GetJniEnv(), env);
  EXPECT_TRUE(klass != NULL);
  EXPECT_TRUE(env->IsInstanceOf(JniCompilerTest::jobj_, klass));
  gJava_MyClassNatives_fooSSIOO_calls++;
  switch (x) {
    case 1:
      return y;
    case 2:
      return z;
    default:
      return klass;
  }
}

TEST_F(JniCompilerTest, CompileAndRunStaticSynchronizedIntObjectObjectMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "fooSSIOO",
               "(ILjava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooSSIOO));

  EXPECT_EQ(0, gJava_MyClassNatives_fooSSIOO_calls);
  jobject result = env_->CallStaticObjectMethod(jklass_, jmethod_, 0, NULL, NULL);
  EXPECT_TRUE(env_->IsSameObject(jklass_, result));
  EXPECT_EQ(1, gJava_MyClassNatives_fooSSIOO_calls);

  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 0, NULL, jobj_);
  EXPECT_TRUE(env_->IsSameObject(jklass_, result));
  EXPECT_EQ(2, gJava_MyClassNatives_fooSSIOO_calls);
  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 1, NULL, jobj_);
  EXPECT_TRUE(env_->IsSameObject(NULL, result));
  EXPECT_EQ(3, gJava_MyClassNatives_fooSSIOO_calls);
  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 2, NULL, jobj_);
  EXPECT_TRUE(env_->IsSameObject(jobj_, result));
  EXPECT_EQ(4, gJava_MyClassNatives_fooSSIOO_calls);

  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 0, jobj_, NULL);
  EXPECT_TRUE(env_->IsSameObject(jklass_, result));
  EXPECT_EQ(5, gJava_MyClassNatives_fooSSIOO_calls);
  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 1, jobj_, NULL);
  EXPECT_TRUE(env_->IsSameObject(jobj_, result));
  EXPECT_EQ(6, gJava_MyClassNatives_fooSSIOO_calls);
  result = env_->CallStaticObjectMethod(jklass_, jmethod_, 2, jobj_, NULL);
  EXPECT_TRUE(env_->IsSameObject(NULL, result));
  EXPECT_EQ(7, gJava_MyClassNatives_fooSSIOO_calls);
}

void Java_MyClassNatives_throwException(JNIEnv* env, jobject) {
  jclass c = env->FindClass("java/lang/RuntimeException");
  env->ThrowNew(c, "hello");
}

TEST_F(JniCompilerTest, ExceptionHandling) {
  TEST_DISABLED_FOR_PORTABLE();
  {
    ASSERT_FALSE(runtime_->IsStarted());
    ScopedObjectAccess soa(Thread::Current());
    class_loader_ = LoadDex("MyClassNatives");

    // all compilation needs to happen before Runtime::Start
    CompileForTest(class_loader_, false, "foo", "()V");
    CompileForTest(class_loader_, false, "throwException", "()V");
    CompileForTest(class_loader_, false, "foo", "()V");
  }
  // Start runtime to avoid re-initialization in SetupForTest.
  Thread::Current()->TransitionFromSuspendedToRunnable();
  bool started = runtime_->Start();
  CHECK(started);

  gJava_MyClassNatives_foo_calls = 0;

  // Check a single call of a JNI method is ok
  SetUpForTest(false, "foo", "()V", reinterpret_cast<void*>(&Java_MyClassNatives_foo));
  env_->CallNonvirtualVoidMethod(jobj_, jklass_, jmethod_);
  EXPECT_EQ(1, gJava_MyClassNatives_foo_calls);
  EXPECT_FALSE(Thread::Current()->IsExceptionPending());

  // Get class for exception we expect to be thrown
  ScopedLocalRef<jclass> jlre(env_, env_->FindClass("java/lang/RuntimeException"));
  SetUpForTest(false, "throwException", "()V",
               reinterpret_cast<void*>(&Java_MyClassNatives_throwException));
  // Call Java_MyClassNatives_throwException (JNI method that throws exception)
  env_->CallNonvirtualVoidMethod(jobj_, jklass_, jmethod_);
  EXPECT_EQ(1, gJava_MyClassNatives_foo_calls);
  EXPECT_TRUE(env_->ExceptionCheck() == JNI_TRUE);
  ScopedLocalRef<jthrowable> exception(env_, env_->ExceptionOccurred());
  env_->ExceptionClear();
  EXPECT_TRUE(env_->IsInstanceOf(exception.get(), jlre.get()));

  // Check a single call of a JNI method is ok
  SetUpForTest(false, "foo", "()V", reinterpret_cast<void*>(&Java_MyClassNatives_foo));
  env_->CallNonvirtualVoidMethod(jobj_, jklass_, jmethod_);
  EXPECT_EQ(2, gJava_MyClassNatives_foo_calls);
}

jint Java_MyClassNatives_nativeUpCall(JNIEnv* env, jobject thisObj, jint i) {
  if (i <= 0) {
    // We want to check raw Object*/Array* below
    ScopedObjectAccess soa(env);

    // Build stack trace
    jobject internal = Thread::Current()->CreateInternalStackTrace(soa);
    jobjectArray ste_array = Thread::InternalStackTraceToStackTraceElementArray(env, internal);
    mirror::ObjectArray<mirror::StackTraceElement>* trace_array =
        soa.Decode<mirror::ObjectArray<mirror::StackTraceElement>*>(ste_array);
    EXPECT_TRUE(trace_array != NULL);
    EXPECT_EQ(11, trace_array->GetLength());

    // Check stack trace entries have expected values
    for (int32_t i = 0; i < trace_array->GetLength(); ++i) {
      EXPECT_EQ(-2, trace_array->Get(i)->GetLineNumber());
      mirror::StackTraceElement* ste = trace_array->Get(i);
      EXPECT_STREQ("MyClassNatives.java", ste->GetFileName()->ToModifiedUtf8().c_str());
      EXPECT_STREQ("MyClassNatives", ste->GetDeclaringClass()->ToModifiedUtf8().c_str());
      EXPECT_STREQ("fooI", ste->GetMethodName()->ToModifiedUtf8().c_str());
    }

    // end recursion
    return 0;
  } else {
    jclass jklass = env->FindClass("MyClassNatives");
    EXPECT_TRUE(jklass != NULL);
    jmethodID jmethod = env->GetMethodID(jklass, "fooI", "(I)I");
    EXPECT_TRUE(jmethod != NULL);

    // Recurse with i - 1
    jint result = env->CallNonvirtualIntMethod(thisObj, jklass, jmethod, i - 1);

    // Return sum of all depths
    return i + result;
  }
}

TEST_F(JniCompilerTest, NativeStackTraceElement) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooI", "(I)I",
               reinterpret_cast<void*>(&Java_MyClassNatives_nativeUpCall));
  jint result = env_->CallNonvirtualIntMethod(jobj_, jklass_, jmethod_, 10);
  EXPECT_EQ(10+9+8+7+6+5+4+3+2+1, result);
}

jobject Java_MyClassNatives_fooO(JNIEnv* env, jobject, jobject x) {
  return env->NewGlobalRef(x);
}

TEST_F(JniCompilerTest, ReturnGlobalRef) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooO", "(Ljava/lang/Object;)Ljava/lang/Object;",
               reinterpret_cast<void*>(&Java_MyClassNatives_fooO));
  jobject result = env_->CallNonvirtualObjectMethod(jobj_, jklass_, jmethod_, jobj_);
  EXPECT_EQ(JNILocalRefType, env_->GetObjectRefType(result));
  EXPECT_TRUE(env_->IsSameObject(result, jobj_));
}

jint local_ref_test(JNIEnv* env, jobject thisObj, jint x) {
  // Add 10 local references
  ScopedObjectAccess soa(env);
  for (int i = 0; i < 10; i++) {
    soa.AddLocalReference<jobject>(soa.Decode<mirror::Object*>(thisObj));
  }
  return x+1;
}

TEST_F(JniCompilerTest, LocalReferenceTableClearingTest) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "fooI", "(I)I", reinterpret_cast<void*>(&local_ref_test));
  // 1000 invocations of a method that adds 10 local references
  for (int i = 0; i < 1000; i++) {
    jint result = env_->CallIntMethod(jobj_, jmethod_, i);
    EXPECT_TRUE(result == i + 1);
  }
}

void my_arraycopy(JNIEnv* env, jclass klass, jobject src, jint src_pos, jobject dst, jint dst_pos, jint length) {
  EXPECT_TRUE(env->IsSameObject(JniCompilerTest::jklass_, klass));
  EXPECT_TRUE(env->IsSameObject(JniCompilerTest::jklass_, dst));
  EXPECT_TRUE(env->IsSameObject(JniCompilerTest::jobj_, src));
  EXPECT_EQ(1234, src_pos);
  EXPECT_EQ(5678, dst_pos);
  EXPECT_EQ(9876, length);
}

TEST_F(JniCompilerTest, JavaLangSystemArrayCopy) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V",
               reinterpret_cast<void*>(&my_arraycopy));
  env_->CallStaticVoidMethod(jklass_, jmethod_, jobj_, 1234, jklass_, 5678, 9876);
}

jboolean my_casi(JNIEnv* env, jobject unsafe, jobject obj, jlong offset, jint expected, jint newval) {
  EXPECT_TRUE(env->IsSameObject(JniCompilerTest::jobj_, unsafe));
  EXPECT_TRUE(env->IsSameObject(JniCompilerTest::jobj_, obj));
  EXPECT_EQ(0x12345678ABCDEF88ll, offset);
  EXPECT_EQ(static_cast<jint>(0xCAFEF00D), expected);
  EXPECT_EQ(static_cast<jint>(0xEBADF00D), newval);
  return JNI_TRUE;
}

TEST_F(JniCompilerTest, CompareAndSwapInt) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "compareAndSwapInt", "(Ljava/lang/Object;JII)Z",
               reinterpret_cast<void*>(&my_casi));
  jboolean result = env_->CallBooleanMethod(jobj_, jmethod_, jobj_, 0x12345678ABCDEF88ll, 0xCAFEF00D, 0xEBADF00D);
  EXPECT_EQ(result, JNI_TRUE);
}

jint my_gettext(JNIEnv* env, jclass klass, jlong val1, jobject obj1, jlong val2, jobject obj2) {
  EXPECT_TRUE(env->IsInstanceOf(JniCompilerTest::jobj_, klass));
  EXPECT_TRUE(env->IsSameObject(JniCompilerTest::jobj_, obj1));
  EXPECT_TRUE(env->IsSameObject(JniCompilerTest::jobj_, obj2));
  EXPECT_EQ(0x12345678ABCDEF88ll, val1);
  EXPECT_EQ(0x7FEDCBA987654321ll, val2);
  return 42;
}

TEST_F(JniCompilerTest, GetText) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "getText", "(JLjava/lang/Object;JLjava/lang/Object;)I",
               reinterpret_cast<void*>(&my_gettext));
  jint result = env_->CallStaticIntMethod(jklass_, jmethod_, 0x12345678ABCDEF88ll, jobj_,
                                          0x7FEDCBA987654321ll, jobj_);
  EXPECT_EQ(result, 42);
}

TEST_F(JniCompilerTest, GetSinkPropertiesNative) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "getSinkPropertiesNative", "(Ljava/lang/String;)[Ljava/lang/Object;", NULL);
  // This space intentionally left blank. Just testing compilation succeeds.
}

// This should return jclass, but we're imitating a bug pattern.
jobject Java_MyClassNatives_instanceMethodThatShouldReturnClass(JNIEnv* env, jobject) {
  return env->NewStringUTF("not a class!");
}

// This should return jclass, but we're imitating a bug pattern.
jobject Java_MyClassNatives_staticMethodThatShouldReturnClass(JNIEnv* env, jclass) {
  return env->NewStringUTF("not a class!");
}

TEST_F(JniCompilerTest, UpcallReturnTypeChecking_Instance) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "instanceMethodThatShouldReturnClass", "()Ljava/lang/Class;",
               reinterpret_cast<void*>(&Java_MyClassNatives_instanceMethodThatShouldReturnClass));

  CheckJniAbortCatcher check_jni_abort_catcher;
  // TODO: check type of returns with portable JNI compiler.
  // This native method is bad, and tries to return a jstring as a jclass.
  env_->CallObjectMethod(jobj_, jmethod_);
  check_jni_abort_catcher.Check("attempt to return an instance of java.lang.String from java.lang.Class MyClassNatives.instanceMethodThatShouldReturnClass()");

  // Here, we just call the method incorrectly; we should catch that too.
  env_->CallVoidMethod(jobj_, jmethod_);
  check_jni_abort_catcher.Check("attempt to return an instance of java.lang.String from java.lang.Class MyClassNatives.instanceMethodThatShouldReturnClass()");
  env_->CallStaticVoidMethod(jklass_, jmethod_);
  check_jni_abort_catcher.Check("calling non-static method java.lang.Class MyClassNatives.instanceMethodThatShouldReturnClass() with CallStaticVoidMethodV");
}

TEST_F(JniCompilerTest, UpcallReturnTypeChecking_Static) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "staticMethodThatShouldReturnClass", "()Ljava/lang/Class;",
               reinterpret_cast<void*>(&Java_MyClassNatives_staticMethodThatShouldReturnClass));

  CheckJniAbortCatcher check_jni_abort_catcher;
  // TODO: check type of returns with portable JNI compiler.
  // This native method is bad, and tries to return a jstring as a jclass.
  env_->CallStaticObjectMethod(jklass_, jmethod_);
  check_jni_abort_catcher.Check("attempt to return an instance of java.lang.String from java.lang.Class MyClassNatives.staticMethodThatShouldReturnClass()");

  // Here, we just call the method incorrectly; we should catch that too.
  env_->CallStaticVoidMethod(jklass_, jmethod_);
  check_jni_abort_catcher.Check("attempt to return an instance of java.lang.String from java.lang.Class MyClassNatives.staticMethodThatShouldReturnClass()");
  env_->CallVoidMethod(jobj_, jmethod_);
  check_jni_abort_catcher.Check("calling static method java.lang.Class MyClassNatives.staticMethodThatShouldReturnClass() with CallVoidMethodV");
}

// This should take jclass, but we're imitating a bug pattern.
void Java_MyClassNatives_instanceMethodThatShouldTakeClass(JNIEnv*, jobject, jclass) {
}

// This should take jclass, but we're imitating a bug pattern.
void Java_MyClassNatives_staticMethodThatShouldTakeClass(JNIEnv*, jclass, jclass) {
}

TEST_F(JniCompilerTest, UpcallArgumentTypeChecking_Instance) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(false, "instanceMethodThatShouldTakeClass", "(ILjava/lang/Class;)V",
               reinterpret_cast<void*>(&Java_MyClassNatives_instanceMethodThatShouldTakeClass));

  CheckJniAbortCatcher check_jni_abort_catcher;
  // We deliberately pass a bad second argument here.
  env_->CallVoidMethod(jobj_, jmethod_, 123, env_->NewStringUTF("not a class!"));
  check_jni_abort_catcher.Check("bad arguments passed to void MyClassNatives.instanceMethodThatShouldTakeClass(int, java.lang.Class)");
}

TEST_F(JniCompilerTest, UpcallArgumentTypeChecking_Static) {
  TEST_DISABLED_FOR_PORTABLE();
  SetUpForTest(true, "staticMethodThatShouldTakeClass", "(ILjava/lang/Class;)V",
               reinterpret_cast<void*>(&Java_MyClassNatives_staticMethodThatShouldTakeClass));

  CheckJniAbortCatcher check_jni_abort_catcher;
  // We deliberately pass a bad second argument here.
  env_->CallStaticVoidMethod(jklass_, jmethod_, 123, env_->NewStringUTF("not a class!"));
  check_jni_abort_catcher.Check("bad arguments passed to void MyClassNatives.staticMethodThatShouldTakeClass(int, java.lang.Class)");
}

}  // namespace art
