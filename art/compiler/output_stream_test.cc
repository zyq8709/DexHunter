/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "base/logging.h"
#include "buffered_output_stream.h"
#include "common_test.h"
#include "file_output_stream.h"
#include "vector_output_stream.h"

namespace art {

class OutputStreamTest : public CommonTest {
 protected:
  void CheckOffset(off_t expected) {
    off_t actual = output_stream_->Seek(0, kSeekCurrent);
    EXPECT_EQ(expected, actual);
  }

  void SetOutputStream(OutputStream& output_stream) {
    output_stream_ = &output_stream;
  }

  void GenerateTestOutput() {
    EXPECT_EQ(3, output_stream_->Seek(3, kSeekCurrent));
    CheckOffset(3);
    EXPECT_EQ(2, output_stream_->Seek(2, kSeekSet));
    CheckOffset(2);
    uint8_t buf[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    EXPECT_TRUE(output_stream_->WriteFully(buf, 2));
    CheckOffset(4);
    EXPECT_EQ(6, output_stream_->Seek(2, kSeekEnd));
    CheckOffset(6);
    EXPECT_TRUE(output_stream_->WriteFully(buf, 4));
    CheckOffset(10);
  }

  void CheckTestOutput(const std::vector<uint8_t>& actual) {
    uint8_t expected[] = {
        0, 0, 1, 2, 0, 0, 1, 2, 3, 4
    };
    EXPECT_EQ(sizeof(expected), actual.size());
    EXPECT_EQ(0, memcmp(expected, &actual[0], actual.size()));
  }

  OutputStream* output_stream_;
};

TEST_F(OutputStreamTest, File) {
  ScratchFile tmp;
  FileOutputStream output_stream(tmp.GetFile());
  SetOutputStream(output_stream);
  GenerateTestOutput();
  UniquePtr<File> in(OS::OpenFileForReading(tmp.GetFilename().c_str()));
  EXPECT_TRUE(in.get() != NULL);
  std::vector<uint8_t> actual(in->GetLength());
  bool readSuccess = in->ReadFully(&actual[0], actual.size());
  EXPECT_TRUE(readSuccess);
  CheckTestOutput(actual);
}

TEST_F(OutputStreamTest, Buffered) {
  ScratchFile tmp;
  UniquePtr<FileOutputStream> file_output_stream(new FileOutputStream(tmp.GetFile()));
  CHECK(file_output_stream.get() != NULL);
  BufferedOutputStream buffered_output_stream(file_output_stream.release());
  SetOutputStream(buffered_output_stream);
  GenerateTestOutput();
  UniquePtr<File> in(OS::OpenFileForReading(tmp.GetFilename().c_str()));
  EXPECT_TRUE(in.get() != NULL);
  std::vector<uint8_t> actual(in->GetLength());
  bool readSuccess = in->ReadFully(&actual[0], actual.size());
  EXPECT_TRUE(readSuccess);
  CheckTestOutput(actual);
}

TEST_F(OutputStreamTest, Vector) {
  std::vector<uint8_t> output;
  VectorOutputStream output_stream("test vector output", output);
  SetOutputStream(output_stream);
  GenerateTestOutput();
  CheckTestOutput(output);
}

}  // namespace art
