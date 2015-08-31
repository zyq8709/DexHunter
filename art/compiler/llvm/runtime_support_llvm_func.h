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

#ifndef ART_COMPILER_LLVM_RUNTIME_SUPPORT_LLVM_FUNC_H_
#define ART_COMPILER_LLVM_RUNTIME_SUPPORT_LLVM_FUNC_H_

#include "runtime_support_llvm_func_list.h"

namespace art {
namespace llvm {
namespace runtime_support {

  enum RuntimeId {
#define DEFINE_RUNTIME_SUPPORT_FUNC_ID(ID, NAME) ID,
    RUNTIME_SUPPORT_FUNC_LIST(DEFINE_RUNTIME_SUPPORT_FUNC_ID)

    MAX_ID
  };

}  // namespace runtime_support
}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_RUNTIME_SUPPORT_LLVM_FUNC_H_
