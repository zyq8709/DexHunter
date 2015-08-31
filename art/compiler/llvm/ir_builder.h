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

#ifndef ART_COMPILER_LLVM_IR_BUILDER_H_
#define ART_COMPILER_LLVM_IR_BUILDER_H_

#include "backend_types.h"
#include "dex/compiler_enums.h"
#include "intrinsic_helper.h"
#include "md_builder.h"
#include "runtime_support_builder.h"
#include "runtime_support_llvm_func.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/NoFolder.h>

#include <stdint.h>


namespace art {
namespace llvm {

class InserterWithDexOffset : public ::llvm::IRBuilderDefaultInserter<true> {
  public:
    InserterWithDexOffset() : node_(NULL) {}

    void InsertHelper(::llvm::Instruction *I, const ::llvm::Twine &Name,
                      ::llvm::BasicBlock *BB,
                      ::llvm::BasicBlock::iterator InsertPt) const {
      ::llvm::IRBuilderDefaultInserter<true>::InsertHelper(I, Name, BB, InsertPt);
      if (node_ != NULL) {
        I->setMetadata("DexOff", node_);
      }
    }

    void SetDexOffset(::llvm::MDNode* node) {
      node_ = node;
    }
  private:
    ::llvm::MDNode* node_;
};

typedef ::llvm::IRBuilder<true, ::llvm::ConstantFolder, InserterWithDexOffset> LLVMIRBuilder;
// NOTE: Here we define our own LLVMIRBuilder type alias, so that we can
// switch "preserveNames" template parameter easily.


class IRBuilder : public LLVMIRBuilder {
 public:
  //--------------------------------------------------------------------------
  // General
  //--------------------------------------------------------------------------

  IRBuilder(::llvm::LLVMContext& context, ::llvm::Module& module,
            IntrinsicHelper& intrinsic_helper);


  //--------------------------------------------------------------------------
  // Extend load & store for TBAA
  //--------------------------------------------------------------------------

  ::llvm::LoadInst* CreateLoad(::llvm::Value* ptr, ::llvm::MDNode* tbaa_info) {
    ::llvm::LoadInst* inst = LLVMIRBuilder::CreateLoad(ptr);
    inst->setMetadata(::llvm::LLVMContext::MD_tbaa, tbaa_info);
    return inst;
  }

  ::llvm::StoreInst* CreateStore(::llvm::Value* val, ::llvm::Value* ptr, ::llvm::MDNode* tbaa_info) {
    ::llvm::StoreInst* inst = LLVMIRBuilder::CreateStore(val, ptr);
    inst->setMetadata(::llvm::LLVMContext::MD_tbaa, tbaa_info);
    return inst;
  }

  ::llvm::AtomicCmpXchgInst*
  CreateAtomicCmpXchgInst(::llvm::Value* ptr, ::llvm::Value* cmp, ::llvm::Value* val,
                          ::llvm::MDNode* tbaa_info) {
    ::llvm::AtomicCmpXchgInst* inst =
        LLVMIRBuilder::CreateAtomicCmpXchg(ptr, cmp, val, ::llvm::Acquire);
    inst->setMetadata(::llvm::LLVMContext::MD_tbaa, tbaa_info);
    return inst;
  }

  //--------------------------------------------------------------------------
  // Extend memory barrier
  //--------------------------------------------------------------------------
  void CreateMemoryBarrier(MemBarrierKind barrier_kind) {
#if ANDROID_SMP
    // TODO: select atomic ordering according to given barrier kind.
    CreateFence(::llvm::SequentiallyConsistent);
#endif
  }

  //--------------------------------------------------------------------------
  // TBAA
  //--------------------------------------------------------------------------

  // TODO: After we design the non-special TBAA info, re-design the TBAA interface.
  ::llvm::LoadInst* CreateLoad(::llvm::Value* ptr, TBAASpecialType special_ty) {
    return CreateLoad(ptr, mdb_.GetTBAASpecialType(special_ty));
  }

  ::llvm::StoreInst* CreateStore(::llvm::Value* val, ::llvm::Value* ptr, TBAASpecialType special_ty) {
    DCHECK_NE(special_ty, kTBAAConstJObject) << "ConstJObject is read only!";
    return CreateStore(val, ptr, mdb_.GetTBAASpecialType(special_ty));
  }

  ::llvm::LoadInst* CreateLoad(::llvm::Value* ptr, TBAASpecialType special_ty, JType j_ty) {
    return CreateLoad(ptr, mdb_.GetTBAAMemoryJType(special_ty, j_ty));
  }

  ::llvm::StoreInst* CreateStore(::llvm::Value* val, ::llvm::Value* ptr,
                               TBAASpecialType special_ty, JType j_ty) {
    DCHECK_NE(special_ty, kTBAAConstJObject) << "ConstJObject is read only!";
    return CreateStore(val, ptr, mdb_.GetTBAAMemoryJType(special_ty, j_ty));
  }

  ::llvm::LoadInst* LoadFromObjectOffset(::llvm::Value* object_addr,
                                       int64_t offset,
                                       ::llvm::Type* type,
                                       TBAASpecialType special_ty) {
    return LoadFromObjectOffset(object_addr, offset, type, mdb_.GetTBAASpecialType(special_ty));
  }

  void StoreToObjectOffset(::llvm::Value* object_addr,
                           int64_t offset,
                           ::llvm::Value* new_value,
                           TBAASpecialType special_ty) {
    DCHECK_NE(special_ty, kTBAAConstJObject) << "ConstJObject is read only!";
    StoreToObjectOffset(object_addr, offset, new_value, mdb_.GetTBAASpecialType(special_ty));
  }

  ::llvm::LoadInst* LoadFromObjectOffset(::llvm::Value* object_addr,
                                       int64_t offset,
                                       ::llvm::Type* type,
                                       TBAASpecialType special_ty, JType j_ty) {
    return LoadFromObjectOffset(object_addr, offset, type, mdb_.GetTBAAMemoryJType(special_ty, j_ty));
  }

  void StoreToObjectOffset(::llvm::Value* object_addr,
                           int64_t offset,
                           ::llvm::Value* new_value,
                           TBAASpecialType special_ty, JType j_ty) {
    DCHECK_NE(special_ty, kTBAAConstJObject) << "ConstJObject is read only!";
    StoreToObjectOffset(object_addr, offset, new_value, mdb_.GetTBAAMemoryJType(special_ty, j_ty));
  }

  ::llvm::AtomicCmpXchgInst*
  CompareExchangeObjectOffset(::llvm::Value* object_addr,
                              int64_t offset,
                              ::llvm::Value* cmp_value,
                              ::llvm::Value* new_value,
                              TBAASpecialType special_ty) {
    DCHECK_NE(special_ty, kTBAAConstJObject) << "ConstJObject is read only!";
    return CompareExchangeObjectOffset(object_addr, offset, cmp_value, new_value,
                                       mdb_.GetTBAASpecialType(special_ty));
  }

  void SetTBAA(::llvm::Instruction* inst, TBAASpecialType special_ty) {
    inst->setMetadata(::llvm::LLVMContext::MD_tbaa, mdb_.GetTBAASpecialType(special_ty));
  }


  //--------------------------------------------------------------------------
  // Static Branch Prediction
  //--------------------------------------------------------------------------

  // Import the orignal conditional branch
  using LLVMIRBuilder::CreateCondBr;
  ::llvm::BranchInst* CreateCondBr(::llvm::Value *cond,
                                 ::llvm::BasicBlock* true_bb,
                                 ::llvm::BasicBlock* false_bb,
                                 ExpectCond expect) {
    ::llvm::BranchInst* branch_inst = CreateCondBr(cond, true_bb, false_bb);
    if (false) {
      // TODO: http://b/8511695 Restore branch weight metadata
      branch_inst->setMetadata(::llvm::LLVMContext::MD_prof, mdb_.GetBranchWeights(expect));
    }
    return branch_inst;
  }


  //--------------------------------------------------------------------------
  // Pointer Arithmetic Helper Function
  //--------------------------------------------------------------------------

  ::llvm::IntegerType* getPtrEquivIntTy() {
    return getInt32Ty();
  }

  size_t getSizeOfPtrEquivInt() {
    return 4;
  }

  ::llvm::ConstantInt* getSizeOfPtrEquivIntValue() {
    return getPtrEquivInt(getSizeOfPtrEquivInt());
  }

  ::llvm::ConstantInt* getPtrEquivInt(int64_t i) {
    return ::llvm::ConstantInt::get(getPtrEquivIntTy(), i);
  }

  ::llvm::Value* CreatePtrDisp(::llvm::Value* base,
                             ::llvm::Value* offset,
                             ::llvm::PointerType* ret_ty) {
    ::llvm::Value* base_int = CreatePtrToInt(base, getPtrEquivIntTy());
    ::llvm::Value* result_int = CreateAdd(base_int, offset);
    ::llvm::Value* result = CreateIntToPtr(result_int, ret_ty);

    return result;
  }

  ::llvm::Value* CreatePtrDisp(::llvm::Value* base,
                             ::llvm::Value* bs,
                             ::llvm::Value* count,
                             ::llvm::Value* offset,
                             ::llvm::PointerType* ret_ty) {
    ::llvm::Value* block_offset = CreateMul(bs, count);
    ::llvm::Value* total_offset = CreateAdd(block_offset, offset);

    return CreatePtrDisp(base, total_offset, ret_ty);
  }

  ::llvm::LoadInst* LoadFromObjectOffset(::llvm::Value* object_addr,
                                       int64_t offset,
                                       ::llvm::Type* type,
                                       ::llvm::MDNode* tbaa_info) {
    // Convert offset to ::llvm::value
    ::llvm::Value* llvm_offset = getPtrEquivInt(offset);
    // Calculate the value's address
    ::llvm::Value* value_addr = CreatePtrDisp(object_addr, llvm_offset, type->getPointerTo());
    // Load
    return CreateLoad(value_addr, tbaa_info);
  }

  void StoreToObjectOffset(::llvm::Value* object_addr,
                           int64_t offset,
                           ::llvm::Value* new_value,
                           ::llvm::MDNode* tbaa_info) {
    // Convert offset to ::llvm::value
    ::llvm::Value* llvm_offset = getPtrEquivInt(offset);
    // Calculate the value's address
    ::llvm::Value* value_addr = CreatePtrDisp(object_addr,
                                            llvm_offset,
                                            new_value->getType()->getPointerTo());
    // Store
    CreateStore(new_value, value_addr, tbaa_info);
  }

  ::llvm::AtomicCmpXchgInst* CompareExchangeObjectOffset(::llvm::Value* object_addr,
                                                       int64_t offset,
                                                       ::llvm::Value* cmp_value,
                                                       ::llvm::Value* new_value,
                                                       ::llvm::MDNode* tbaa_info) {
    // Convert offset to ::llvm::value
    ::llvm::Value* llvm_offset = getPtrEquivInt(offset);
    // Calculate the value's address
    ::llvm::Value* value_addr = CreatePtrDisp(object_addr,
                                            llvm_offset,
                                            new_value->getType()->getPointerTo());
    // Atomic compare and exchange
    return CreateAtomicCmpXchgInst(value_addr, cmp_value, new_value, tbaa_info);
  }


  //--------------------------------------------------------------------------
  // Runtime Helper Function
  //--------------------------------------------------------------------------

  RuntimeSupportBuilder& Runtime() {
    return *runtime_support_;
  }

  // TODO: Deprecate
  ::llvm::Function* GetRuntime(runtime_support::RuntimeId rt) {
    return runtime_support_->GetRuntimeSupportFunction(rt);
  }

  // TODO: Deprecate
  void SetRuntimeSupport(RuntimeSupportBuilder* runtime_support) {
    // Can only set once. We can't do this on constructor, because RuntimeSupportBuilder needs
    // IRBuilder.
    if (runtime_support_ == NULL && runtime_support != NULL) {
      runtime_support_ = runtime_support;
    }
  }


  //--------------------------------------------------------------------------
  // Type Helper Function
  //--------------------------------------------------------------------------

  ::llvm::Type* getJType(char shorty_jty) {
    return getJType(GetJTypeFromShorty(shorty_jty));
  }

  ::llvm::Type* getJType(JType jty);

  ::llvm::Type* getJVoidTy() {
    return getVoidTy();
  }

  ::llvm::IntegerType* getJBooleanTy() {
    return getInt8Ty();
  }

  ::llvm::IntegerType* getJByteTy() {
    return getInt8Ty();
  }

  ::llvm::IntegerType* getJCharTy() {
    return getInt16Ty();
  }

  ::llvm::IntegerType* getJShortTy() {
    return getInt16Ty();
  }

  ::llvm::IntegerType* getJIntTy() {
    return getInt32Ty();
  }

  ::llvm::IntegerType* getJLongTy() {
    return getInt64Ty();
  }

  ::llvm::Type* getJFloatTy() {
    return getFloatTy();
  }

  ::llvm::Type* getJDoubleTy() {
    return getDoubleTy();
  }

  ::llvm::PointerType* getJObjectTy() {
    return java_object_type_;
  }

  ::llvm::PointerType* getJMethodTy() {
    return java_method_type_;
  }

  ::llvm::PointerType* getJThreadTy() {
    return java_thread_type_;
  }

  ::llvm::Type* getArtFrameTy() {
    return art_frame_type_;
  }

  ::llvm::PointerType* getJEnvTy() {
    return jenv_type_;
  }

  ::llvm::Type* getJValueTy() {
    // NOTE: JValue is an union type, which may contains boolean, byte, char,
    // short, int, long, float, double, Object.  However, LLVM itself does
    // not support union type, so we have to return a type with biggest size,
    // then bitcast it before we use it.
    return getJLongTy();
  }

  ::llvm::StructType* getShadowFrameTy(uint32_t vreg_size);


  //--------------------------------------------------------------------------
  // Constant Value Helper Function
  //--------------------------------------------------------------------------

  ::llvm::ConstantInt* getJBoolean(bool is_true) {
    return (is_true) ? getTrue() : getFalse();
  }

  ::llvm::ConstantInt* getJByte(int8_t i) {
    return ::llvm::ConstantInt::getSigned(getJByteTy(), i);
  }

  ::llvm::ConstantInt* getJChar(int16_t i) {
    return ::llvm::ConstantInt::getSigned(getJCharTy(), i);
  }

  ::llvm::ConstantInt* getJShort(int16_t i) {
    return ::llvm::ConstantInt::getSigned(getJShortTy(), i);
  }

  ::llvm::ConstantInt* getJInt(int32_t i) {
    return ::llvm::ConstantInt::getSigned(getJIntTy(), i);
  }

  ::llvm::ConstantInt* getJLong(int64_t i) {
    return ::llvm::ConstantInt::getSigned(getJLongTy(), i);
  }

  ::llvm::Constant* getJFloat(float f) {
    return ::llvm::ConstantFP::get(getJFloatTy(), f);
  }

  ::llvm::Constant* getJDouble(double d) {
    return ::llvm::ConstantFP::get(getJDoubleTy(), d);
  }

  ::llvm::ConstantPointerNull* getJNull() {
    return ::llvm::ConstantPointerNull::get(getJObjectTy());
  }

  ::llvm::Constant* getJZero(char shorty_jty) {
    return getJZero(GetJTypeFromShorty(shorty_jty));
  }

  ::llvm::Constant* getJZero(JType jty) {
    switch (jty) {
    case kVoid:
      LOG(FATAL) << "Zero is not a value of void type";
      return NULL;

    case kBoolean:
      return getJBoolean(false);

    case kByte:
      return getJByte(0);

    case kChar:
      return getJChar(0);

    case kShort:
      return getJShort(0);

    case kInt:
      return getJInt(0);

    case kLong:
      return getJLong(0);

    case kFloat:
      return getJFloat(0.0f);

    case kDouble:
      return getJDouble(0.0);

    case kObject:
      return getJNull();

    default:
      LOG(FATAL) << "Unknown java type: " << jty;
      return NULL;
    }
  }


 private:
  ::llvm::Module* module_;

  MDBuilder mdb_;

  ::llvm::PointerType* java_object_type_;
  ::llvm::PointerType* java_method_type_;
  ::llvm::PointerType* java_thread_type_;

  ::llvm::PointerType* jenv_type_;

  ::llvm::StructType* art_frame_type_;

  RuntimeSupportBuilder* runtime_support_;

  IntrinsicHelper& intrinsic_helper_;
};


}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_IR_BUILDER_H_
