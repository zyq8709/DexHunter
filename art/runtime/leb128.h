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

#ifndef ART_RUNTIME_LEB128_H_
#define ART_RUNTIME_LEB128_H_

#include "globals.h"

namespace art {

// Reads an unsigned LEB128 value, updating the given pointer to point
// just past the end of the read value. This function tolerates
// non-zero high-order bits in the fifth encoded byte.
static inline uint32_t DecodeUnsignedLeb128(const uint8_t** data) {
  const uint8_t* ptr = *data;
  int result = *(ptr++);
  if (result > 0x7f) {
    int cur = *(ptr++);
    result = (result & 0x7f) | ((cur & 0x7f) << 7);
    if (cur > 0x7f) {
      cur = *(ptr++);
      result |= (cur & 0x7f) << 14;
      if (cur > 0x7f) {
        cur = *(ptr++);
        result |= (cur & 0x7f) << 21;
        if (cur > 0x7f) {
          // Note: We don't check to see if cur is out of range here,
          // meaning we tolerate garbage in the four high-order bits.
          cur = *(ptr++);
          result |= cur << 28;
        }
      }
    }
  }
  *data = ptr;
  return static_cast<uint32_t>(result);
}

// Reads an unsigned LEB128 + 1 value. updating the given pointer to point
// just past the end of the read value. This function tolerates
// non-zero high-order bits in the fifth encoded byte.
// It is possible for this function to return -1.
static inline int32_t DecodeUnsignedLeb128P1(const uint8_t** data) {
  return DecodeUnsignedLeb128(data) - 1;
}

// Reads a signed LEB128 value, updating the given pointer to point
// just past the end of the read value. This function tolerates
// non-zero high-order bits in the fifth encoded byte.
static inline int32_t DecodeSignedLeb128(const uint8_t** data) {
  const uint8_t* ptr = *data;
  int32_t result = *(ptr++);
  if (result <= 0x7f) {
    result = (result << 25) >> 25;
  } else {
    int cur = *(ptr++);
    result = (result & 0x7f) | ((cur & 0x7f) << 7);
    if (cur <= 0x7f) {
      result = (result << 18) >> 18;
    } else {
      cur = *(ptr++);
      result |= (cur & 0x7f) << 14;
      if (cur <= 0x7f) {
        result = (result << 11) >> 11;
      } else {
        cur = *(ptr++);
        result |= (cur & 0x7f) << 21;
        if (cur <= 0x7f) {
          result = (result << 4) >> 4;
        } else {
          // Note: We don't check to see if cur is out of range here,
          // meaning we tolerate garbage in the four high-order bits.
          cur = *(ptr++);
          result |= cur << 28;
        }
      }
    }
  }
  *data = ptr;
  return result;
}

// Returns the number of bytes needed to encode the value in unsigned LEB128.
static inline uint32_t UnsignedLeb128Size(uint32_t data) {
  uint32_t count = 0;
  do {
    data >>= 7;
    count++;
  } while (data != 0);
  return count;
}

}  // namespace art

#endif  // ART_RUNTIME_LEB128_H_
