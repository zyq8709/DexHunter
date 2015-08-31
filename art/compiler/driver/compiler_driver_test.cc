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

#include "driver/compiler_driver.h"

#include <stdint.h>
#include <stdio.h>

#include "UniquePtr.h"
#include "class_linker.h"
#include "common_test.h"
#include "dex_file.h"
#include "gc/heap.h"
#include "mirror/art_method-inl.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"

namespace art {

class CompilerDriverTest : public CommonTest {
 protected:
  void CompileAll(jobject class_loader) LOCKS_EXCLUDED(Locks::mutator_lock_) {
    base::TimingLogger timings("CompilerDriverTest::CompileAll", false, false);
    timings.StartSplit("CompileAll");
    compiler_driver_->CompileAll(class_loader,
                                 Runtime::Current()->GetCompileTimeClassPath(class_loader),
                                 timings);
    MakeAllExecutable(class_loader);
  }

  void EnsureCompiled(jobject class_loader, const char* class_name, const char* method,
                      const char* signature, bool is_virtual)
      LOCKS_EXCLUDED(Locks::mutator_lock_) {
    CompileAll(class_loader);
    Thread::Current()->TransitionFromSuspendedToRunnable();
    bool started = runtime_->Start();
    CHECK(started);
    env_ = Thread::Current()->GetJniEnv();
    class_ = env_->FindClass(class_name);
    CHECK(class_ != NULL) << "Class not found: " << class_name;
    if (is_virtual) {
      mid_ = env_->GetMethodID(class_, method, signature);
    } else {
      mid_ = env_->GetStaticMethodID(class_, method, signature);
    }
    CHECK(mid_ != NULL) << "Method not found: " << class_name << "." << method << signature;
  }

  void MakeAllExecutable(jobject class_loader) {
    const std::vector<const DexFile*>& class_path
        = Runtime::Current()->GetCompileTimeClassPath(class_loader);
    for (size_t i = 0; i != class_path.size(); ++i) {
      const DexFile* dex_file = class_path[i];
      CHECK(dex_file != NULL);
      MakeDexFileExecutable(class_loader, *dex_file);
    }
  }

  void MakeDexFileExecutable(jobject class_loader, const DexFile& dex_file) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    for (size_t i = 0; i < dex_file.NumClassDefs(); i++) {
      const DexFile::ClassDef& class_def = dex_file.GetClassDef(i);
      const char* descriptor = dex_file.GetClassDescriptor(class_def);
      ScopedObjectAccess soa(Thread::Current());
      mirror::Class* c = class_linker->FindClass(descriptor, soa.Decode<mirror::ClassLoader*>(class_loader));
      CHECK(c != NULL);
      for (size_t i = 0; i < c->NumDirectMethods(); i++) {
        MakeExecutable(c->GetDirectMethod(i));
      }
      for (size_t i = 0; i < c->NumVirtualMethods(); i++) {
        MakeExecutable(c->GetVirtualMethod(i));
      }
    }
  }

  JNIEnv* env_;
  jclass class_;
  jmethodID mid_;
};

// Disabled due to 10 second runtime on host
TEST_F(CompilerDriverTest, DISABLED_LARGE_CompileDexLibCore) {
  CompileAll(NULL);

  // All libcore references should resolve
  ScopedObjectAccess soa(Thread::Current());
  const DexFile* dex = java_lang_dex_file_;
  mirror::DexCache* dex_cache = class_linker_->FindDexCache(*dex);
  EXPECT_EQ(dex->NumStringIds(), dex_cache->NumStrings());
  for (size_t i = 0; i < dex_cache->NumStrings(); i++) {
    const mirror::String* string = dex_cache->GetResolvedString(i);
    EXPECT_TRUE(string != NULL) << "string_idx=" << i;
  }
  EXPECT_EQ(dex->NumTypeIds(), dex_cache->NumResolvedTypes());
  for (size_t i = 0; i < dex_cache->NumResolvedTypes(); i++) {
    mirror::Class* type = dex_cache->GetResolvedType(i);
    EXPECT_TRUE(type != NULL) << "type_idx=" << i
                              << " " << dex->GetTypeDescriptor(dex->GetTypeId(i));
  }
  EXPECT_EQ(dex->NumMethodIds(), dex_cache->NumResolvedMethods());
  for (size_t i = 0; i < dex_cache->NumResolvedMethods(); i++) {
    mirror::ArtMethod* method = dex_cache->GetResolvedMethod(i);
    EXPECT_TRUE(method != NULL) << "method_idx=" << i
                                << " " << dex->GetMethodDeclaringClassDescriptor(dex->GetMethodId(i))
                                << " " << dex->GetMethodName(dex->GetMethodId(i));
    EXPECT_TRUE(method->GetEntryPointFromCompiledCode() != NULL) << "method_idx=" << i
                                           << " "
                                           << dex->GetMethodDeclaringClassDescriptor(dex->GetMethodId(i))
                                           << " " << dex->GetMethodName(dex->GetMethodId(i));
  }
  EXPECT_EQ(dex->NumFieldIds(), dex_cache->NumResolvedFields());
  for (size_t i = 0; i < dex_cache->NumResolvedFields(); i++) {
    mirror::ArtField* field = dex_cache->GetResolvedField(i);
    EXPECT_TRUE(field != NULL) << "field_idx=" << i
                               << " " << dex->GetFieldDeclaringClassDescriptor(dex->GetFieldId(i))
                               << " " << dex->GetFieldName(dex->GetFieldId(i));
  }

  // TODO check Class::IsVerified for all classes

  // TODO: check that all Method::GetCode() values are non-null
}

TEST_F(CompilerDriverTest, AbstractMethodErrorStub) {
  TEST_DISABLED_FOR_PORTABLE();
  jobject class_loader;
  {
    ScopedObjectAccess soa(Thread::Current());
    CompileVirtualMethod(NULL, "java.lang.Class", "isFinalizable", "()Z");
    CompileDirectMethod(NULL, "java.lang.Object", "<init>", "()V");
    class_loader = LoadDex("AbstractMethod");
  }
  ASSERT_TRUE(class_loader != NULL);
  EnsureCompiled(class_loader, "AbstractClass", "foo", "()V", true);

  // Create a jobj_ of ConcreteClass, NOT AbstractClass.
  jclass c_class = env_->FindClass("ConcreteClass");
  jmethodID constructor = env_->GetMethodID(c_class, "<init>", "()V");
  jobject jobj_ = env_->NewObject(c_class, constructor);
  ASSERT_TRUE(jobj_ != NULL);

  // Force non-virtual call to AbstractClass foo, will throw AbstractMethodError exception.
  env_->CallNonvirtualVoidMethod(jobj_, class_, mid_);
  EXPECT_EQ(env_->ExceptionCheck(), JNI_TRUE);
  jthrowable exception = env_->ExceptionOccurred();
  env_->ExceptionClear();
  jclass jlame = env_->FindClass("java/lang/AbstractMethodError");
  EXPECT_TRUE(env_->IsInstanceOf(exception, jlame));
  Thread::Current()->ClearException();
}

// TODO: need check-cast test (when stub complete & we can throw/catch

}  // namespace art
