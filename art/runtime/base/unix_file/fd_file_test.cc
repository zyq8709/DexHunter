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

#include "base/unix_file/fd_file.h"
#include "base/unix_file/random_access_file_test.h"
#include "gtest/gtest.h"

namespace unix_file {

class FdFileTest : public RandomAccessFileTest {
 protected:
  virtual RandomAccessFile* MakeTestFile() {
    return new FdFile(fileno(tmpfile()));
  }
};

TEST_F(FdFileTest, Read) {
  TestRead();
}

TEST_F(FdFileTest, SetLength) {
  TestSetLength();
}

TEST_F(FdFileTest, Write) {
  TestWrite();
}

TEST_F(FdFileTest, UnopenedFile) {
  FdFile file;
  EXPECT_EQ(-1, file.Fd());
  EXPECT_FALSE(file.IsOpened());
  EXPECT_TRUE(file.GetPath().empty());
}

TEST_F(FdFileTest, OpenClose) {
  std::string good_path(GetTmpPath("some-file.txt"));
  FdFile file;
  ASSERT_TRUE(file.Open(good_path, O_CREAT | O_WRONLY));
  EXPECT_GE(file.Fd(), 0);
  EXPECT_TRUE(file.IsOpened());
  EXPECT_EQ(0, file.Close());
  EXPECT_EQ(-1, file.Fd());
  EXPECT_FALSE(file.IsOpened());
  EXPECT_TRUE(file.Open(good_path,  O_RDONLY));
  EXPECT_GE(file.Fd(), 0);
  EXPECT_TRUE(file.IsOpened());
}

}  // namespace unix_file
