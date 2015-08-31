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

#include <stdint.h>

namespace art {

int CmplFloat(float a, float b) {
  if (a == b) {
    return 0;
  } else if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  }
  return -1;
}

int CmpgFloat(float a, float b) {
  if (a == b) {
    return 0;
  } else if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  }
  return 1;
}

int CmpgDouble(double a, double b) {
  if (a == b) {
    return 0;
  } else if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  }
  return 1;
}

int CmplDouble(double a, double b) {
  if (a == b) {
    return 0;
  } else if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  }
  return -1;
}

extern "C" int64_t artLmulFromCode(int64_t a, int64_t b) {
  return a * b;
}

extern "C" int64_t artLdivFromCode(int64_t a, int64_t b) {
  return a / b;
}

extern "C" int64_t artLdivmodFromCode(int64_t a, int64_t b) {
  return a % b;
}

}  // namespace art
