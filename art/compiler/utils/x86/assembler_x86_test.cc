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

#include "assembler_x86.h"

#include "gtest/gtest.h"

namespace art {

TEST(AssemblerX86, CreateBuffer) {
  AssemblerBuffer buffer;
  AssemblerBuffer::EnsureCapacity ensured(&buffer);
  buffer.Emit<uint8_t>(0x42);
  ASSERT_EQ(static_cast<size_t>(1), buffer.Size());
  buffer.Emit<int32_t>(42);
  ASSERT_EQ(static_cast<size_t>(5), buffer.Size());
}

}  // namespace art
