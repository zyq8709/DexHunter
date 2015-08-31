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

#ifndef ART_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_THUMB2_H_
#define ART_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_THUMB2_H_

#include "runtime_support_builder_arm.h"

namespace art {
namespace llvm {

class RuntimeSupportBuilderThumb2 : public RuntimeSupportBuilderARM {
 public:
  RuntimeSupportBuilderThumb2(::llvm::LLVMContext& context, ::llvm::Module& module, IRBuilder& irb)
    : RuntimeSupportBuilderARM(context, module, irb) {}

  /* Monitor */
  virtual void EmitLockObject(::llvm::Value* object);
};

}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_THUMB2_H_
