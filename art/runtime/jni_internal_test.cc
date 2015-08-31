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

#include "jni_internal.h"

#include <limits.h>
#include <cfloat>
#include <cmath>

#include "common_test.h"
#include "invoke_arg_array_builder.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "ScopedLocalRef.h"
#include "sirt_ref.h"

namespace art {

class JniInternalTest : public CommonTest {
 protected:
  virtual void SetUp() {
    CommonTest::SetUp();

    vm_ = Runtime::Current()->GetJavaVM();

    // Turn on -verbose:jni for the JNI tests.
    // gLogVerbosity.jni = true;

    vm_->AttachCurrentThread(&env_, NULL);

    ScopedLocalRef<jclass> aioobe(env_,
                                  env_->FindClass("java/lang/ArrayIndexOutOfBoundsException"));
    CHECK(aioobe.get() != NULL);
    aioobe_ = reinterpret_cast<jclass>(env_->NewGlobalRef(aioobe.get()));

    ScopedLocalRef<jclass> ase(env_, env_->FindClass("java/lang/ArrayStoreException"));
    CHECK(ase.get() != NULL);
    ase_ = reinterpret_cast<jclass>(env_->NewGlobalRef(ase.get()));

    ScopedLocalRef<jclass> sioobe(env_,
                                  env_->FindClass("java/lang/StringIndexOutOfBoundsException"));
    CHECK(sioobe.get() != NULL);
    sioobe_ = reinterpret_cast<jclass>(env_->NewGlobalRef(sioobe.get()));
  }

  void CleanUpJniEnv() {
    if (aioobe_ != NULL) {
      env_->DeleteGlobalRef(aioobe_);
      aioobe_ = NULL;
    }
    if (ase_ != NULL) {
      env_->DeleteGlobalRef(ase_);
      ase_ = NULL;
    }
    if (sioobe_ != NULL) {
      env_->DeleteGlobalRef(sioobe_);
      sioobe_ = NULL;
    }
  }

  virtual void TearDown() {
    CleanUpJniEnv();
    CommonTest::TearDown();
  }

  void DoCompile(mirror::ArtMethod*& method,
                 mirror::Object*& receiver,
                 bool is_static, const char* method_name,
                 const char* method_signature)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    const char* class_name = is_static ? "StaticLeafMethods" : "NonStaticLeafMethods";
    jobject jclass_loader(LoadDex(class_name));
    Thread* self = Thread::Current();
    SirtRef<mirror::ClassLoader>
        class_loader(self,
                     ScopedObjectAccessUnchecked(self).Decode<mirror::ClassLoader*>(jclass_loader));
    if (is_static) {
      CompileDirectMethod(class_loader.get(), class_name, method_name, method_signature);
    } else {
      CompileVirtualMethod(NULL, "java.lang.Class", "isFinalizable", "()Z");
      CompileDirectMethod(NULL, "java.lang.Object", "<init>", "()V");
      CompileVirtualMethod(class_loader.get(), class_name, method_name, method_signature);
    }

    mirror::Class* c = class_linker_->FindClass(DotToDescriptor(class_name).c_str(),
                                                class_loader.get());
    CHECK(c != NULL);

    method = is_static ? c->FindDirectMethod(method_name, method_signature)
                       : c->FindVirtualMethod(method_name, method_signature);
    CHECK(method != NULL);

    receiver = (is_static ? NULL : c->AllocObject(self));

    // Start runtime.
    bool started = runtime_->Start();
    CHECK(started);
    self->TransitionFromSuspendedToRunnable();
  }

  void InvokeNopMethod(bool is_static) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "nop", "()V");

    ArgArray arg_array(NULL, 0);
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
    }

    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'V');
  }

  void InvokeIdentityByteMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "identity", "(I)I");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    arg_array.Append(0);
    result.SetB(-1);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'B');
    EXPECT_EQ(0, result.GetB());

    args[0] = -1;
    result.SetB(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'B');
    EXPECT_EQ(-1, result.GetB());

    args[0] = SCHAR_MAX;
    result.SetB(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'B');
    EXPECT_EQ(SCHAR_MAX, result.GetB());

    args[0] = (SCHAR_MIN << 24) >> 24;
    result.SetB(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'B');
    EXPECT_EQ(SCHAR_MIN, result.GetB());
  }

  void InvokeIdentityIntMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "identity", "(I)I");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    arg_array.Append(0);
    result.SetI(-1);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(0, result.GetI());

    args[0] = -1;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(-1, result.GetI());

    args[0] = INT_MAX;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(INT_MAX, result.GetI());

    args[0] = INT_MIN;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(INT_MIN, result.GetI());
  }

  void InvokeIdentityDoubleMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "identity", "(D)D");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue value;
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    value.SetD(0.0);
    arg_array.AppendWide(value.GetJ());
    result.SetD(-1.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(0.0, result.GetD());

    value.SetD(-1.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(-1.0, result.GetD());

    value.SetD(DBL_MAX);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(DBL_MAX, result.GetD());

    value.SetD(DBL_MIN);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(DBL_MIN, result.GetD());
  }

  void InvokeSumIntIntMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "sum", "(II)I");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    arg_array.Append(0);
    arg_array.Append(0);
    result.SetI(-1);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(0, result.GetI());

    args[0] = 1;
    args[1] = 2;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(3, result.GetI());

    args[0] = -2;
    args[1] = 5;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(3, result.GetI());

    args[0] = INT_MAX;
    args[1] = INT_MIN;
    result.SetI(1234);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(-1, result.GetI());

    args[0] = INT_MAX;
    args[1] = INT_MAX;
    result.SetI(INT_MIN);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(-2, result.GetI());
  }

  void InvokeSumIntIntIntMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "sum", "(III)I");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    arg_array.Append(0);
    arg_array.Append(0);
    arg_array.Append(0);
    result.SetI(-1);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(0, result.GetI());

    args[0] = 1;
    args[1] = 2;
    args[2] = 3;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(6, result.GetI());

    args[0] = -1;
    args[1] = 2;
    args[2] = -3;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(-2, result.GetI());

    args[0] = INT_MAX;
    args[1] = INT_MIN;
    args[2] = INT_MAX;
    result.SetI(1234);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(2147483646, result.GetI());

    args[0] = INT_MAX;
    args[1] = INT_MAX;
    args[2] = INT_MAX;
    result.SetI(INT_MIN);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(2147483645, result.GetI());
  }

  void InvokeSumIntIntIntIntMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "sum", "(IIII)I");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    arg_array.Append(0);
    arg_array.Append(0);
    arg_array.Append(0);
    arg_array.Append(0);
    result.SetI(-1);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(0, result.GetI());

    args[0] = 1;
    args[1] = 2;
    args[2] = 3;
    args[3] = 4;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(10, result.GetI());

    args[0] = -1;
    args[1] = 2;
    args[2] = -3;
    args[3] = 4;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(2, result.GetI());

    args[0] = INT_MAX;
    args[1] = INT_MIN;
    args[2] = INT_MAX;
    args[3] = INT_MIN;
    result.SetI(1234);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(-2, result.GetI());

    args[0] = INT_MAX;
    args[1] = INT_MAX;
    args[2] = INT_MAX;
    args[3] = INT_MAX;
    result.SetI(INT_MIN);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(-4, result.GetI());
  }

  void InvokeSumIntIntIntIntIntMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "sum", "(IIIII)I");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    arg_array.Append(0);
    arg_array.Append(0);
    arg_array.Append(0);
    arg_array.Append(0);
    arg_array.Append(0);
    result.SetI(-1.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(0, result.GetI());

    args[0] = 1;
    args[1] = 2;
    args[2] = 3;
    args[3] = 4;
    args[4] = 5;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(15, result.GetI());

    args[0] = -1;
    args[1] = 2;
    args[2] = -3;
    args[3] = 4;
    args[4] = -5;
    result.SetI(0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(-3, result.GetI());

    args[0] = INT_MAX;
    args[1] = INT_MIN;
    args[2] = INT_MAX;
    args[3] = INT_MIN;
    args[4] = INT_MAX;
    result.SetI(1234);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(2147483645, result.GetI());

    args[0] = INT_MAX;
    args[1] = INT_MAX;
    args[2] = INT_MAX;
    args[3] = INT_MAX;
    args[4] = INT_MAX;
    result.SetI(INT_MIN);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'I');
    EXPECT_EQ(2147483643, result.GetI());
  }

  void InvokeSumDoubleDoubleMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "sum", "(DD)D");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue value;
    JValue value2;
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    value.SetD(0.0);
    value2.SetD(0.0);
    arg_array.AppendWide(value.GetJ());
    arg_array.AppendWide(value2.GetJ());
    result.SetD(-1.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(0.0, result.GetD());

    value.SetD(1.0);
    value2.SetD(2.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(3.0, result.GetD());

    value.SetD(1.0);
    value2.SetD(-2.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(-1.0, result.GetD());

    value.SetD(DBL_MAX);
    value2.SetD(DBL_MIN);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(1.7976931348623157e308, result.GetD());

    value.SetD(DBL_MAX);
    value2.SetD(DBL_MAX);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(INFINITY, result.GetD());
  }

  void InvokeSumDoubleDoubleDoubleMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "sum", "(DDD)D");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue value;
    JValue value2;
    JValue value3;
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    value.SetD(0.0);
    value2.SetD(0.0);
    value3.SetD(0.0);
    arg_array.AppendWide(value.GetJ());
    arg_array.AppendWide(value2.GetJ());
    arg_array.AppendWide(value3.GetJ());
    result.SetD(-1.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(0.0, result.GetD());

    value.SetD(1.0);
    value2.SetD(2.0);
    value3.SetD(3.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    args[4] = value3.GetJ();
    args[5] = value3.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(6.0, result.GetD());

    value.SetD(1.0);
    value2.SetD(-2.0);
    value3.SetD(3.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    args[4] = value3.GetJ();
    args[5] = value3.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(2.0, result.GetD());
  }

  void InvokeSumDoubleDoubleDoubleDoubleMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "sum", "(DDDD)D");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue value;
    JValue value2;
    JValue value3;
    JValue value4;
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    value.SetD(0.0);
    value2.SetD(0.0);
    value3.SetD(0.0);
    value4.SetD(0.0);
    arg_array.AppendWide(value.GetJ());
    arg_array.AppendWide(value2.GetJ());
    arg_array.AppendWide(value3.GetJ());
    arg_array.AppendWide(value4.GetJ());
    result.SetD(-1.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(0.0, result.GetD());

    value.SetD(1.0);
    value2.SetD(2.0);
    value3.SetD(3.0);
    value4.SetD(4.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    args[4] = value3.GetJ();
    args[5] = value3.GetJ() >> 32;
    args[6] = value4.GetJ();
    args[7] = value4.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(10.0, result.GetD());

    value.SetD(1.0);
    value2.SetD(-2.0);
    value3.SetD(3.0);
    value4.SetD(-4.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    args[4] = value3.GetJ();
    args[5] = value3.GetJ() >> 32;
    args[6] = value4.GetJ();
    args[7] = value4.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(-2.0, result.GetD());
  }

  void InvokeSumDoubleDoubleDoubleDoubleDoubleMethod(bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method;
    mirror::Object* receiver;
    DoCompile(method, receiver, is_static, "sum", "(DDDDD)D");

    ArgArray arg_array(NULL, 0);
    uint32_t* args = arg_array.GetArray();
    JValue value;
    JValue value2;
    JValue value3;
    JValue value4;
    JValue value5;
    JValue result;

    if (!is_static) {
      arg_array.Append(reinterpret_cast<uint32_t>(receiver));
      args++;
    }

    value.SetD(0.0);
    value2.SetD(0.0);
    value3.SetD(0.0);
    value4.SetD(0.0);
    value5.SetD(0.0);
    arg_array.AppendWide(value.GetJ());
    arg_array.AppendWide(value2.GetJ());
    arg_array.AppendWide(value3.GetJ());
    arg_array.AppendWide(value4.GetJ());
    arg_array.AppendWide(value5.GetJ());
    result.SetD(-1.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(0.0, result.GetD());

    value.SetD(1.0);
    value2.SetD(2.0);
    value3.SetD(3.0);
    value4.SetD(4.0);
    value5.SetD(5.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    args[4] = value3.GetJ();
    args[5] = value3.GetJ() >> 32;
    args[6] = value4.GetJ();
    args[7] = value4.GetJ() >> 32;
    args[8] = value5.GetJ();
    args[9] = value5.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(15.0, result.GetD());

    value.SetD(1.0);
    value2.SetD(-2.0);
    value3.SetD(3.0);
    value4.SetD(-4.0);
    value5.SetD(5.0);
    args[0] = value.GetJ();
    args[1] = value.GetJ() >> 32;
    args[2] = value2.GetJ();
    args[3] = value2.GetJ() >> 32;
    args[4] = value3.GetJ();
    args[5] = value3.GetJ() >> 32;
    args[6] = value4.GetJ();
    args[7] = value4.GetJ() >> 32;
    args[8] = value5.GetJ();
    args[9] = value5.GetJ() >> 32;
    result.SetD(0.0);
    method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'D');
    EXPECT_EQ(3.0, result.GetD());
  }

  JavaVMExt* vm_;
  JNIEnv* env_;
  jclass aioobe_;
  jclass ase_;
  jclass sioobe_;
};

TEST_F(JniInternalTest, AllocObject) {
  jclass c = env_->FindClass("java/lang/String");
  ASSERT_TRUE(c != NULL);
  jobject o = env_->AllocObject(c);
  ASSERT_TRUE(o != NULL);

  // We have an instance of the class we asked for...
  ASSERT_TRUE(env_->IsInstanceOf(o, c));
  // ...whose fields haven't been initialized because
  // we didn't call a constructor.
  ASSERT_EQ(0, env_->GetIntField(o, env_->GetFieldID(c, "count", "I")));
  ASSERT_EQ(0, env_->GetIntField(o, env_->GetFieldID(c, "offset", "I")));
  ASSERT_TRUE(env_->GetObjectField(o, env_->GetFieldID(c, "value", "[C")) == NULL);
}

TEST_F(JniInternalTest, GetVersion) {
  ASSERT_EQ(JNI_VERSION_1_6, env_->GetVersion());
}

#define EXPECT_CLASS_FOUND(NAME) \
  EXPECT_TRUE(env_->FindClass(NAME) != NULL); \
  EXPECT_FALSE(env_->ExceptionCheck())

#define EXPECT_CLASS_NOT_FOUND(NAME) \
  EXPECT_TRUE(env_->FindClass(NAME) == NULL); \
  EXPECT_TRUE(env_->ExceptionCheck()); \
  env_->ExceptionClear()

TEST_F(JniInternalTest, FindClass) {
  // Reference types...
  EXPECT_CLASS_FOUND("java/lang/String");
  // ...for arrays too, where you must include "L;".
  EXPECT_CLASS_FOUND("[Ljava/lang/String;");
  // Primitive arrays are okay too, if the primitive type is valid.
  EXPECT_CLASS_FOUND("[C");

  {
    // We support . as well as / for compatibility, if -Xcheck:jni is off.
    CheckJniAbortCatcher check_jni_abort_catcher;
    EXPECT_CLASS_FOUND("java.lang.String");
    check_jni_abort_catcher.Check("illegal class name 'java.lang.String'");
    EXPECT_CLASS_NOT_FOUND("Ljava.lang.String;");
    check_jni_abort_catcher.Check("illegal class name 'Ljava.lang.String;'");
    EXPECT_CLASS_FOUND("[Ljava.lang.String;");
    check_jni_abort_catcher.Check("illegal class name '[Ljava.lang.String;'");
    EXPECT_CLASS_NOT_FOUND("[java.lang.String");
    check_jni_abort_catcher.Check("illegal class name '[java.lang.String'");

    // You can't include the "L;" in a JNI class descriptor.
    EXPECT_CLASS_NOT_FOUND("Ljava/lang/String;");
    check_jni_abort_catcher.Check("illegal class name 'Ljava/lang/String;'");

    // But you must include it for an array of any reference type.
    EXPECT_CLASS_NOT_FOUND("[java/lang/String");
    check_jni_abort_catcher.Check("illegal class name '[java/lang/String'");

    EXPECT_CLASS_NOT_FOUND("[K");
    check_jni_abort_catcher.Check("illegal class name '[K'");
  }

  // But primitive types aren't allowed...
  EXPECT_CLASS_NOT_FOUND("C");
  EXPECT_CLASS_NOT_FOUND("K");
}

#define EXPECT_EXCEPTION(exception_class) \
  do { \
    EXPECT_TRUE(env_->ExceptionCheck()); \
    jthrowable exception = env_->ExceptionOccurred(); \
    EXPECT_NE(static_cast<jthrowable>(NULL), exception); \
    env_->ExceptionClear(); \
    EXPECT_TRUE(env_->IsInstanceOf(exception, exception_class)); \
  } while (false)

TEST_F(JniInternalTest, GetFieldID) {
  jclass jlnsfe = env_->FindClass("java/lang/NoSuchFieldError");
  ASSERT_TRUE(jlnsfe != NULL);
  jclass c = env_->FindClass("java/lang/String");
  ASSERT_TRUE(c != NULL);

  // Wrong type.
  jfieldID fid = env_->GetFieldID(c, "count", "J");
  EXPECT_EQ(static_cast<jfieldID>(NULL), fid);
  EXPECT_EXCEPTION(jlnsfe);

  // Wrong type where type doesn't exist.
  fid = env_->GetFieldID(c, "count", "Lrod/jane/freddy;");
  EXPECT_EQ(static_cast<jfieldID>(NULL), fid);
  EXPECT_EXCEPTION(jlnsfe);

  // Wrong name.
  fid = env_->GetFieldID(c, "Count", "I");
  EXPECT_EQ(static_cast<jfieldID>(NULL), fid);
  EXPECT_EXCEPTION(jlnsfe);

  // Good declared field lookup.
  fid = env_->GetFieldID(c, "count", "I");
  EXPECT_NE(static_cast<jfieldID>(NULL), fid);
  EXPECT_TRUE(fid != NULL);
  EXPECT_FALSE(env_->ExceptionCheck());

  // Good superclass field lookup.
  c = env_->FindClass("java/lang/StringBuilder");
  fid = env_->GetFieldID(c, "count", "I");
  EXPECT_NE(static_cast<jfieldID>(NULL), fid);
  EXPECT_TRUE(fid != NULL);
  EXPECT_FALSE(env_->ExceptionCheck());

  // Not instance.
  fid = env_->GetFieldID(c, "CASE_INSENSITIVE_ORDER", "Ljava/util/Comparator;");
  EXPECT_EQ(static_cast<jfieldID>(NULL), fid);
  EXPECT_EXCEPTION(jlnsfe);
}

TEST_F(JniInternalTest, GetStaticFieldID) {
  jclass jlnsfe = env_->FindClass("java/lang/NoSuchFieldError");
  ASSERT_TRUE(jlnsfe != NULL);
  jclass c = env_->FindClass("java/lang/String");
  ASSERT_TRUE(c != NULL);

  // Wrong type.
  jfieldID fid = env_->GetStaticFieldID(c, "CASE_INSENSITIVE_ORDER", "J");
  EXPECT_EQ(static_cast<jfieldID>(NULL), fid);
  EXPECT_EXCEPTION(jlnsfe);

  // Wrong type where type doesn't exist.
  fid = env_->GetStaticFieldID(c, "CASE_INSENSITIVE_ORDER", "Lrod/jane/freddy;");
  EXPECT_EQ(static_cast<jfieldID>(NULL), fid);
  EXPECT_EXCEPTION(jlnsfe);

  // Wrong name.
  fid = env_->GetStaticFieldID(c, "cASE_INSENSITIVE_ORDER", "Ljava/util/Comparator;");
  EXPECT_EQ(static_cast<jfieldID>(NULL), fid);
  EXPECT_EXCEPTION(jlnsfe);

  // Good declared field lookup.
  fid = env_->GetStaticFieldID(c, "CASE_INSENSITIVE_ORDER", "Ljava/util/Comparator;");
  EXPECT_NE(static_cast<jfieldID>(NULL), fid);
  EXPECT_TRUE(fid != NULL);
  EXPECT_FALSE(env_->ExceptionCheck());

  // Not static.
  fid = env_->GetStaticFieldID(c, "count", "I");
  EXPECT_EQ(static_cast<jfieldID>(NULL), fid);
  EXPECT_EXCEPTION(jlnsfe);
}

TEST_F(JniInternalTest, GetMethodID) {
  jclass jlobject = env_->FindClass("java/lang/Object");
  jclass jlstring = env_->FindClass("java/lang/String");
  jclass jlnsme = env_->FindClass("java/lang/NoSuchMethodError");

  // Sanity check that no exceptions are pending
  ASSERT_FALSE(env_->ExceptionCheck());

  // Check that java.lang.Object.foo() doesn't exist and NoSuchMethodError is
  // a pending exception
  jmethodID method = env_->GetMethodID(jlobject, "foo", "()V");
  EXPECT_EQ(static_cast<jmethodID>(NULL), method);
  EXPECT_EXCEPTION(jlnsme);

  // Check that java.lang.Object.equals() does exist
  method = env_->GetMethodID(jlobject, "equals", "(Ljava/lang/Object;)Z");
  EXPECT_NE(static_cast<jmethodID>(NULL), method);
  EXPECT_FALSE(env_->ExceptionCheck());

  // Check that GetMethodID for java.lang.String.valueOf(int) fails as the
  // method is static
  method = env_->GetMethodID(jlstring, "valueOf", "(I)Ljava/lang/String;");
  EXPECT_EQ(static_cast<jmethodID>(NULL), method);
  EXPECT_EXCEPTION(jlnsme);

  // Check that GetMethodID for java.lang.NoSuchMethodError.<init>(String) finds the constructor
  method = env_->GetMethodID(jlnsme, "<init>", "(Ljava/lang/String;)V");
  EXPECT_NE(static_cast<jmethodID>(NULL), method);
  EXPECT_FALSE(env_->ExceptionCheck());
}

TEST_F(JniInternalTest, GetStaticMethodID) {
  jclass jlobject = env_->FindClass("java/lang/Object");
  jclass jlnsme = env_->FindClass("java/lang/NoSuchMethodError");

  // Sanity check that no exceptions are pending
  ASSERT_FALSE(env_->ExceptionCheck());

  // Check that java.lang.Object.foo() doesn't exist and NoSuchMethodError is
  // a pending exception
  jmethodID method = env_->GetStaticMethodID(jlobject, "foo", "()V");
  EXPECT_EQ(static_cast<jmethodID>(NULL), method);
  EXPECT_EXCEPTION(jlnsme);

  // Check that GetStaticMethodID for java.lang.Object.equals(Object) fails as
  // the method is not static
  method = env_->GetStaticMethodID(jlobject, "equals", "(Ljava/lang/Object;)Z");
  EXPECT_EQ(static_cast<jmethodID>(NULL), method);
  EXPECT_EXCEPTION(jlnsme);

  // Check that java.lang.String.valueOf(int) does exist
  jclass jlstring = env_->FindClass("java/lang/String");
  method = env_->GetStaticMethodID(jlstring, "valueOf",
                                   "(I)Ljava/lang/String;");
  EXPECT_NE(static_cast<jmethodID>(NULL), method);
  EXPECT_FALSE(env_->ExceptionCheck());
}

TEST_F(JniInternalTest, FromReflectedField_ToReflectedField) {
  jclass jlrField = env_->FindClass("java/lang/reflect/Field");
  jclass c = env_->FindClass("java/lang/String");
  ASSERT_TRUE(c != NULL);
  jfieldID fid = env_->GetFieldID(c, "count", "I");
  ASSERT_TRUE(fid != NULL);
  // Turn the fid into a java.lang.reflect.Field...
  jobject field = env_->ToReflectedField(c, fid, JNI_FALSE);
  ASSERT_TRUE(c != NULL);
  ASSERT_TRUE(env_->IsInstanceOf(field, jlrField));
  // ...and back again.
  jfieldID fid2 = env_->FromReflectedField(field);
  ASSERT_TRUE(fid2 != NULL);
  // Make sure we can actually use it.
  jstring s = env_->NewStringUTF("poop");
  ASSERT_EQ(4, env_->GetIntField(s, fid2));
}

TEST_F(JniInternalTest, FromReflectedMethod_ToReflectedMethod) {
  jclass jlrMethod = env_->FindClass("java/lang/reflect/Method");
  jclass c = env_->FindClass("java/lang/String");
  ASSERT_TRUE(c != NULL);
  jmethodID mid = env_->GetMethodID(c, "length", "()I");
  ASSERT_TRUE(mid != NULL);
  // Turn the mid into a java.lang.reflect.Method...
  jobject method = env_->ToReflectedMethod(c, mid, JNI_FALSE);
  ASSERT_TRUE(c != NULL);
  ASSERT_TRUE(env_->IsInstanceOf(method, jlrMethod));
  // ...and back again.
  jmethodID mid2 = env_->FromReflectedMethod(method);
  ASSERT_TRUE(mid2 != NULL);
  // Make sure we can actually use it.
  jstring s = env_->NewStringUTF("poop");
  // TODO: this should return 4, but the runtime skips the method
  // invoke because the runtime isn't started. In the future it would
  // be nice to use interpretter for things like this. This still does
  // validate that we have a sane jmethodID value.
  ASSERT_EQ(0, env_->CallIntMethod(s, mid2));
}

void BogusMethod() {
  // You can't pass NULL function pointers to RegisterNatives.
}

TEST_F(JniInternalTest, RegisterNatives) {
  jclass jlobject = env_->FindClass("java/lang/Object");
  jclass jlnsme = env_->FindClass("java/lang/NoSuchMethodError");

  // Sanity check that no exceptions are pending
  ASSERT_FALSE(env_->ExceptionCheck());

  // Check that registering to a non-existent java.lang.Object.foo() causes a
  // NoSuchMethodError
  {
    JNINativeMethod methods[] = { { "foo", "()V", NULL } };
    env_->RegisterNatives(jlobject, methods, 1);
  }
  EXPECT_EXCEPTION(jlnsme);

  // Check that registering non-native methods causes a NoSuchMethodError
  {
    JNINativeMethod methods[] = { { "equals", "(Ljava/lang/Object;)Z", NULL } };
    env_->RegisterNatives(jlobject, methods, 1);
  }
  EXPECT_EXCEPTION(jlnsme);

  // Check that registering native methods is successful
  {
    JNINativeMethod methods[] = { { "notify", "()V", reinterpret_cast<void*>(BogusMethod) } };
    env_->RegisterNatives(jlobject, methods, 1);
  }
  EXPECT_FALSE(env_->ExceptionCheck());

  env_->UnregisterNatives(jlobject);
}

#define EXPECT_PRIMITIVE_ARRAY(new_fn, \
                               get_region_fn, \
                               set_region_fn, \
                               get_elements_fn, \
                               release_elements_fn, \
                               scalar_type, \
                               expected_class_descriptor) \
  jsize size = 4; \
  /* Allocate an array and check it has the right type and length. */ \
  scalar_type ## Array a = env_->new_fn(size); \
  EXPECT_TRUE(a != NULL); \
  EXPECT_TRUE(env_->IsInstanceOf(a, env_->FindClass(expected_class_descriptor))); \
  EXPECT_EQ(size, env_->GetArrayLength(a)); \
  /* AIOOBE for negative start offset. */ \
  env_->get_region_fn(a, -1, 1, NULL); \
  EXPECT_EXCEPTION(aioobe_); \
  env_->set_region_fn(a, -1, 1, NULL); \
  EXPECT_EXCEPTION(aioobe_); \
  /* AIOOBE for negative length. */ \
  env_->get_region_fn(a, 0, -1, NULL); \
  EXPECT_EXCEPTION(aioobe_); \
  env_->set_region_fn(a, 0, -1, NULL); \
  EXPECT_EXCEPTION(aioobe_); \
  /* AIOOBE for buffer overrun. */ \
  env_->get_region_fn(a, size - 1, size, NULL); \
  EXPECT_EXCEPTION(aioobe_); \
  env_->set_region_fn(a, size - 1, size, NULL); \
  EXPECT_EXCEPTION(aioobe_); \
  /* Prepare a couple of buffers. */ \
  UniquePtr<scalar_type[]> src_buf(new scalar_type[size]); \
  UniquePtr<scalar_type[]> dst_buf(new scalar_type[size]); \
  for (jsize i = 0; i < size; ++i) { src_buf[i] = scalar_type(i); } \
  for (jsize i = 0; i < size; ++i) { dst_buf[i] = scalar_type(-1); } \
  /* Copy all of src_buf onto the heap. */ \
  env_->set_region_fn(a, 0, size, &src_buf[0]); \
  /* Copy back only part. */ \
  env_->get_region_fn(a, 1, size - 2, &dst_buf[1]); \
  EXPECT_NE(memcmp(&src_buf[0], &dst_buf[0], size * sizeof(scalar_type)), 0) \
    << "short copy equal"; \
  /* Copy the missing pieces. */ \
  env_->get_region_fn(a, 0, 1, &dst_buf[0]); \
  env_->get_region_fn(a, size - 1, 1, &dst_buf[size - 1]); \
  EXPECT_EQ(memcmp(&src_buf[0], &dst_buf[0], size * sizeof(scalar_type)), 0) \
    << "fixed copy not equal"; \
  /* Copy back the whole array. */ \
  env_->get_region_fn(a, 0, size, &dst_buf[0]); \
  EXPECT_EQ(memcmp(&src_buf[0], &dst_buf[0], size * sizeof(scalar_type)), 0) \
    << "full copy not equal"; \
  /* GetPrimitiveArrayCritical */ \
  void* v = env_->GetPrimitiveArrayCritical(a, NULL); \
  EXPECT_EQ(memcmp(&src_buf[0], v, size * sizeof(scalar_type)), 0) \
    << "GetPrimitiveArrayCritical not equal"; \
  env_->ReleasePrimitiveArrayCritical(a, v, 0); \
  /* GetXArrayElements */ \
  scalar_type* xs = env_->get_elements_fn(a, NULL); \
  EXPECT_EQ(memcmp(&src_buf[0], xs, size * sizeof(scalar_type)), 0) \
    << # get_elements_fn " not equal"; \
  env_->release_elements_fn(a, xs, 0); \
  EXPECT_EQ(reinterpret_cast<uintptr_t>(v), reinterpret_cast<uintptr_t>(xs))

TEST_F(JniInternalTest, BooleanArrays) {
  EXPECT_PRIMITIVE_ARRAY(NewBooleanArray, GetBooleanArrayRegion, SetBooleanArrayRegion,
                         GetBooleanArrayElements, ReleaseBooleanArrayElements, jboolean, "[Z");
}
TEST_F(JniInternalTest, ByteArrays) {
  EXPECT_PRIMITIVE_ARRAY(NewByteArray, GetByteArrayRegion, SetByteArrayRegion,
                         GetByteArrayElements, ReleaseByteArrayElements, jbyte, "[B");
}
TEST_F(JniInternalTest, CharArrays) {
  EXPECT_PRIMITIVE_ARRAY(NewCharArray, GetCharArrayRegion, SetCharArrayRegion,
                         GetCharArrayElements, ReleaseCharArrayElements, jchar, "[C");
}
TEST_F(JniInternalTest, DoubleArrays) {
  EXPECT_PRIMITIVE_ARRAY(NewDoubleArray, GetDoubleArrayRegion, SetDoubleArrayRegion,
                         GetDoubleArrayElements, ReleaseDoubleArrayElements, jdouble, "[D");
}
TEST_F(JniInternalTest, FloatArrays) {
  EXPECT_PRIMITIVE_ARRAY(NewFloatArray, GetFloatArrayRegion, SetFloatArrayRegion,
                         GetFloatArrayElements, ReleaseFloatArrayElements, jfloat, "[F");
}
TEST_F(JniInternalTest, IntArrays) {
  EXPECT_PRIMITIVE_ARRAY(NewIntArray, GetIntArrayRegion, SetIntArrayRegion,
                         GetIntArrayElements, ReleaseIntArrayElements, jint, "[I");
}
TEST_F(JniInternalTest, LongArrays) {
  EXPECT_PRIMITIVE_ARRAY(NewLongArray, GetLongArrayRegion, SetLongArrayRegion,
                         GetLongArrayElements, ReleaseLongArrayElements, jlong, "[J");
}
TEST_F(JniInternalTest, ShortArrays) {
  EXPECT_PRIMITIVE_ARRAY(NewShortArray, GetShortArrayRegion, SetShortArrayRegion,
                         GetShortArrayElements, ReleaseShortArrayElements, jshort, "[S");
}

TEST_F(JniInternalTest, NewObjectArray) {
  // TODO: death tests for negative array sizes.

  // TODO: check non-NULL initial elements.

  jclass element_class = env_->FindClass("java/lang/String");
  ASSERT_TRUE(element_class != NULL);
  jclass array_class = env_->FindClass("[Ljava/lang/String;");
  ASSERT_TRUE(array_class != NULL);

  jobjectArray a;

  a = env_->NewObjectArray(0, element_class, NULL);
  EXPECT_TRUE(a != NULL);
  EXPECT_TRUE(env_->IsInstanceOf(a, array_class));
  EXPECT_EQ(0, env_->GetArrayLength(a));

  a = env_->NewObjectArray(1, element_class, NULL);
  EXPECT_TRUE(a != NULL);
  EXPECT_TRUE(env_->IsInstanceOf(a, array_class));
  EXPECT_EQ(1, env_->GetArrayLength(a));
  EXPECT_TRUE(env_->IsSameObject(env_->GetObjectArrayElement(a, 0), NULL));

  jstring s = env_->NewStringUTF("poop");
  a = env_->NewObjectArray(2, element_class, s);
  EXPECT_TRUE(a != NULL);
  EXPECT_TRUE(env_->IsInstanceOf(a, array_class));
  EXPECT_EQ(2, env_->GetArrayLength(a));
  EXPECT_TRUE(env_->IsSameObject(env_->GetObjectArrayElement(a, 0), s));
  EXPECT_TRUE(env_->IsSameObject(env_->GetObjectArrayElement(a, 1), s));
}

TEST_F(JniInternalTest, GetArrayLength) {
  // Already tested in NewObjectArray/NewPrimitiveArray.
}

TEST_F(JniInternalTest, GetObjectClass) {
  jclass string_class = env_->FindClass("java/lang/String");
  ASSERT_TRUE(string_class != NULL);
  jclass class_class = env_->FindClass("java/lang/Class");
  ASSERT_TRUE(class_class != NULL);

  jstring s = env_->NewStringUTF("poop");
  jclass c = env_->GetObjectClass(s);
  ASSERT_TRUE(env_->IsSameObject(string_class, c));

  jclass c2 = env_->GetObjectClass(c);
  ASSERT_TRUE(env_->IsSameObject(class_class, env_->GetObjectClass(c2)));
}

TEST_F(JniInternalTest, GetSuperclass) {
  jclass object_class = env_->FindClass("java/lang/Object");
  ASSERT_TRUE(object_class != NULL);
  jclass string_class = env_->FindClass("java/lang/String");
  ASSERT_TRUE(string_class != NULL);
  jclass runnable_interface = env_->FindClass("java/lang/Runnable");
  ASSERT_TRUE(runnable_interface != NULL);
  ASSERT_TRUE(env_->IsSameObject(object_class, env_->GetSuperclass(string_class)));
  ASSERT_TRUE(env_->GetSuperclass(object_class) == NULL);
  ASSERT_TRUE(env_->IsSameObject(object_class, env_->GetSuperclass(runnable_interface)));
}

TEST_F(JniInternalTest, IsAssignableFrom) {
  jclass object_class = env_->FindClass("java/lang/Object");
  ASSERT_TRUE(object_class != NULL);
  jclass string_class = env_->FindClass("java/lang/String");
  ASSERT_TRUE(string_class != NULL);

  ASSERT_TRUE(env_->IsAssignableFrom(object_class, string_class));
  ASSERT_FALSE(env_->IsAssignableFrom(string_class, object_class));
}

TEST_F(JniInternalTest, GetObjectRefType) {
  jclass local = env_->FindClass("java/lang/Object");
  ASSERT_TRUE(local != NULL);
  EXPECT_EQ(JNILocalRefType, env_->GetObjectRefType(local));

  jobject global = env_->NewGlobalRef(local);
  EXPECT_EQ(JNIGlobalRefType, env_->GetObjectRefType(global));

  jweak weak_global = env_->NewWeakGlobalRef(local);
  EXPECT_EQ(JNIWeakGlobalRefType, env_->GetObjectRefType(weak_global));

  jobject invalid = reinterpret_cast<jobject>(this);
  EXPECT_EQ(JNIInvalidRefType, env_->GetObjectRefType(invalid));

  // TODO: invoke a native method and test that its arguments are considered local references.
}

TEST_F(JniInternalTest, NewStringUTF) {
  EXPECT_TRUE(env_->NewStringUTF(NULL) == NULL);
  jstring s;

  s = env_->NewStringUTF("");
  EXPECT_TRUE(s != NULL);
  EXPECT_EQ(0, env_->GetStringLength(s));
  EXPECT_EQ(0, env_->GetStringUTFLength(s));
  s = env_->NewStringUTF("hello");
  EXPECT_TRUE(s != NULL);
  EXPECT_EQ(5, env_->GetStringLength(s));
  EXPECT_EQ(5, env_->GetStringUTFLength(s));

  // TODO: check some non-ASCII strings.
}

TEST_F(JniInternalTest, NewString) {
  jchar chars[] = { 'h', 'i' };
  jstring s;
  s = env_->NewString(chars, 0);
  EXPECT_TRUE(s != NULL);
  EXPECT_EQ(0, env_->GetStringLength(s));
  EXPECT_EQ(0, env_->GetStringUTFLength(s));
  s = env_->NewString(chars, 2);
  EXPECT_TRUE(s != NULL);
  EXPECT_EQ(2, env_->GetStringLength(s));
  EXPECT_EQ(2, env_->GetStringUTFLength(s));

  // TODO: check some non-ASCII strings.
}

TEST_F(JniInternalTest, NewStringNullCharsZeroLength) {
  jstring s = env_->NewString(NULL, 0);
  EXPECT_TRUE(s != NULL);
  EXPECT_EQ(0, env_->GetStringLength(s));
}

// TODO: fix gtest death tests on host http://b/5690440 (and target)
TEST_F(JniInternalTest, DISABLED_NewStringNullCharsNonzeroLength) {
  ASSERT_DEATH(env_->NewString(NULL, 1), "");
}

TEST_F(JniInternalTest, GetStringLength_GetStringUTFLength) {
  // Already tested in the NewString/NewStringUTF tests.
}

TEST_F(JniInternalTest, GetStringRegion_GetStringUTFRegion) {
  jstring s = env_->NewStringUTF("hello");
  ASSERT_TRUE(s != NULL);

  env_->GetStringRegion(s, -1, 0, NULL);
  EXPECT_EXCEPTION(sioobe_);
  env_->GetStringRegion(s, 0, -1, NULL);
  EXPECT_EXCEPTION(sioobe_);
  env_->GetStringRegion(s, 0, 10, NULL);
  EXPECT_EXCEPTION(sioobe_);
  env_->GetStringRegion(s, 10, 1, NULL);
  EXPECT_EXCEPTION(sioobe_);

  jchar chars[4] = { 'x', 'x', 'x', 'x' };
  env_->GetStringRegion(s, 1, 2, &chars[1]);
  EXPECT_EQ('x', chars[0]);
  EXPECT_EQ('e', chars[1]);
  EXPECT_EQ('l', chars[2]);
  EXPECT_EQ('x', chars[3]);

  env_->GetStringUTFRegion(s, -1, 0, NULL);
  EXPECT_EXCEPTION(sioobe_);
  env_->GetStringUTFRegion(s, 0, -1, NULL);
  EXPECT_EXCEPTION(sioobe_);
  env_->GetStringUTFRegion(s, 0, 10, NULL);
  EXPECT_EXCEPTION(sioobe_);
  env_->GetStringUTFRegion(s, 10, 1, NULL);
  EXPECT_EXCEPTION(sioobe_);

  char bytes[4] = { 'x', 'x', 'x', 'x' };
  env_->GetStringUTFRegion(s, 1, 2, &bytes[1]);
  EXPECT_EQ('x', bytes[0]);
  EXPECT_EQ('e', bytes[1]);
  EXPECT_EQ('l', bytes[2]);
  EXPECT_EQ('x', bytes[3]);
}

TEST_F(JniInternalTest, GetStringUTFChars_ReleaseStringUTFChars) {
  // Passing in a NULL jstring is ignored normally, but caught by -Xcheck:jni.
  {
    CheckJniAbortCatcher check_jni_abort_catcher;
    EXPECT_TRUE(env_->GetStringUTFChars(NULL, NULL) == NULL);
    check_jni_abort_catcher.Check("GetStringUTFChars received null jstring");
  }

  jstring s = env_->NewStringUTF("hello");
  ASSERT_TRUE(s != NULL);

  const char* utf = env_->GetStringUTFChars(s, NULL);
  EXPECT_STREQ("hello", utf);
  env_->ReleaseStringUTFChars(s, utf);

  jboolean is_copy = JNI_FALSE;
  utf = env_->GetStringUTFChars(s, &is_copy);
  EXPECT_EQ(JNI_TRUE, is_copy);
  EXPECT_STREQ("hello", utf);
  env_->ReleaseStringUTFChars(s, utf);
}

TEST_F(JniInternalTest, GetStringChars_ReleaseStringChars) {
  jstring s = env_->NewStringUTF("hello");
  ASSERT_TRUE(s != NULL);

  jchar expected[] = { 'h', 'e', 'l', 'l', 'o' };
  const jchar* chars = env_->GetStringChars(s, NULL);
  EXPECT_EQ(expected[0], chars[0]);
  EXPECT_EQ(expected[1], chars[1]);
  EXPECT_EQ(expected[2], chars[2]);
  EXPECT_EQ(expected[3], chars[3]);
  EXPECT_EQ(expected[4], chars[4]);
  env_->ReleaseStringChars(s, chars);

  jboolean is_copy = JNI_FALSE;
  chars = env_->GetStringChars(s, &is_copy);
  EXPECT_EQ(JNI_FALSE, is_copy);
  EXPECT_EQ(expected[0], chars[0]);
  EXPECT_EQ(expected[1], chars[1]);
  EXPECT_EQ(expected[2], chars[2]);
  EXPECT_EQ(expected[3], chars[3]);
  EXPECT_EQ(expected[4], chars[4]);
  env_->ReleaseStringChars(s, chars);
}

TEST_F(JniInternalTest, GetStringCritical_ReleaseStringCritical) {
  jstring s = env_->NewStringUTF("hello");
  ASSERT_TRUE(s != NULL);

  jchar expected[] = { 'h', 'e', 'l', 'l', 'o' };
  const jchar* chars = env_->GetStringCritical(s, NULL);
  EXPECT_EQ(expected[0], chars[0]);
  EXPECT_EQ(expected[1], chars[1]);
  EXPECT_EQ(expected[2], chars[2]);
  EXPECT_EQ(expected[3], chars[3]);
  EXPECT_EQ(expected[4], chars[4]);
  env_->ReleaseStringCritical(s, chars);

  jboolean is_copy = JNI_FALSE;
  chars = env_->GetStringCritical(s, &is_copy);
  EXPECT_EQ(JNI_FALSE, is_copy);
  EXPECT_EQ(expected[0], chars[0]);
  EXPECT_EQ(expected[1], chars[1]);
  EXPECT_EQ(expected[2], chars[2]);
  EXPECT_EQ(expected[3], chars[3]);
  EXPECT_EQ(expected[4], chars[4]);
  env_->ReleaseStringCritical(s, chars);
}

TEST_F(JniInternalTest, GetObjectArrayElement_SetObjectArrayElement) {
  jclass java_lang_Class = env_->FindClass("java/lang/Class");
  ASSERT_TRUE(java_lang_Class != NULL);

  jobjectArray array = env_->NewObjectArray(1, java_lang_Class, NULL);
  EXPECT_TRUE(array != NULL);
  EXPECT_TRUE(env_->GetObjectArrayElement(array, 0) == NULL);
  env_->SetObjectArrayElement(array, 0, java_lang_Class);
  EXPECT_TRUE(env_->IsSameObject(env_->GetObjectArrayElement(array, 0), java_lang_Class));

  // ArrayIndexOutOfBounds for negative index.
  env_->SetObjectArrayElement(array, -1, java_lang_Class);
  EXPECT_EXCEPTION(aioobe_);

  // ArrayIndexOutOfBounds for too-large index.
  env_->SetObjectArrayElement(array, 1, java_lang_Class);
  EXPECT_EXCEPTION(aioobe_);

  // ArrayStoreException thrown for bad types.
  env_->SetObjectArrayElement(array, 0, env_->NewStringUTF("not a jclass!"));
  EXPECT_EXCEPTION(ase_);
}

#define EXPECT_STATIC_PRIMITIVE_FIELD(type, field_name, sig, value1, value2) \
  do { \
    jfieldID fid = env_->GetStaticFieldID(c, field_name, sig); \
    EXPECT_TRUE(fid != NULL); \
    env_->SetStatic ## type ## Field(c, fid, value1); \
    EXPECT_TRUE(value1 == env_->GetStatic ## type ## Field(c, fid)); \
    env_->SetStatic ## type ## Field(c, fid, value2); \
    EXPECT_TRUE(value2 == env_->GetStatic ## type ## Field(c, fid)); \
  } while (false)

#define EXPECT_PRIMITIVE_FIELD(instance, type, field_name, sig, value1, value2) \
  do { \
    jfieldID fid = env_->GetFieldID(c, field_name, sig); \
    EXPECT_TRUE(fid != NULL); \
    env_->Set ## type ## Field(instance, fid, value1); \
    EXPECT_TRUE(value1 == env_->Get ## type ## Field(instance, fid)); \
    env_->Set ## type ## Field(instance, fid, value2); \
    EXPECT_TRUE(value2 == env_->Get ## type ## Field(instance, fid)); \
  } while (false)


#if !defined(ART_USE_PORTABLE_COMPILER)
TEST_F(JniInternalTest, GetPrimitiveField_SetPrimitiveField) {
  Thread::Current()->TransitionFromSuspendedToRunnable();
  LoadDex("AllFields");
  bool started = runtime_->Start();
  CHECK(started);

  jclass c = env_->FindClass("AllFields");
  ASSERT_TRUE(c != NULL);
  jobject o = env_->AllocObject(c);
  ASSERT_TRUE(o != NULL);

  EXPECT_STATIC_PRIMITIVE_FIELD(Boolean, "sZ", "Z", true, false);
  EXPECT_STATIC_PRIMITIVE_FIELD(Byte, "sB", "B", 1, 2);
  EXPECT_STATIC_PRIMITIVE_FIELD(Char, "sC", "C", 'a', 'b');
  EXPECT_STATIC_PRIMITIVE_FIELD(Double, "sD", "D", 1.0, 2.0);
  EXPECT_STATIC_PRIMITIVE_FIELD(Float, "sF", "F", 1.0, 2.0);
  EXPECT_STATIC_PRIMITIVE_FIELD(Int, "sI", "I", 1, 2);
  EXPECT_STATIC_PRIMITIVE_FIELD(Long, "sJ", "J", 1, 2);
  EXPECT_STATIC_PRIMITIVE_FIELD(Short, "sS", "S", 1, 2);

  EXPECT_PRIMITIVE_FIELD(o, Boolean, "iZ", "Z", true, false);
  EXPECT_PRIMITIVE_FIELD(o, Byte, "iB", "B", 1, 2);
  EXPECT_PRIMITIVE_FIELD(o, Char, "iC", "C", 'a', 'b');
  EXPECT_PRIMITIVE_FIELD(o, Double, "iD", "D", 1.0, 2.0);
  EXPECT_PRIMITIVE_FIELD(o, Float, "iF", "F", 1.0, 2.0);
  EXPECT_PRIMITIVE_FIELD(o, Int, "iI", "I", 1, 2);
  EXPECT_PRIMITIVE_FIELD(o, Long, "iJ", "J", 1, 2);
  EXPECT_PRIMITIVE_FIELD(o, Short, "iS", "S", 1, 2);
}

TEST_F(JniInternalTest, GetObjectField_SetObjectField) {
  Thread::Current()->TransitionFromSuspendedToRunnable();
  LoadDex("AllFields");
  runtime_->Start();

  jclass c = env_->FindClass("AllFields");
  ASSERT_TRUE(c != NULL);
  jobject o = env_->AllocObject(c);
  ASSERT_TRUE(o != NULL);

  jstring s1 = env_->NewStringUTF("hello");
  ASSERT_TRUE(s1 != NULL);
  jstring s2 = env_->NewStringUTF("world");
  ASSERT_TRUE(s2 != NULL);

  jfieldID s_fid = env_->GetStaticFieldID(c, "sObject", "Ljava/lang/Object;");
  ASSERT_TRUE(s_fid != NULL);
  jfieldID i_fid = env_->GetFieldID(c, "iObject", "Ljava/lang/Object;");
  ASSERT_TRUE(i_fid != NULL);

  env_->SetStaticObjectField(c, s_fid, s1);
  ASSERT_TRUE(env_->IsSameObject(s1, env_->GetStaticObjectField(c, s_fid)));
  env_->SetStaticObjectField(c, s_fid, s2);
  ASSERT_TRUE(env_->IsSameObject(s2, env_->GetStaticObjectField(c, s_fid)));

  env_->SetObjectField(o, i_fid, s1);
  ASSERT_TRUE(env_->IsSameObject(s1, env_->GetObjectField(o, i_fid)));
  env_->SetObjectField(o, i_fid, s2);
  ASSERT_TRUE(env_->IsSameObject(s2, env_->GetObjectField(o, i_fid)));
}
#endif

TEST_F(JniInternalTest, NewLocalRef_NULL) {
  EXPECT_TRUE(env_->NewLocalRef(NULL) == NULL);
}

TEST_F(JniInternalTest, NewLocalRef) {
  jstring s = env_->NewStringUTF("");
  ASSERT_TRUE(s != NULL);
  jobject o = env_->NewLocalRef(s);
  EXPECT_TRUE(o != NULL);
  EXPECT_TRUE(o != s);

  EXPECT_EQ(JNILocalRefType, env_->GetObjectRefType(o));
}

TEST_F(JniInternalTest, DeleteLocalRef_NULL) {
  env_->DeleteLocalRef(NULL);
}

TEST_F(JniInternalTest, DeleteLocalRef) {
  jstring s = env_->NewStringUTF("");
  ASSERT_TRUE(s != NULL);
  env_->DeleteLocalRef(s);

  // Currently, deleting an already-deleted reference is just a CheckJNI warning.
  {
    CheckJniAbortCatcher check_jni_abort_catcher;
    env_->DeleteLocalRef(s);

    std::string expected(StringPrintf("native code passing in reference to "
                                      "invalid local reference: %p", s));
    check_jni_abort_catcher.Check(expected.c_str());
  }

  s = env_->NewStringUTF("");
  ASSERT_TRUE(s != NULL);
  jobject o = env_->NewLocalRef(s);
  ASSERT_TRUE(o != NULL);

  env_->DeleteLocalRef(s);
  env_->DeleteLocalRef(o);
}

TEST_F(JniInternalTest, PushLocalFrame_10395422) {
  // The JNI specification is ambiguous about whether the given capacity is to be interpreted as a
  // maximum or as a minimum, but it seems like it's supposed to be a minimum, and that's how
  // Android historically treated it, and it's how the RI treats it. It's also the more useful
  // interpretation!
  ASSERT_EQ(JNI_OK, env_->PushLocalFrame(0));
  env_->PopLocalFrame(NULL);

  // Negative capacities are not allowed.
  ASSERT_EQ(JNI_ERR, env_->PushLocalFrame(-1));

  // And it's okay to have an upper limit. Ours is currently 512.
  ASSERT_EQ(JNI_ERR, env_->PushLocalFrame(8192));
}

TEST_F(JniInternalTest, PushLocalFrame_PopLocalFrame) {
  jobject original = env_->NewStringUTF("");
  ASSERT_TRUE(original != NULL);

  jobject outer;
  jobject inner1, inner2;
  ScopedObjectAccess soa(env_);
  mirror::Object* inner2_direct_pointer;
  {
    ASSERT_EQ(JNI_OK, env_->PushLocalFrame(4));
    outer = env_->NewLocalRef(original);

    {
      ASSERT_EQ(JNI_OK, env_->PushLocalFrame(4));
      inner1 = env_->NewLocalRef(outer);
      inner2 = env_->NewStringUTF("survivor");
      inner2_direct_pointer = soa.Decode<mirror::Object*>(inner2);
      env_->PopLocalFrame(inner2);
    }

    EXPECT_EQ(JNILocalRefType, env_->GetObjectRefType(original));
    EXPECT_EQ(JNILocalRefType, env_->GetObjectRefType(outer));
    EXPECT_EQ(JNIInvalidRefType, env_->GetObjectRefType(inner1));

    // Our local reference for the survivor is invalid because the survivor
    // gets a new local reference...
    EXPECT_EQ(JNIInvalidRefType, env_->GetObjectRefType(inner2));
    // ...but the survivor should be in the local reference table.
    JNIEnvExt* env = reinterpret_cast<JNIEnvExt*>(env_);
    EXPECT_TRUE(env->locals.ContainsDirectPointer(inner2_direct_pointer));

    env_->PopLocalFrame(NULL);
  }
  EXPECT_EQ(JNILocalRefType, env_->GetObjectRefType(original));
  EXPECT_EQ(JNIInvalidRefType, env_->GetObjectRefType(outer));
  EXPECT_EQ(JNIInvalidRefType, env_->GetObjectRefType(inner1));
  EXPECT_EQ(JNIInvalidRefType, env_->GetObjectRefType(inner2));
}

TEST_F(JniInternalTest, NewGlobalRef_NULL) {
  EXPECT_TRUE(env_->NewGlobalRef(NULL) == NULL);
}

TEST_F(JniInternalTest, NewGlobalRef) {
  jstring s = env_->NewStringUTF("");
  ASSERT_TRUE(s != NULL);
  jobject o = env_->NewGlobalRef(s);
  EXPECT_TRUE(o != NULL);
  EXPECT_TRUE(o != s);

  // TODO: check that o is a global reference.
}

TEST_F(JniInternalTest, DeleteGlobalRef_NULL) {
  env_->DeleteGlobalRef(NULL);
}

TEST_F(JniInternalTest, DeleteGlobalRef) {
  jstring s = env_->NewStringUTF("");
  ASSERT_TRUE(s != NULL);

  jobject o = env_->NewGlobalRef(s);
  ASSERT_TRUE(o != NULL);
  env_->DeleteGlobalRef(o);

  // Currently, deleting an already-deleted reference is just a CheckJNI warning.
  {
    CheckJniAbortCatcher check_jni_abort_catcher;
    env_->DeleteGlobalRef(o);

    std::string expected(StringPrintf("native code passing in reference to "
                                      "invalid global reference: %p", o));
    check_jni_abort_catcher.Check(expected.c_str());
  }

  jobject o1 = env_->NewGlobalRef(s);
  ASSERT_TRUE(o1 != NULL);
  jobject o2 = env_->NewGlobalRef(s);
  ASSERT_TRUE(o2 != NULL);

  env_->DeleteGlobalRef(o1);
  env_->DeleteGlobalRef(o2);
}

TEST_F(JniInternalTest, NewWeakGlobalRef_NULL) {
  EXPECT_TRUE(env_->NewWeakGlobalRef(NULL) == NULL);
}

TEST_F(JniInternalTest, NewWeakGlobalRef) {
  jstring s = env_->NewStringUTF("");
  ASSERT_TRUE(s != NULL);
  jobject o = env_->NewWeakGlobalRef(s);
  EXPECT_TRUE(o != NULL);
  EXPECT_TRUE(o != s);

  // TODO: check that o is a weak global reference.
}

TEST_F(JniInternalTest, DeleteWeakGlobalRef_NULL) {
  env_->DeleteWeakGlobalRef(NULL);
}

TEST_F(JniInternalTest, DeleteWeakGlobalRef) {
  jstring s = env_->NewStringUTF("");
  ASSERT_TRUE(s != NULL);

  jobject o = env_->NewWeakGlobalRef(s);
  ASSERT_TRUE(o != NULL);
  env_->DeleteWeakGlobalRef(o);

  // Currently, deleting an already-deleted reference is just a CheckJNI warning.
  {
    CheckJniAbortCatcher check_jni_abort_catcher;
    env_->DeleteWeakGlobalRef(o);

    std::string expected(StringPrintf("native code passing in reference to "
                                      "invalid weak global reference: %p", o));
    check_jni_abort_catcher.Check(expected.c_str());
  }

  jobject o1 = env_->NewWeakGlobalRef(s);
  ASSERT_TRUE(o1 != NULL);
  jobject o2 = env_->NewWeakGlobalRef(s);
  ASSERT_TRUE(o2 != NULL);

  env_->DeleteWeakGlobalRef(o1);
  env_->DeleteWeakGlobalRef(o2);
}

TEST_F(JniInternalTest, StaticMainMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  jobject jclass_loader = LoadDex("Main");
  SirtRef<mirror::ClassLoader>
      class_loader(soa.Self(), soa.Decode<mirror::ClassLoader*>(jclass_loader));
  CompileDirectMethod(class_loader.get(), "Main", "main", "([Ljava/lang/String;)V");

  mirror::Class* klass = class_linker_->FindClass("LMain;", class_loader.get());
  ASSERT_TRUE(klass != NULL);

  mirror::ArtMethod* method = klass->FindDirectMethod("main", "([Ljava/lang/String;)V");
  ASSERT_TRUE(method != NULL);

  ArgArray arg_array(NULL, 0);
  arg_array.Append(0);
  JValue result;

  // Start runtime.
  bool started = runtime_->Start();
  CHECK(started);
  Thread::Current()->TransitionFromSuspendedToRunnable();

  method->Invoke(Thread::Current(), arg_array.GetArray(), arg_array.GetNumBytes(), &result, 'V');
}

TEST_F(JniInternalTest, StaticNopMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeNopMethod(true);
}

TEST_F(JniInternalTest, NonStaticNopMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeNopMethod(false);
}

TEST_F(JniInternalTest, StaticIdentityByteMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeIdentityByteMethod(true);
}

TEST_F(JniInternalTest, NonStaticIdentityByteMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeIdentityByteMethod(false);
}

TEST_F(JniInternalTest, StaticIdentityIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeIdentityIntMethod(true);
}

TEST_F(JniInternalTest, NonStaticIdentityIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeIdentityIntMethod(false);
}

TEST_F(JniInternalTest, StaticIdentityDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeIdentityDoubleMethod(true);
}

TEST_F(JniInternalTest, NonStaticIdentityDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeIdentityDoubleMethod(false);
}

TEST_F(JniInternalTest, StaticSumIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumIntIntMethod(true);
}

TEST_F(JniInternalTest, NonStaticSumIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumIntIntMethod(false);
}

TEST_F(JniInternalTest, StaticSumIntIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumIntIntIntMethod(true);
}

TEST_F(JniInternalTest, NonStaticSumIntIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumIntIntIntMethod(false);
}

TEST_F(JniInternalTest, StaticSumIntIntIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumIntIntIntIntMethod(true);
}

TEST_F(JniInternalTest, NonStaticSumIntIntIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumIntIntIntIntMethod(false);
}

TEST_F(JniInternalTest, StaticSumIntIntIntIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumIntIntIntIntIntMethod(true);
}

TEST_F(JniInternalTest, NonStaticSumIntIntIntIntIntMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumIntIntIntIntIntMethod(false);
}

TEST_F(JniInternalTest, StaticSumDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumDoubleDoubleMethod(true);
}

TEST_F(JniInternalTest, NonStaticSumDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumDoubleDoubleMethod(false);
}

TEST_F(JniInternalTest, StaticSumDoubleDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumDoubleDoubleDoubleMethod(true);
}

TEST_F(JniInternalTest, NonStaticSumDoubleDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumDoubleDoubleDoubleMethod(false);
}

TEST_F(JniInternalTest, StaticSumDoubleDoubleDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumDoubleDoubleDoubleDoubleMethod(true);
}

TEST_F(JniInternalTest, NonStaticSumDoubleDoubleDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumDoubleDoubleDoubleDoubleMethod(false);
}

TEST_F(JniInternalTest, StaticSumDoubleDoubleDoubleDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumDoubleDoubleDoubleDoubleDoubleMethod(true);
}

TEST_F(JniInternalTest, NonStaticSumDoubleDoubleDoubleDoubleDoubleMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  ScopedObjectAccess soa(Thread::Current());
  InvokeSumDoubleDoubleDoubleDoubleDoubleMethod(false);
}

TEST_F(JniInternalTest, Throw) {
  EXPECT_EQ(JNI_ERR, env_->Throw(NULL));

  jclass exception_class = env_->FindClass("java/lang/RuntimeException");
  ASSERT_TRUE(exception_class != NULL);
  jthrowable exception = reinterpret_cast<jthrowable>(env_->AllocObject(exception_class));
  ASSERT_TRUE(exception != NULL);

  EXPECT_EQ(JNI_OK, env_->Throw(exception));
  EXPECT_TRUE(env_->ExceptionCheck());
  jthrowable thrown_exception = env_->ExceptionOccurred();
  env_->ExceptionClear();
  EXPECT_TRUE(env_->IsSameObject(exception, thrown_exception));
}

TEST_F(JniInternalTest, ThrowNew) {
  EXPECT_EQ(JNI_ERR, env_->Throw(NULL));

  jclass exception_class = env_->FindClass("java/lang/RuntimeException");
  ASSERT_TRUE(exception_class != NULL);

  jthrowable thrown_exception;

  EXPECT_EQ(JNI_OK, env_->ThrowNew(exception_class, "hello world"));
  EXPECT_TRUE(env_->ExceptionCheck());
  thrown_exception = env_->ExceptionOccurred();
  env_->ExceptionClear();
  EXPECT_TRUE(env_->IsInstanceOf(thrown_exception, exception_class));

  EXPECT_EQ(JNI_OK, env_->ThrowNew(exception_class, NULL));
  EXPECT_TRUE(env_->ExceptionCheck());
  thrown_exception = env_->ExceptionOccurred();
  env_->ExceptionClear();
  EXPECT_TRUE(env_->IsInstanceOf(thrown_exception, exception_class));
}

// TODO: this test is DISABLED until we can actually run java.nio.Buffer's <init>.
TEST_F(JniInternalTest, DISABLED_NewDirectBuffer_GetDirectBufferAddress_GetDirectBufferCapacity) {
  jclass buffer_class = env_->FindClass("java/nio/Buffer");
  ASSERT_TRUE(buffer_class != NULL);

  char bytes[1024];
  jobject buffer = env_->NewDirectByteBuffer(bytes, sizeof(bytes));
  ASSERT_TRUE(buffer != NULL);
  ASSERT_TRUE(env_->IsInstanceOf(buffer, buffer_class));
  ASSERT_TRUE(env_->GetDirectBufferAddress(buffer) == bytes);
  ASSERT_TRUE(env_->GetDirectBufferCapacity(buffer) == sizeof(bytes));
}

TEST_F(JniInternalTest, MonitorEnterExit) {
  // Create an object to torture
  jclass object_class = env_->FindClass("java/lang/Object");
  ASSERT_TRUE(object_class != NULL);
  jobject object = env_->AllocObject(object_class);
  ASSERT_TRUE(object != NULL);

  // Expected class of exceptions
  jclass imse_class = env_->FindClass("java/lang/IllegalMonitorStateException");
  ASSERT_TRUE(imse_class != NULL);

  jthrowable thrown_exception;

  // Unlock of unowned monitor
  env_->MonitorExit(object);
  EXPECT_TRUE(env_->ExceptionCheck());
  thrown_exception = env_->ExceptionOccurred();
  env_->ExceptionClear();
  EXPECT_TRUE(env_->IsInstanceOf(thrown_exception, imse_class));

  // Lock of unowned monitor
  env_->MonitorEnter(object);
  EXPECT_FALSE(env_->ExceptionCheck());
  // Regular unlock
  env_->MonitorExit(object);
  EXPECT_FALSE(env_->ExceptionCheck());

  // Recursively lock a lot
  size_t max_recursive_lock = 1024;
  for (size_t i = 0; i < max_recursive_lock; i++) {
    env_->MonitorEnter(object);
    EXPECT_FALSE(env_->ExceptionCheck());
  }
  // Recursively unlock a lot
  for (size_t i = 0; i < max_recursive_lock; i++) {
    env_->MonitorExit(object);
    EXPECT_FALSE(env_->ExceptionCheck());
  }

  // Unlock of unowned monitor
  env_->MonitorExit(object);
  EXPECT_TRUE(env_->ExceptionCheck());
  thrown_exception = env_->ExceptionOccurred();
  env_->ExceptionClear();
  EXPECT_TRUE(env_->IsInstanceOf(thrown_exception, imse_class));

  // It's an error to call MonitorEnter or MonitorExit on NULL.
  {
    CheckJniAbortCatcher check_jni_abort_catcher;
    env_->MonitorEnter(NULL);
    check_jni_abort_catcher.Check("in call to MonitorEnter");

    env_->MonitorExit(NULL);
    check_jni_abort_catcher.Check("in call to MonitorExit");
  }
}

TEST_F(JniInternalTest, DetachCurrentThread) {
  CleanUpJniEnv();  // cleanup now so TearDown won't have junk from wrong JNIEnv
  jint ok = vm_->DetachCurrentThread();
  EXPECT_EQ(JNI_OK, ok);

  jint err = vm_->DetachCurrentThread();
  EXPECT_EQ(JNI_ERR, err);
  vm_->AttachCurrentThread(&env_, NULL);  // need attached thread for CommonTest::TearDown
}

}  // namespace art
