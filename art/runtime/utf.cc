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

#include "utf.h"

#include "base/logging.h"
#include "mirror/array.h"
#include "mirror/object-inl.h"

namespace art {

size_t CountModifiedUtf8Chars(const char* utf8) {
  size_t len = 0;
  int ic;
  while ((ic = *utf8++) != '\0') {
    len++;
    if ((ic & 0x80) == 0) {
      // one-byte encoding
      continue;
    }
    // two- or three-byte encoding
    utf8++;
    if ((ic & 0x20) == 0) {
      // two-byte encoding
      continue;
    }
    // three-byte encoding
    utf8++;
  }
  return len;
}

void ConvertModifiedUtf8ToUtf16(uint16_t* utf16_data_out, const char* utf8_data_in) {
  while (*utf8_data_in != '\0') {
    *utf16_data_out++ = GetUtf16FromUtf8(&utf8_data_in);
  }
}

void ConvertUtf16ToModifiedUtf8(char* utf8_out, const uint16_t* utf16_in, size_t char_count) {
  while (char_count--) {
    uint16_t ch = *utf16_in++;
    if (ch > 0 && ch <= 0x7f) {
      *utf8_out++ = ch;
    } else {
      if (ch > 0x07ff) {
        *utf8_out++ = (ch >> 12) | 0xe0;
        *utf8_out++ = ((ch >> 6) & 0x3f) | 0x80;
        *utf8_out++ = (ch & 0x3f) | 0x80;
      } else /*(ch > 0x7f || ch == 0)*/ {
        *utf8_out++ = (ch >> 6) | 0xc0;
        *utf8_out++ = (ch & 0x3f) | 0x80;
      }
    }
  }
}

int32_t ComputeUtf16Hash(const mirror::CharArray* chars, int32_t offset,
                         size_t char_count) {
  int32_t hash = 0;
  for (size_t i = 0; i < char_count; i++) {
    hash = hash * 31 + chars->Get(offset + i);
  }
  return hash;
}

int32_t ComputeUtf16Hash(const uint16_t* chars, size_t char_count) {
  int32_t hash = 0;
  while (char_count--) {
    hash = hash * 31 + *chars++;
  }
  return hash;
}


uint16_t GetUtf16FromUtf8(const char** utf8_data_in) {
  uint8_t one = *(*utf8_data_in)++;
  if ((one & 0x80) == 0) {
    // one-byte encoding
    return one;
  }
  // two- or three-byte encoding
  uint8_t two = *(*utf8_data_in)++;
  if ((one & 0x20) == 0) {
    // two-byte encoding
    return ((one & 0x1f) << 6) | (two & 0x3f);
  }
  // three-byte encoding
  uint8_t three = *(*utf8_data_in)++;
  return ((one & 0x0f) << 12) | ((two & 0x3f) << 6) | (three & 0x3f);
}

int CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(const char* utf8_1, const char* utf8_2) {
  for (;;) {
    if (*utf8_1 == '\0') {
      return (*utf8_2 == '\0') ? 0 : -1;
    } else if (*utf8_2 == '\0') {
      return 1;
    }

    int c1 = GetUtf16FromUtf8(&utf8_1);
    int c2 = GetUtf16FromUtf8(&utf8_2);

    if (c1 != c2) {
      return c1 > c2 ? 1 : -1;
    }
  }
}

int CompareModifiedUtf8ToUtf16AsCodePointValues(const char* utf8_1, const uint16_t* utf8_2) {
  for (;;) {
    if (*utf8_1 == '\0') {
      return (*utf8_2 == '\0') ? 0 : -1;
    } else if (*utf8_2 == '\0') {
      return 1;
    }

    int c1 = GetUtf16FromUtf8(&utf8_1);
    int c2 = *utf8_2;

    if (c1 != c2) {
      return c1 > c2 ? 1 : -1;
    }
  }
}

size_t CountUtf8Bytes(const uint16_t* chars, size_t char_count) {
  size_t result = 0;
  while (char_count--) {
    uint16_t ch = *chars++;
    if (ch > 0 && ch <= 0x7f) {
      ++result;
    } else {
      if (ch > 0x7ff) {
        result += 3;
      } else {
        result += 2;
      }
    }
  }
  return result;
}

}  // namespace art
