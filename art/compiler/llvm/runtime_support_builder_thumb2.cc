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

#include "runtime_support_builder_thumb2.h"

#include "ir_builder.h"
#include "mirror/object.h"
#include "monitor.h"
#include "thread.h"
#include "utils_llvm.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <inttypes.h>
#include <vector>

using ::llvm::BasicBlock;
using ::llvm::Function;
using ::llvm::FunctionType;
using ::llvm::InlineAsm;
using ::llvm::Type;
using ::llvm::Value;

namespace art {
namespace llvm {


void RuntimeSupportBuilderThumb2::EmitLockObject(Value* object) {
  FunctionType* func_ty = FunctionType::get(/*Result=*/irb_.getInt32Ty(),
                                            /*Params=*/irb_.getJObjectTy(),
                                            /*isVarArg=*/false);
  // $0: result
  // $1: object
  // $2: temp
  // $3: temp
  std::string asms;
  StringAppendF(&asms, "add $3, $1, #%" PRId32 "\n", mirror::Object::MonitorOffset().Int32Value());
  StringAppendF(&asms, "ldr $2, [r9, #%" PRId32 "]\n", Thread::ThinLockIdOffset().Int32Value());
  StringAppendF(&asms, "ldrex $0, [$3]\n");
  StringAppendF(&asms, "lsl $2, $2, %d\n", LW_LOCK_OWNER_SHIFT);
  StringAppendF(&asms, "bfi $2, $0, #0, #%d\n", LW_LOCK_OWNER_SHIFT - 1);
  StringAppendF(&asms, "bfc $0, #%d, #%d\n", LW_HASH_STATE_SHIFT, LW_LOCK_OWNER_SHIFT - 1);
  StringAppendF(&asms, "cmp $0, #0\n");
  StringAppendF(&asms, "it eq\n");
  StringAppendF(&asms, "strexeq $0, $2, [$3]\n");

  InlineAsm* func = InlineAsm::get(func_ty, asms, "=&l,l,~l,~l", true);

  Value* retry_slow_path = irb_.CreateCall(func, object);
  retry_slow_path = irb_.CreateICmpNE(retry_slow_path, irb_.getJInt(0));

  Function* parent_func = irb_.GetInsertBlock()->getParent();
  BasicBlock* basic_block_lock = BasicBlock::Create(context_, "lock", parent_func);
  BasicBlock* basic_block_cont = BasicBlock::Create(context_, "lock_cont", parent_func);
  irb_.CreateCondBr(retry_slow_path, basic_block_lock, basic_block_cont, kUnlikely);

  irb_.SetInsertPoint(basic_block_lock);
  Function* slow_func = GetRuntimeSupportFunction(runtime_support::LockObject);
  irb_.CreateCall2(slow_func, object, EmitGetCurrentThread());
  irb_.CreateBr(basic_block_cont);

  irb_.SetInsertPoint(basic_block_cont);
  {  // Memory barrier
    FunctionType* asm_ty = FunctionType::get(/*Result=*/Type::getVoidTy(context_),
                                              /*isVarArg=*/false);
    InlineAsm* func = InlineAsm::get(asm_ty, "dmb sy", "", true);
    irb_.CreateCall(func);
  }
}


}  // namespace llvm
}  // namespace art
