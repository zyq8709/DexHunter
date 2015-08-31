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

#include "common_test.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "sirt_ref.h"

namespace art {
namespace gc {

class HeapTest : public CommonTest {};

TEST_F(HeapTest, ClearGrowthLimit) {
  Heap* heap = Runtime::Current()->GetHeap();
  int64_t max_memory_before = heap->GetMaxMemory();
  int64_t total_memory_before = heap->GetTotalMemory();
  heap->ClearGrowthLimit();
  int64_t max_memory_after = heap->GetMaxMemory();
  int64_t total_memory_after = heap->GetTotalMemory();
  EXPECT_GE(max_memory_after, max_memory_before);
  EXPECT_GE(total_memory_after, total_memory_before);
}

TEST_F(HeapTest, GarbageCollectClassLinkerInit) {
  {
    ScopedObjectAccess soa(Thread::Current());
    // garbage is created during ClassLinker::Init

    mirror::Class* c = class_linker_->FindSystemClass("[Ljava/lang/Object;");
    for (size_t i = 0; i < 1024; ++i) {
      SirtRef<mirror::ObjectArray<mirror::Object> > array(soa.Self(),
          mirror::ObjectArray<mirror::Object>::Alloc(soa.Self(), c, 2048));
      for (size_t j = 0; j < 2048; ++j) {
        array->Set(j, mirror::String::AllocFromModifiedUtf8(soa.Self(), "hello, world!"));
      }
    }
  }
  Runtime::Current()->GetHeap()->CollectGarbage(false);
}

TEST_F(HeapTest, HeapBitmapCapacityTest) {
  byte* heap_begin = reinterpret_cast<byte*>(0x1000);
  const size_t heap_capacity = accounting::SpaceBitmap::kAlignment * (sizeof(intptr_t) * 8 + 1);
  UniquePtr<accounting::SpaceBitmap> bitmap(accounting::SpaceBitmap::Create("test bitmap",
                                                                            heap_begin,
                                                                            heap_capacity));
  mirror::Object* fake_end_of_heap_object =
      reinterpret_cast<mirror::Object*>(&heap_begin[heap_capacity -
                                                    accounting::SpaceBitmap::kAlignment]);
  bitmap->Set(fake_end_of_heap_object);
}

}  // namespace gc
}  // namespace art
