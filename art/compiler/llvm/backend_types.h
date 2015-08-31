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

#ifndef ART_COMPILER_LLVM_BACKEND_TYPES_H_
#define ART_COMPILER_LLVM_BACKEND_TYPES_H_

#include "base/logging.h"


namespace art {
namespace llvm {


enum JType {
  kVoid,
  kBoolean,
  kByte,
  kChar,
  kShort,
  kInt,
  kLong,
  kFloat,
  kDouble,
  kObject,
  MAX_JTYPE
};

enum TBAASpecialType {
  kTBAARegister,
  kTBAAStackTemp,
  kTBAAHeapArray,
  kTBAAHeapInstance,
  kTBAAHeapStatic,
  kTBAAJRuntime,
  kTBAARuntimeInfo,
  kTBAAShadowFrame,
  kTBAAConstJObject,
  MAX_TBAA_SPECIAL_TYPE
};


enum ExpectCond {
  kLikely,
  kUnlikely,
  MAX_EXPECT
};


inline JType GetJTypeFromShorty(char shorty_jty) {
  switch (shorty_jty) {
  case 'V':
    return kVoid;

  case 'Z':
    return kBoolean;

  case 'B':
    return kByte;

  case 'C':
    return kChar;

  case 'S':
    return kShort;

  case 'I':
    return kInt;

  case 'J':
    return kLong;

  case 'F':
    return kFloat;

  case 'D':
    return kDouble;

  case 'L':
    return kObject;

  default:
    LOG(FATAL) << "Unknown Dalvik shorty descriptor: " << shorty_jty;
    return kVoid;
  }
}

}  // namespace llvm
}  // namespace art


#endif  // ART_COMPILER_LLVM_BACKEND_TYPES_H_
