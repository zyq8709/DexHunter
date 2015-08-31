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

#ifndef ART_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_H_
#define ART_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_H_

#include "backend_types.h"
#include "base/logging.h"
#include "runtime_support_llvm_func.h"

#include <stdint.h>

namespace llvm {
  class LLVMContext;
  class Module;
  class Function;
  class Type;
  class Value;
}

namespace art {
namespace llvm {

class IRBuilder;


class RuntimeSupportBuilder {
 public:
  RuntimeSupportBuilder(::llvm::LLVMContext& context, ::llvm::Module& module, IRBuilder& irb);

  /* Thread */
  virtual ::llvm::Value* EmitGetCurrentThread();
  virtual ::llvm::Value* EmitLoadFromThreadOffset(int64_t offset, ::llvm::Type* type,
                                                TBAASpecialType s_ty);
  virtual void EmitStoreToThreadOffset(int64_t offset, ::llvm::Value* value,
                                       TBAASpecialType s_ty);
  virtual ::llvm::Value* EmitSetCurrentThread(::llvm::Value* thread);

  /* ShadowFrame */
  virtual ::llvm::Value* EmitPushShadowFrame(::llvm::Value* new_shadow_frame,
                                           ::llvm::Value* method, uint32_t num_vregs);
  virtual ::llvm::Value* EmitPushShadowFrameNoInline(::llvm::Value* new_shadow_frame,
                                                   ::llvm::Value* method, uint32_t num_vregs);
  virtual void EmitPopShadowFrame(::llvm::Value* old_shadow_frame);

  /* Exception */
  virtual ::llvm::Value* EmitGetAndClearException();
  virtual ::llvm::Value* EmitIsExceptionPending();

  /* Suspend */
  virtual void EmitTestSuspend();

  /* Monitor */
  virtual void EmitLockObject(::llvm::Value* object);
  virtual void EmitUnlockObject(::llvm::Value* object);

  /* MarkGCCard */
  virtual void EmitMarkGCCard(::llvm::Value* value, ::llvm::Value* target_addr);

  ::llvm::Function* GetRuntimeSupportFunction(runtime_support::RuntimeId id) {
    if (id >= 0 && id < runtime_support::MAX_ID) {
      return runtime_support_func_decls_[id];
    } else {
      LOG(ERROR) << "Unknown runtime function id: " << id;
      return NULL;
    }
  }

  virtual ~RuntimeSupportBuilder() {}

 protected:
  ::llvm::LLVMContext& context_;
  ::llvm::Module& module_;
  IRBuilder& irb_;

 private:
  ::llvm::Function* runtime_support_func_decls_[runtime_support::MAX_ID];
  bool target_runtime_support_func_[runtime_support::MAX_ID];
};


}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_H_
