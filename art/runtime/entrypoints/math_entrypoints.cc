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

namespace art {

extern "C" double art_l2d(int64_t l) {
  return static_cast<double>(l);
}

extern "C" float art_l2f(int64_t l) {
  return static_cast<float>(l);
}

/*
 * Float/double conversion requires clamping to min and max of integer form.  If
 * target doesn't support this normally, use these.
 */
extern "C" int64_t art_d2l(double d) {
  static const double kMaxLong = static_cast<double>(static_cast<int64_t>(0x7fffffffffffffffULL));
  static const double kMinLong = static_cast<double>(static_cast<int64_t>(0x8000000000000000ULL));
  if (d >= kMaxLong) {
    return static_cast<int64_t>(0x7fffffffffffffffULL);
  } else if (d <= kMinLong) {
    return static_cast<int64_t>(0x8000000000000000ULL);
  } else if (d != d)  {  // NaN case
    return 0;
  } else {
    return static_cast<int64_t>(d);
  }
}

extern "C" int64_t art_f2l(float f) {
  static const float kMaxLong = static_cast<float>(static_cast<int64_t>(0x7fffffffffffffffULL));
  static const float kMinLong = static_cast<float>(static_cast<int64_t>(0x8000000000000000ULL));
  if (f >= kMaxLong) {
    return static_cast<int64_t>(0x7fffffffffffffffULL);
  } else if (f <= kMinLong) {
    return static_cast<int64_t>(0x8000000000000000ULL);
  } else if (f != f) {  // NaN case
    return 0;
  } else {
    return static_cast<int64_t>(f);
  }
}

extern "C" int32_t art_d2i(double d) {
  static const double kMaxInt = static_cast<double>(static_cast<int32_t>(0x7fffffffUL));
  static const double kMinInt = static_cast<double>(static_cast<int32_t>(0x80000000UL));
  if (d >= kMaxInt) {
    return static_cast<int32_t>(0x7fffffffUL);
  } else if (d <= kMinInt) {
    return static_cast<int32_t>(0x80000000UL);
  } else if (d != d)  {  // NaN case
    return 0;
  } else {
    return static_cast<int32_t>(d);
  }
}

extern "C" int32_t art_f2i(float f) {
  static const float kMaxInt = static_cast<float>(static_cast<int32_t>(0x7fffffffUL));
  static const float kMinInt = static_cast<float>(static_cast<int32_t>(0x80000000UL));
  if (f >= kMaxInt) {
    return static_cast<int32_t>(0x7fffffffUL);
  } else if (f <= kMinInt) {
    return static_cast<int32_t>(0x80000000UL);
  } else if (f != f) {  // NaN case
    return 0;
  } else {
    return static_cast<int32_t>(f);
  }
}

}  // namespace art
