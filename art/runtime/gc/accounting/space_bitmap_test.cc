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

#include "space_bitmap.h"

#include "common_test.h"
#include "globals.h"
#include "space_bitmap-inl.h"
#include "UniquePtr.h"

#include <stdint.h>

namespace art {
namespace gc {
namespace accounting {

class SpaceBitmapTest : public CommonTest {
 public:
};

TEST_F(SpaceBitmapTest, Init) {
  byte* heap_begin = reinterpret_cast<byte*>(0x10000000);
  size_t heap_capacity = 16 * MB;
  UniquePtr<SpaceBitmap> space_bitmap(SpaceBitmap::Create("test bitmap",
                                                          heap_begin, heap_capacity));
  EXPECT_TRUE(space_bitmap.get() != NULL);
}

class BitmapVerify {
 public:
  BitmapVerify(SpaceBitmap* bitmap, const mirror::Object* begin, const mirror::Object* end)
    : bitmap_(bitmap),
      begin_(begin),
      end_(end) {}

  void operator()(const mirror::Object* obj) {
    EXPECT_TRUE(obj >= begin_);
    EXPECT_TRUE(obj <= end_);
    EXPECT_EQ(bitmap_->Test(obj), ((reinterpret_cast<uintptr_t>(obj) & 0xF) != 0));
  }

  SpaceBitmap* bitmap_;
  const mirror::Object* begin_;
  const mirror::Object* end_;
};

TEST_F(SpaceBitmapTest, ScanRange) {
  byte* heap_begin = reinterpret_cast<byte*>(0x10000000);
  size_t heap_capacity = 16 * MB;

  UniquePtr<SpaceBitmap> space_bitmap(SpaceBitmap::Create("test bitmap",
                                                          heap_begin, heap_capacity));
  EXPECT_TRUE(space_bitmap.get() != NULL);

  // Set all the odd bits in the first BitsPerWord * 3 to one.
  for (size_t j = 0; j < kBitsPerWord * 3; ++j) {
    const mirror::Object* obj =
        reinterpret_cast<mirror::Object*>(heap_begin + j * SpaceBitmap::kAlignment);
    if (reinterpret_cast<uintptr_t>(obj) & 0xF) {
      space_bitmap->Set(obj);
    }
  }
  // Try every possible starting bit in the first word. Then for each starting bit, try each
  // possible length up to a maximum of kBitsPerWord * 2 - 1 bits.
  // This handles all the cases, having runs which start and end on the same word, and different
  // words.
  for (size_t i = 0; i < static_cast<size_t>(kBitsPerWord); ++i) {
    mirror::Object* start =
        reinterpret_cast<mirror::Object*>(heap_begin + i * SpaceBitmap::kAlignment);
    for (size_t j = 0; j < static_cast<size_t>(kBitsPerWord * 2); ++j) {
      mirror::Object* end =
          reinterpret_cast<mirror::Object*>(heap_begin + (i + j) * SpaceBitmap::kAlignment);
      BitmapVerify(space_bitmap.get(), start, end);
    }
  }
}

}  // namespace accounting
}  // namespace gc
}  // namespace art
