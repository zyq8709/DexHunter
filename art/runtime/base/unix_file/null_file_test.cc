/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include "base/unix_file/null_file.h"

#include <errno.h>

#include "gtest/gtest.h"

namespace unix_file {

class NullFileTest : public testing::Test { };

TEST_F(NullFileTest, Read) {
  NullFile f;
  char buf[256];
  // You can't read a negative number of bytes...
  ASSERT_EQ(-EINVAL, f.Read(buf, 0, -1));
  // ...but everything else is fine (though you'll get no data).
  ASSERT_EQ(0, f.Read(buf, 128, 0));
  ASSERT_EQ(0, f.Read(buf, 128, 128));
}

TEST_F(NullFileTest, SetLength) {
  NullFile f;
  // You can't set a negative length...
  ASSERT_EQ(-EINVAL, f.SetLength(-1));
  // ...but everything else is fine.
  ASSERT_EQ(0, f.SetLength(0));
  ASSERT_EQ(0, f.SetLength(128));
}

TEST_F(NullFileTest, GetLength) {
  const std::string content("hello");
  NullFile f;
  // The length is always 0.
  ASSERT_EQ(0, f.GetLength());
  ASSERT_EQ(content.size(), f.Write(content.data(), content.size(), 0));
  ASSERT_EQ(0, f.GetLength());
}

TEST_F(NullFileTest, Write) {
  const std::string content("hello");
  NullFile f;
  // You can't write at a negative offset...
  ASSERT_EQ(-EINVAL, f.Write(content.data(), content.size(), -128));
  // But you can write anywhere else...
  ASSERT_EQ(content.size(), f.Write(content.data(), content.size(), 0));
  ASSERT_EQ(content.size(), f.Write(content.data(), content.size(), 128));
  // ...though the file will remain empty.
  ASSERT_EQ(0, f.GetLength());
}

}  // namespace unix_file
