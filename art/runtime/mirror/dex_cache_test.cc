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
#include "dex_cache.h"
#include "gc/heap.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "sirt_ref.h"

#include <stdio.h>

namespace art {
namespace mirror {

class DexCacheTest : public CommonTest {};

TEST_F(DexCacheTest, Open) {
  ScopedObjectAccess soa(Thread::Current());
  SirtRef<DexCache> dex_cache(soa.Self(), class_linker_->AllocDexCache(soa.Self(),
                                                                       *java_lang_dex_file_));
  ASSERT_TRUE(dex_cache.get() != NULL);

  EXPECT_EQ(java_lang_dex_file_->NumStringIds(), dex_cache->NumStrings());
  EXPECT_EQ(java_lang_dex_file_->NumTypeIds(),   dex_cache->NumResolvedTypes());
  EXPECT_EQ(java_lang_dex_file_->NumMethodIds(), dex_cache->NumResolvedMethods());
  EXPECT_EQ(java_lang_dex_file_->NumFieldIds(),  dex_cache->NumResolvedFields());
  EXPECT_EQ(java_lang_dex_file_->NumTypeIds(),   dex_cache->NumInitializedStaticStorage());

  EXPECT_LE(0, dex_cache->GetStrings()->GetLength());
  EXPECT_LE(0, dex_cache->GetResolvedTypes()->GetLength());
  EXPECT_LE(0, dex_cache->GetResolvedMethods()->GetLength());
  EXPECT_LE(0, dex_cache->GetResolvedFields()->GetLength());
  EXPECT_LE(0, dex_cache->GetInitializedStaticStorage()->GetLength());

  EXPECT_EQ(java_lang_dex_file_->NumStringIds(),
            static_cast<uint32_t>(dex_cache->GetStrings()->GetLength()));
  EXPECT_EQ(java_lang_dex_file_->NumTypeIds(),
            static_cast<uint32_t>(dex_cache->GetResolvedTypes()->GetLength()));
  EXPECT_EQ(java_lang_dex_file_->NumMethodIds(),
            static_cast<uint32_t>(dex_cache->GetResolvedMethods()->GetLength()));
  EXPECT_EQ(java_lang_dex_file_->NumFieldIds(),
            static_cast<uint32_t>(dex_cache->GetResolvedFields()->GetLength()));
  EXPECT_EQ(java_lang_dex_file_->NumTypeIds(),
            static_cast<uint32_t>(dex_cache->GetInitializedStaticStorage()->GetLength()));
}

}  // namespace mirror
}  // namespace art
