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

#include "math_entrypoints.h"

#include "common_test.h"
#include <limits>

namespace art {

class MathEntrypointsTest : public CommonTest {};

TEST_F(MathEntrypointsTest, DoubleToLong) {
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), art_d2l(1.85e19));
  EXPECT_EQ(std::numeric_limits<int64_t>::min(), art_d2l(-1.85e19));
  EXPECT_EQ(0LL, art_d2l(0));
  EXPECT_EQ(1LL, art_d2l(1.0));
  EXPECT_EQ(10LL, art_d2l(10.0));
  EXPECT_EQ(100LL, art_d2l(100.0));
  EXPECT_EQ(-1LL, art_d2l(-1.0));
  EXPECT_EQ(-10LL, art_d2l(-10.0));
  EXPECT_EQ(-100LL, art_d2l(-100.0));
}

TEST_F(MathEntrypointsTest, FloatToLong) {
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), art_f2l(1.85e19));
  EXPECT_EQ(std::numeric_limits<int64_t>::min(), art_f2l(-1.85e19));
  EXPECT_EQ(0LL, art_f2l(0));
  EXPECT_EQ(1LL, art_f2l(1.0));
  EXPECT_EQ(10LL, art_f2l(10.0));
  EXPECT_EQ(100LL, art_f2l(100.0));
  EXPECT_EQ(-1LL, art_f2l(-1.0));
  EXPECT_EQ(-10LL, art_f2l(-10.0));
  EXPECT_EQ(-100LL, art_f2l(-100.0));
}

TEST_F(MathEntrypointsTest, DoubleToInt) {
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), art_d2i(4.3e9));
  EXPECT_EQ(std::numeric_limits<int32_t>::min(), art_d2i(-4.3e9));
  EXPECT_EQ(0L, art_d2i(0));
  EXPECT_EQ(1L, art_d2i(1.0));
  EXPECT_EQ(10L, art_d2i(10.0));
  EXPECT_EQ(100L, art_d2i(100.0));
  EXPECT_EQ(-1L, art_d2i(-1.0));
  EXPECT_EQ(-10L, art_d2i(-10.0));
  EXPECT_EQ(-100L, art_d2i(-100.0));
}

TEST_F(MathEntrypointsTest, FloatToInt) {
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), art_f2i(4.3e9));
  EXPECT_EQ(std::numeric_limits<int32_t>::min(), art_f2i(-4.3e9));
  EXPECT_EQ(0L, art_f2i(0));
  EXPECT_EQ(1L, art_f2i(1.0));
  EXPECT_EQ(10L, art_f2i(10.0));
  EXPECT_EQ(100L, art_f2i(100.0));
  EXPECT_EQ(-1L, art_f2i(-1.0));
  EXPECT_EQ(-10L, art_f2i(-10.0));
  EXPECT_EQ(-100L, art_f2i(-100.0));
}

}  // namespace art
