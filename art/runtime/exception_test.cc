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
#include "leb128_encoder.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "mirror/stack_trace_element.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "sirt_ref.h"
#include "thread.h"
#include "UniquePtr.h"

namespace art {

class ExceptionTest : public CommonTest {
 protected:
  virtual void SetUp() {
    CommonTest::SetUp();

    ScopedObjectAccess soa(Thread::Current());
    SirtRef<mirror::ClassLoader> class_loader(soa.Self(),
                                      soa.Decode<mirror::ClassLoader*>(LoadDex("ExceptionHandle")));
    my_klass_ = class_linker_->FindClass("LExceptionHandle;", class_loader.get());
    ASSERT_TRUE(my_klass_ != NULL);
    class_linker_->EnsureInitialized(my_klass_, true, true);

    dex_ = my_klass_->GetDexCache()->GetDexFile();

    uint32_t code_size = 12;
    fake_code_.push_back((code_size >> 24) & 0xFF);
    fake_code_.push_back((code_size >> 16) & 0xFF);
    fake_code_.push_back((code_size >>  8) & 0xFF);
    fake_code_.push_back((code_size >>  0) & 0xFF);
    for (size_t i = 0 ; i < code_size; i++) {
      fake_code_.push_back(0x70 | i);
    }

    fake_mapping_data_.PushBack(4);  // first element is count
    fake_mapping_data_.PushBack(4);  // total (non-length) elements
    fake_mapping_data_.PushBack(2);  // count of pc to dex elements
                                      // ---  pc to dex table
    fake_mapping_data_.PushBack(3);  // offset 3
    fake_mapping_data_.PushBack(3);  // maps to dex offset 3
                                      // ---  dex to pc table
    fake_mapping_data_.PushBack(3);  // offset 3
    fake_mapping_data_.PushBack(3);  // maps to dex offset 3

    fake_vmap_table_data_.PushBack(0);

    fake_gc_map_.push_back(0);  // 0 bytes to encode references and native pc offsets.
    fake_gc_map_.push_back(0);
    fake_gc_map_.push_back(0);  // 0 entries.
    fake_gc_map_.push_back(0);

    method_f_ = my_klass_->FindVirtualMethod("f", "()I");
    ASSERT_TRUE(method_f_ != NULL);
    method_f_->SetFrameSizeInBytes(kStackAlignment);
    method_f_->SetEntryPointFromCompiledCode(CompiledMethod::CodePointer(&fake_code_[sizeof(code_size)], kThumb2));
    method_f_->SetMappingTable(&fake_mapping_data_.GetData()[0]);
    method_f_->SetVmapTable(&fake_vmap_table_data_.GetData()[0]);
    method_f_->SetNativeGcMap(&fake_gc_map_[0]);

    method_g_ = my_klass_->FindVirtualMethod("g", "(I)V");
    ASSERT_TRUE(method_g_ != NULL);
    method_g_->SetFrameSizeInBytes(kStackAlignment);
    method_g_->SetEntryPointFromCompiledCode(CompiledMethod::CodePointer(&fake_code_[sizeof(code_size)], kThumb2));
    method_g_->SetMappingTable(&fake_mapping_data_.GetData()[0]);
    method_g_->SetVmapTable(&fake_vmap_table_data_.GetData()[0]);
    method_g_->SetNativeGcMap(&fake_gc_map_[0]);
  }

  const DexFile* dex_;

  std::vector<uint8_t> fake_code_;
  UnsignedLeb128EncodingVector fake_mapping_data_;
  UnsignedLeb128EncodingVector fake_vmap_table_data_;
  std::vector<uint8_t> fake_gc_map_;

  mirror::ArtMethod* method_f_;
  mirror::ArtMethod* method_g_;

 private:
  mirror::Class* my_klass_;
};

TEST_F(ExceptionTest, FindCatchHandler) {
  const DexFile::CodeItem* code_item = dex_->GetCodeItem(method_f_->GetCodeItemOffset());

  ASSERT_TRUE(code_item != NULL);

  ASSERT_EQ(2u, code_item->tries_size_);
  ASSERT_NE(0u, code_item->insns_size_in_code_units_);

  const DexFile::TryItem *t0, *t1;
  t0 = dex_->GetTryItems(*code_item, 0);
  t1 = dex_->GetTryItems(*code_item, 1);
  EXPECT_LE(t0->start_addr_, t1->start_addr_);
  {
    CatchHandlerIterator iter(*code_item, 4 /* Dex PC in the first try block */);
    EXPECT_STREQ("Ljava/io/IOException;", dex_->StringByTypeIdx(iter.GetHandlerTypeIndex()));
    ASSERT_TRUE(iter.HasNext());
    iter.Next();
    EXPECT_STREQ("Ljava/lang/Exception;", dex_->StringByTypeIdx(iter.GetHandlerTypeIndex()));
    ASSERT_TRUE(iter.HasNext());
    iter.Next();
    EXPECT_FALSE(iter.HasNext());
  }
  {
    CatchHandlerIterator iter(*code_item, 8 /* Dex PC in the second try block */);
    EXPECT_STREQ("Ljava/io/IOException;", dex_->StringByTypeIdx(iter.GetHandlerTypeIndex()));
    ASSERT_TRUE(iter.HasNext());
    iter.Next();
    EXPECT_FALSE(iter.HasNext());
  }
  {
    CatchHandlerIterator iter(*code_item, 11 /* Dex PC not in any try block */);
    EXPECT_FALSE(iter.HasNext());
  }
}

TEST_F(ExceptionTest, StackTraceElement) {
  Thread* thread = Thread::Current();
  thread->TransitionFromSuspendedToRunnable();
  bool started = runtime_->Start();
  CHECK(started);
  JNIEnv* env = thread->GetJniEnv();
  ScopedObjectAccess soa(env);

  std::vector<uintptr_t> fake_stack;
  ASSERT_EQ(kStackAlignment, 16);
  ASSERT_EQ(sizeof(uintptr_t), sizeof(uint32_t));

#if !defined(ART_USE_PORTABLE_COMPILER)
  // Create two fake stack frames with mapping data created in SetUp. We map offset 3 in the code
  // to dex pc 3.
  const uint32_t dex_pc = 3;

  // Create/push fake 16byte stack frame for method g
  fake_stack.push_back(reinterpret_cast<uintptr_t>(method_g_));
  fake_stack.push_back(0);
  fake_stack.push_back(0);
  fake_stack.push_back(method_f_->ToNativePc(dex_pc));  // return pc

  // Create/push fake 16byte stack frame for method f
  fake_stack.push_back(reinterpret_cast<uintptr_t>(method_f_));
  fake_stack.push_back(0);
  fake_stack.push_back(0);
  fake_stack.push_back(0xEBAD6070);  // return pc

  // Pull Method* of NULL to terminate the trace
  fake_stack.push_back(0);

  // Push null values which will become null incoming arguments.
  fake_stack.push_back(0);
  fake_stack.push_back(0);
  fake_stack.push_back(0);

  // Set up thread to appear as if we called out of method_g_ at pc dex 3
  thread->SetTopOfStack(&fake_stack[0], method_g_->ToNativePc(dex_pc));  // return pc
#else
  // Create/push fake 20-byte shadow frame for method g
  fake_stack.push_back(0);
  fake_stack.push_back(0);
  fake_stack.push_back(reinterpret_cast<uintptr_t>(method_g_));
  fake_stack.push_back(3);
  fake_stack.push_back(0);

  // Create/push fake 20-byte shadow frame for method f
  fake_stack.push_back(0);
  fake_stack.push_back(0);
  fake_stack.push_back(reinterpret_cast<uintptr_t>(method_f_));
  fake_stack.push_back(3);
  fake_stack.push_back(0);

  thread->PushShadowFrame(reinterpret_cast<ShadowFrame*>(&fake_stack[5]));
  thread->PushShadowFrame(reinterpret_cast<ShadowFrame*>(&fake_stack[0]));
#endif

  jobject internal = thread->CreateInternalStackTrace(soa);
  ASSERT_TRUE(internal != NULL);
  jobjectArray ste_array = Thread::InternalStackTraceToStackTraceElementArray(env, internal);
  ASSERT_TRUE(ste_array != NULL);
  mirror::ObjectArray<mirror::StackTraceElement>* trace_array =
      soa.Decode<mirror::ObjectArray<mirror::StackTraceElement>*>(ste_array);

  ASSERT_TRUE(trace_array != NULL);
  ASSERT_TRUE(trace_array->Get(0) != NULL);
  EXPECT_STREQ("ExceptionHandle",
               trace_array->Get(0)->GetDeclaringClass()->ToModifiedUtf8().c_str());
  EXPECT_STREQ("ExceptionHandle.java", trace_array->Get(0)->GetFileName()->ToModifiedUtf8().c_str());
  EXPECT_STREQ("g", trace_array->Get(0)->GetMethodName()->ToModifiedUtf8().c_str());
  EXPECT_EQ(37, trace_array->Get(0)->GetLineNumber());

  ASSERT_TRUE(trace_array->Get(1) != NULL);
  EXPECT_STREQ("ExceptionHandle",
               trace_array->Get(1)->GetDeclaringClass()->ToModifiedUtf8().c_str());
  EXPECT_STREQ("ExceptionHandle.java", trace_array->Get(1)->GetFileName()->ToModifiedUtf8().c_str());
  EXPECT_STREQ("f", trace_array->Get(1)->GetMethodName()->ToModifiedUtf8().c_str());
  EXPECT_EQ(22, trace_array->Get(1)->GetLineNumber());

#if !defined(ART_USE_PORTABLE_COMPILER)
  thread->SetTopOfStack(NULL, 0);  // Disarm the assertion that no code is running when we detach.
#else
  thread->PopShadowFrame();
  thread->PopShadowFrame();
#endif
}

}  // namespace art
