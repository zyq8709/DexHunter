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

#include "runtime_support_builder.h"

#include "gc/accounting/card_table.h"
#include "ir_builder.h"
#include "monitor.h"
#include "mirror/object.h"
#include "runtime_support_llvm_func_list.h"
#include "thread.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

using ::llvm::BasicBlock;
using ::llvm::CallInst;
using ::llvm::Function;
using ::llvm::Value;

namespace art {
namespace llvm {

RuntimeSupportBuilder::RuntimeSupportBuilder(::llvm::LLVMContext& context,
                                             ::llvm::Module& module,
                                             IRBuilder& irb)
    : context_(context), module_(module), irb_(irb) {
  memset(target_runtime_support_func_, 0, sizeof(target_runtime_support_func_));
#define GET_RUNTIME_SUPPORT_FUNC_DECL(ID, NAME) \
  do { \
    ::llvm::Function* fn = module_.getFunction(#NAME); \
    DCHECK(fn != NULL) << "Function not found: " << #NAME; \
    runtime_support_func_decls_[runtime_support::ID] = fn; \
  } while (0);

  RUNTIME_SUPPORT_FUNC_LIST(GET_RUNTIME_SUPPORT_FUNC_DECL)
}


/* Thread */

::llvm::Value* RuntimeSupportBuilder::EmitGetCurrentThread() {
  Function* func = GetRuntimeSupportFunction(runtime_support::GetCurrentThread);
  CallInst* call_inst = irb_.CreateCall(func);
  call_inst->setOnlyReadsMemory();
  irb_.SetTBAA(call_inst, kTBAAConstJObject);
  return call_inst;
}

::llvm::Value* RuntimeSupportBuilder::EmitLoadFromThreadOffset(int64_t offset, ::llvm::Type* type,
                                                             TBAASpecialType s_ty) {
  Value* thread = EmitGetCurrentThread();
  return irb_.LoadFromObjectOffset(thread, offset, type, s_ty);
}

void RuntimeSupportBuilder::EmitStoreToThreadOffset(int64_t offset, ::llvm::Value* value,
                                                    TBAASpecialType s_ty) {
  Value* thread = EmitGetCurrentThread();
  irb_.StoreToObjectOffset(thread, offset, value, s_ty);
}

::llvm::Value* RuntimeSupportBuilder::EmitSetCurrentThread(::llvm::Value* thread) {
  Function* func = GetRuntimeSupportFunction(runtime_support::SetCurrentThread);
  return irb_.CreateCall(func, thread);
}


/* ShadowFrame */

::llvm::Value* RuntimeSupportBuilder::EmitPushShadowFrame(::llvm::Value* new_shadow_frame,
                                                        ::llvm::Value* method,
                                                        uint32_t num_vregs) {
  Value* old_shadow_frame = EmitLoadFromThreadOffset(Thread::TopShadowFrameOffset().Int32Value(),
                                                     irb_.getArtFrameTy()->getPointerTo(),
                                                     kTBAARuntimeInfo);
  EmitStoreToThreadOffset(Thread::TopShadowFrameOffset().Int32Value(),
                          new_shadow_frame,
                          kTBAARuntimeInfo);

  // Store the method pointer
  irb_.StoreToObjectOffset(new_shadow_frame,
                           ShadowFrame::MethodOffset(),
                           method,
                           kTBAAShadowFrame);

  // Store the number of vregs
  irb_.StoreToObjectOffset(new_shadow_frame,
                           ShadowFrame::NumberOfVRegsOffset(),
                           irb_.getInt32(num_vregs),
                           kTBAAShadowFrame);

  // Store the link to previous shadow frame
  irb_.StoreToObjectOffset(new_shadow_frame,
                           ShadowFrame::LinkOffset(),
                           old_shadow_frame,
                           kTBAAShadowFrame);

  return old_shadow_frame;
}

::llvm::Value*
RuntimeSupportBuilder::EmitPushShadowFrameNoInline(::llvm::Value* new_shadow_frame,
                                                   ::llvm::Value* method,
                                                   uint32_t num_vregs) {
  Function* func = GetRuntimeSupportFunction(runtime_support::PushShadowFrame);
  ::llvm::CallInst* call_inst =
      irb_.CreateCall4(func,
                       EmitGetCurrentThread(),
                       new_shadow_frame,
                       method,
                       irb_.getInt32(num_vregs));
  irb_.SetTBAA(call_inst, kTBAARuntimeInfo);
  return call_inst;
}

void RuntimeSupportBuilder::EmitPopShadowFrame(::llvm::Value* old_shadow_frame) {
  // Store old shadow frame to TopShadowFrame
  EmitStoreToThreadOffset(Thread::TopShadowFrameOffset().Int32Value(),
                          old_shadow_frame,
                          kTBAARuntimeInfo);
}


/* Exception */

::llvm::Value* RuntimeSupportBuilder::EmitGetAndClearException() {
  Function* slow_func = GetRuntimeSupportFunction(runtime_support::GetAndClearException);
  return irb_.CreateCall(slow_func, EmitGetCurrentThread());
}

::llvm::Value* RuntimeSupportBuilder::EmitIsExceptionPending() {
  Value* exception = EmitLoadFromThreadOffset(Thread::ExceptionOffset().Int32Value(),
                                              irb_.getJObjectTy(),
                                              kTBAARuntimeInfo);
  // If exception not null
  return irb_.CreateIsNotNull(exception);
}


/* Suspend */

void RuntimeSupportBuilder::EmitTestSuspend() {
  Function* slow_func = GetRuntimeSupportFunction(runtime_support::TestSuspend);
  CallInst* call_inst = irb_.CreateCall(slow_func, EmitGetCurrentThread());
  irb_.SetTBAA(call_inst, kTBAAJRuntime);
}


/* Monitor */

void RuntimeSupportBuilder::EmitLockObject(::llvm::Value* object) {
  Value* monitor =
      irb_.LoadFromObjectOffset(object,
                                mirror::Object::MonitorOffset().Int32Value(),
                                irb_.getJIntTy(),
                                kTBAARuntimeInfo);

  Value* real_monitor =
      irb_.CreateAnd(monitor, ~(LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT));

  // Is thin lock, unheld and not recursively acquired.
  Value* unheld = irb_.CreateICmpEQ(real_monitor, irb_.getInt32(0));

  Function* parent_func = irb_.GetInsertBlock()->getParent();
  BasicBlock* bb_fast = BasicBlock::Create(context_, "lock_fast", parent_func);
  BasicBlock* bb_slow = BasicBlock::Create(context_, "lock_slow", parent_func);
  BasicBlock* bb_cont = BasicBlock::Create(context_, "lock_cont", parent_func);
  irb_.CreateCondBr(unheld, bb_fast, bb_slow, kLikely);

  irb_.SetInsertPoint(bb_fast);

  // Calculate new monitor: new = old | (lock_id << LW_LOCK_OWNER_SHIFT)
  Value* lock_id =
      EmitLoadFromThreadOffset(Thread::ThinLockIdOffset().Int32Value(),
                               irb_.getInt32Ty(), kTBAARuntimeInfo);

  Value* owner = irb_.CreateShl(lock_id, LW_LOCK_OWNER_SHIFT);
  Value* new_monitor = irb_.CreateOr(monitor, owner);

  // Atomically update monitor.
  Value* old_monitor =
      irb_.CompareExchangeObjectOffset(object,
                                       mirror::Object::MonitorOffset().Int32Value(),
                                       monitor, new_monitor, kTBAARuntimeInfo);

  Value* retry_slow_path = irb_.CreateICmpEQ(old_monitor, monitor);
  irb_.CreateCondBr(retry_slow_path, bb_cont, bb_slow, kLikely);

  irb_.SetInsertPoint(bb_slow);
  Function* slow_func = GetRuntimeSupportFunction(runtime_support::LockObject);
  irb_.CreateCall2(slow_func, object, EmitGetCurrentThread());
  irb_.CreateBr(bb_cont);

  irb_.SetInsertPoint(bb_cont);
}

void RuntimeSupportBuilder::EmitUnlockObject(::llvm::Value* object) {
  Value* lock_id =
      EmitLoadFromThreadOffset(Thread::ThinLockIdOffset().Int32Value(),
                               irb_.getJIntTy(),
                               kTBAARuntimeInfo);
  Value* monitor =
      irb_.LoadFromObjectOffset(object,
                                mirror::Object::MonitorOffset().Int32Value(),
                                irb_.getJIntTy(),
                                kTBAARuntimeInfo);

  Value* my_monitor = irb_.CreateShl(lock_id, LW_LOCK_OWNER_SHIFT);
  Value* hash_state = irb_.CreateAnd(monitor, (LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT));
  Value* real_monitor = irb_.CreateAnd(monitor, ~(LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT));

  // Is thin lock, held by us and not recursively acquired
  Value* is_fast_path = irb_.CreateICmpEQ(real_monitor, my_monitor);

  Function* parent_func = irb_.GetInsertBlock()->getParent();
  BasicBlock* bb_fast = BasicBlock::Create(context_, "unlock_fast", parent_func);
  BasicBlock* bb_slow = BasicBlock::Create(context_, "unlock_slow", parent_func);
  BasicBlock* bb_cont = BasicBlock::Create(context_, "unlock_cont", parent_func);
  irb_.CreateCondBr(is_fast_path, bb_fast, bb_slow, kLikely);

  irb_.SetInsertPoint(bb_fast);
  // Set all bits to zero (except hash state)
  irb_.StoreToObjectOffset(object,
                           mirror::Object::MonitorOffset().Int32Value(),
                           hash_state,
                           kTBAARuntimeInfo);
  irb_.CreateBr(bb_cont);

  irb_.SetInsertPoint(bb_slow);
  Function* slow_func = GetRuntimeSupportFunction(runtime_support::UnlockObject);
  irb_.CreateCall2(slow_func, object, EmitGetCurrentThread());
  irb_.CreateBr(bb_cont);

  irb_.SetInsertPoint(bb_cont);
}


void RuntimeSupportBuilder::EmitMarkGCCard(::llvm::Value* value, ::llvm::Value* target_addr) {
  Function* parent_func = irb_.GetInsertBlock()->getParent();
  BasicBlock* bb_mark_gc_card = BasicBlock::Create(context_, "mark_gc_card", parent_func);
  BasicBlock* bb_cont = BasicBlock::Create(context_, "mark_gc_card_cont", parent_func);

  ::llvm::Value* not_null = irb_.CreateIsNotNull(value);
  irb_.CreateCondBr(not_null, bb_mark_gc_card, bb_cont);

  irb_.SetInsertPoint(bb_mark_gc_card);
  Value* card_table = EmitLoadFromThreadOffset(Thread::CardTableOffset().Int32Value(),
                                               irb_.getInt8Ty()->getPointerTo(),
                                               kTBAAConstJObject);
  Value* target_addr_int = irb_.CreatePtrToInt(target_addr, irb_.getPtrEquivIntTy());
  Value* card_no = irb_.CreateLShr(target_addr_int,
                                   irb_.getPtrEquivInt(gc::accounting::CardTable::kCardShift));
  Value* card_table_entry = irb_.CreateGEP(card_table, card_no);
  irb_.CreateStore(irb_.getInt8(gc::accounting::CardTable::kCardDirty), card_table_entry,
                   kTBAARuntimeInfo);
  irb_.CreateBr(bb_cont);

  irb_.SetInsertPoint(bb_cont);
}


}  // namespace llvm
}  // namespace art
