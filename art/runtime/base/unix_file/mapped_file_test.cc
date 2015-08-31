/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "base/unix_file/mapped_file.h"
#include "base/logging.h"
#include "base/unix_file/fd_file.h"
#include "base/unix_file/random_access_file_test.h"
#include "base/unix_file/random_access_file_utils.h"
#include "base/unix_file/string_file.h"
#include "gtest/gtest.h"

namespace unix_file {

class MappedFileTest : public RandomAccessFileTest {
 protected:
  MappedFileTest() : kContent("some content") {
  }

  void SetUp() {
    art::CommonTest::SetEnvironmentVariables(android_data_);

    good_path_ = GetTmpPath("some-file.txt");
    int fd = TEMP_FAILURE_RETRY(open(good_path_.c_str(), O_CREAT|O_RDWR, 0666));
    FdFile dst(fd);

    StringFile src;
    src.Assign(kContent);

    ASSERT_TRUE(CopyFile(src, &dst));
  }

  virtual RandomAccessFile* MakeTestFile() {
    TEMP_FAILURE_RETRY(truncate(good_path_.c_str(), 0));
    MappedFile* f = new MappedFile;
    CHECK(f->Open(good_path_, MappedFile::kReadWriteMode));
    return f;
  }

  const std::string kContent;
  std::string good_path_;
};

TEST_F(MappedFileTest, OkayToNotUse) {
  MappedFile file;
  EXPECT_EQ(-1, file.Fd());
  EXPECT_FALSE(file.IsOpened());
  EXPECT_FALSE(file.IsMapped());
}

TEST_F(MappedFileTest, OpenClose) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  EXPECT_GE(file.Fd(), 0);
  EXPECT_TRUE(file.IsOpened());
  EXPECT_EQ(kContent.size(), file.size());
  EXPECT_EQ(0, file.Close());
  EXPECT_EQ(-1, file.Fd());
  EXPECT_FALSE(file.IsOpened());
}

TEST_F(MappedFileTest, OpenFdClose) {
  FILE* f = tmpfile();
  ASSERT_TRUE(f != NULL);
  MappedFile file(fileno(f));
  EXPECT_GE(file.Fd(), 0);
  EXPECT_TRUE(file.IsOpened());
  EXPECT_EQ(0, file.Close());
}

TEST_F(MappedFileTest, CanUseAfterMapReadOnly) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  EXPECT_FALSE(file.IsMapped());
  EXPECT_TRUE(file.MapReadOnly());
  EXPECT_TRUE(file.IsMapped());
  EXPECT_EQ(kContent.size(), file.size());
  ASSERT_TRUE(file.data());
  EXPECT_EQ(0, memcmp(kContent.c_str(), file.data(), file.size()));
  EXPECT_EQ(0, file.Flush());
}

TEST_F(MappedFileTest, CanUseAfterMapReadWrite) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadWriteMode));
  EXPECT_FALSE(file.IsMapped());
  EXPECT_TRUE(file.MapReadWrite(1));
  EXPECT_TRUE(file.IsMapped());
  EXPECT_EQ(1, file.size());
  ASSERT_TRUE(file.data());
  EXPECT_EQ(kContent[0], *file.data());
  EXPECT_EQ(0, file.Flush());
}

TEST_F(MappedFileTest, CanWriteNewData) {
  const std::string new_path(GetTmpPath("new-file.txt"));
  ASSERT_EQ(-1, unlink(new_path.c_str()));
  ASSERT_EQ(ENOENT, errno);

  MappedFile file;
  ASSERT_TRUE(file.Open(new_path, MappedFile::kReadWriteMode));
  EXPECT_TRUE(file.MapReadWrite(kContent.size()));
  EXPECT_TRUE(file.IsMapped());
  EXPECT_EQ(kContent.size(), file.size());
  ASSERT_TRUE(file.data());
  memcpy(file.data(), kContent.c_str(), kContent.size());
  EXPECT_EQ(0, file.Close());
  EXPECT_FALSE(file.IsMapped());

  FdFile new_file(TEMP_FAILURE_RETRY(open(new_path.c_str(), O_RDONLY)));
  StringFile buffer;
  ASSERT_TRUE(CopyFile(new_file, &buffer));
  EXPECT_EQ(kContent, buffer.ToStringPiece());
  EXPECT_EQ(0, unlink(new_path.c_str()));
}

TEST_F(MappedFileTest, FileMustExist) {
  const std::string bad_path(GetTmpPath("does-not-exist.txt"));
  MappedFile file;
  EXPECT_FALSE(file.Open(bad_path, MappedFile::kReadOnlyMode));
  EXPECT_EQ(-1, file.Fd());
}

TEST_F(MappedFileTest, FileMustBeWritable) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  EXPECT_FALSE(file.MapReadWrite(10));
}

TEST_F(MappedFileTest, RemappingAllowedUntilSuccess) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  EXPECT_FALSE(file.MapReadWrite(10));
  EXPECT_FALSE(file.MapReadWrite(10));
}

TEST_F(MappedFileTest, ResizeMappedFile) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadWriteMode));
  ASSERT_TRUE(file.MapReadWrite(10));
  EXPECT_EQ(10, file.GetLength());
  EXPECT_TRUE(file.Unmap());
  EXPECT_TRUE(file.MapReadWrite(20));
  EXPECT_EQ(20, file.GetLength());
  EXPECT_EQ(0, file.Flush());
  EXPECT_TRUE(file.Unmap());
  EXPECT_EQ(0, file.Flush());
  EXPECT_EQ(0, file.SetLength(5));
  EXPECT_TRUE(file.MapReadOnly());
  EXPECT_EQ(5, file.GetLength());
}

TEST_F(MappedFileTest, ReadNotMapped) {
  TestRead();
}

TEST_F(MappedFileTest, SetLengthNotMapped) {
  TestSetLength();
}

TEST_F(MappedFileTest, WriteNotMapped) {
  TestWrite();
}

TEST_F(MappedFileTest, ReadMappedReadOnly) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  ASSERT_TRUE(file.MapReadOnly());
  TestReadContent(kContent, &file);
}

TEST_F(MappedFileTest, ReadMappedReadWrite) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadWriteMode));
  ASSERT_TRUE(file.MapReadWrite(kContent.size()));
  TestReadContent(kContent, &file);
}

TEST_F(MappedFileTest, WriteMappedReadWrite) {
  TEMP_FAILURE_RETRY(unlink(good_path_.c_str()));
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadWriteMode));
  ASSERT_TRUE(file.MapReadWrite(kContent.size()));

  // Can't write to a negative offset.
  EXPECT_EQ(-EINVAL, file.Write(kContent.c_str(), 0, -123));

  // A zero-length write is a no-op.
  EXPECT_EQ(0, file.Write(kContent.c_str(), 0, 0));
  // But the file size is as given when mapped.
  EXPECT_EQ(kContent.size(), file.GetLength());

  // Data written past the end are discarded.
  EXPECT_EQ(kContent.size() - 1,
            file.Write(kContent.c_str(), kContent.size(), 1));
  EXPECT_EQ(0, memcmp(kContent.c_str(), file.data() + 1, kContent.size() - 1));

  // Data can be overwritten.
  EXPECT_EQ(kContent.size(), file.Write(kContent.c_str(), kContent.size(), 0));
  EXPECT_EQ(0, memcmp(kContent.c_str(), file.data(), kContent.size()));
}

#if 0  // death tests don't work on android yet

class MappedFileDeathTest : public MappedFileTest {};

TEST_F(MappedFileDeathTest, MustMapBeforeUse) {
  MappedFile file;
  EXPECT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  EXPECT_DEATH(file.data(), "mapped_");
}

TEST_F(MappedFileDeathTest, RemappingNotAllowedReadOnly) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  ASSERT_TRUE(file.MapReadOnly());
  EXPECT_DEATH(file.MapReadOnly(), "mapped_");
}

TEST_F(MappedFileDeathTest, RemappingNotAllowedReadWrite) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadWriteMode));
  ASSERT_TRUE(file.MapReadWrite(10));
  EXPECT_DEATH(file.MapReadWrite(10), "mapped_");
}

TEST_F(MappedFileDeathTest, SetLengthMappedReadWrite) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadWriteMode));
  ASSERT_TRUE(file.MapReadWrite(10));
  EXPECT_EQ(10, file.GetLength());
  EXPECT_DEATH(file.SetLength(0), ".*");
}

TEST_F(MappedFileDeathTest, SetLengthMappedReadOnly) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  ASSERT_TRUE(file.MapReadOnly());
  EXPECT_EQ(kContent.size(), file.GetLength());
  EXPECT_DEATH(file.SetLength(0), ".*");
}

TEST_F(MappedFileDeathTest, WriteMappedReadOnly) {
  MappedFile file;
  ASSERT_TRUE(file.Open(good_path_, MappedFile::kReadOnlyMode));
  ASSERT_TRUE(file.MapReadOnly());
  char buf[10];
  EXPECT_DEATH(file.Write(buf, 0, 0), ".*");
}

#endif

}  // namespace unix_file
