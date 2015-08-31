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

#ifndef ART_COMPILER_LEB128_ENCODER_H_
#define ART_COMPILER_LEB128_ENCODER_H_

#include "base/macros.h"

namespace art {

// An encoder with an API similar to vector<uint32_t> where the data is captured in ULEB128 format.
class UnsignedLeb128EncodingVector {
 public:
  UnsignedLeb128EncodingVector() {
  }

  void PushBack(uint32_t value) {
    bool done = false;
    do {
      uint8_t out = value & 0x7f;
      if (out != value) {
        data_.push_back(out | 0x80);
        value >>= 7;
      } else {
        data_.push_back(out);
        done = true;
      }
    } while (!done);
  }

  template<typename It>
  void InsertBack(It cur, It end) {
    for (; cur != end; ++cur) {
      PushBack(*cur);
    }
  }

  const std::vector<uint8_t>& GetData() const {
    return data_;
  }

 private:
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(UnsignedLeb128EncodingVector);
};

}  // namespace art

#endif  // ART_COMPILER_LEB128_ENCODER_H_
