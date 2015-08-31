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

#include "intrinsic_helper.h"

#include "ir_builder.h"

#include <llvm/IR/Attributes.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>

namespace art {
namespace llvm {

const IntrinsicHelper::IntrinsicInfo IntrinsicHelper::Info[] = {
#define DEF_INTRINSICS_FUNC(_, NAME, ATTR, RET_TYPE, ARG1_TYPE, ARG2_TYPE, \
                                                     ARG3_TYPE, ARG4_TYPE, \
                                                     ARG5_TYPE) \
  { #NAME, ATTR, RET_TYPE, { ARG1_TYPE, ARG2_TYPE, \
                             ARG3_TYPE, ARG4_TYPE, \
                             ARG5_TYPE} },
#include "intrinsic_func_list.def"
};

static ::llvm::Type* GetLLVMTypeOfIntrinsicValType(IRBuilder& irb,
                                                   IntrinsicHelper::IntrinsicValType type) {
  switch (type) {
    case IntrinsicHelper::kVoidTy: {
      return irb.getVoidTy();
    }
    case IntrinsicHelper::kJavaObjectTy: {
      return irb.getJObjectTy();
    }
    case IntrinsicHelper::kJavaMethodTy: {
      return irb.getJMethodTy();
    }
    case IntrinsicHelper::kJavaThreadTy: {
      return irb.getJThreadTy();
    }
    case IntrinsicHelper::kInt1Ty:
    case IntrinsicHelper::kInt1ConstantTy: {
      return irb.getInt1Ty();
    }
    case IntrinsicHelper::kInt8Ty:
    case IntrinsicHelper::kInt8ConstantTy: {
      return irb.getInt8Ty();
    }
    case IntrinsicHelper::kInt16Ty:
    case IntrinsicHelper::kInt16ConstantTy: {
      return irb.getInt16Ty();
    }
    case IntrinsicHelper::kInt32Ty:
    case IntrinsicHelper::kInt32ConstantTy: {
      return irb.getInt32Ty();
    }
    case IntrinsicHelper::kInt64Ty:
    case IntrinsicHelper::kInt64ConstantTy: {
      return irb.getInt64Ty();
    }
    case IntrinsicHelper::kFloatTy:
    case IntrinsicHelper::kFloatConstantTy: {
      return irb.getFloatTy();
    }
    case IntrinsicHelper::kDoubleTy:
    case IntrinsicHelper::kDoubleConstantTy: {
      return irb.getDoubleTy();
    }
    case IntrinsicHelper::kNone:
    case IntrinsicHelper::kVarArgTy:
    default: {
      LOG(FATAL) << "Invalid intrinsic type " << type << "to get LLVM type!";
      return NULL;
    }
  }
  // unreachable
}

IntrinsicHelper::IntrinsicHelper(::llvm::LLVMContext& context,
                                 ::llvm::Module& module) {
  IRBuilder irb(context, module, *this);

  ::memset(intrinsic_funcs_, 0, sizeof(intrinsic_funcs_));

  // This loop does the following things:
  // 1. Introduce the intrinsic function into the module
  // 2. Add "nocapture" and "noalias" attribute to the arguments in all
  //    intrinsics functions.
  // 3. Initialize intrinsic_funcs_map_.
  for (unsigned i = 0; i < MaxIntrinsicId; i++) {
    IntrinsicId id = static_cast<IntrinsicId>(i);
    const IntrinsicInfo& info = Info[i];

    // Parse and construct the argument type from IntrinsicInfo
    ::llvm::Type* arg_type[kIntrinsicMaxArgc];
    unsigned num_args = 0;
    bool is_var_arg = false;
    for (unsigned arg_iter = 0; arg_iter < kIntrinsicMaxArgc; arg_iter++) {
      IntrinsicValType type = info.arg_type_[arg_iter];

      if (type == kNone) {
        break;
      } else if (type == kVarArgTy) {
        // Variable argument type must be the last argument
        is_var_arg = true;
        break;
      }

      arg_type[num_args++] = GetLLVMTypeOfIntrinsicValType(irb, type);
    }

    // Construct the function type
    ::llvm::Type* ret_type =
        GetLLVMTypeOfIntrinsicValType(irb, info.ret_val_type_);

    ::llvm::FunctionType* type =
        ::llvm::FunctionType::get(ret_type,
                                  ::llvm::ArrayRef< ::llvm::Type*>(arg_type, num_args),
                                  is_var_arg);

    // Declare the function
    ::llvm::Function *fn = ::llvm::Function::Create(type,
                                                    ::llvm::Function::ExternalLinkage,
                                                     info.name_, &module);

    if (info.attr_ & kAttrReadOnly) {
        fn->setOnlyReadsMemory();
    }
    if (info.attr_ & kAttrReadNone) {
        fn->setDoesNotAccessMemory();
    }
    // None of the intrinsics throws exception
    fn->setDoesNotThrow();

    intrinsic_funcs_[id] = fn;

    DCHECK_NE(fn, static_cast< ::llvm::Function*>(NULL)) << "Intrinsic `"
        << GetName(id) << "' was not defined!";

    // Add "noalias" and "nocapture" attribute to all arguments of pointer type
    for (::llvm::Function::arg_iterator arg_iter = fn->arg_begin(),
            arg_end = fn->arg_end(); arg_iter != arg_end; arg_iter++) {
      if (arg_iter->getType()->isPointerTy()) {
        std::vector< ::llvm::Attribute::AttrKind> attributes;
        attributes.push_back(::llvm::Attribute::NoCapture);
        attributes.push_back(::llvm::Attribute::NoAlias);
        ::llvm::AttributeSet attribute_set = ::llvm::AttributeSet::get(fn->getContext(),
                                                                       arg_iter->getArgNo(),
                                                                       attributes);
        arg_iter->addAttr(attribute_set);
      }
    }

    // Insert the newly created intrinsic to intrinsic_funcs_map_
    if (!intrinsic_funcs_map_.insert(std::make_pair(fn, id)).second) {
      LOG(FATAL) << "Duplicate entry in intrinsic functions map?";
    }
  }

  return;
}

}  // namespace llvm
}  // namespace art
