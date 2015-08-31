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

#ifndef ART_COMPILER_LLVM_MD_BUILDER_H_
#define ART_COMPILER_LLVM_MD_BUILDER_H_

#include "backend_types.h"

#include "llvm/IR/MDBuilder.h"

#include <cstring>

namespace llvm {
  class LLVMContext;
  class MDNode;
}

namespace art {
namespace llvm {

typedef ::llvm::MDBuilder LLVMMDBuilder;

class MDBuilder : public LLVMMDBuilder {
 public:
  explicit MDBuilder(::llvm::LLVMContext& context)
     : LLVMMDBuilder(context), tbaa_root_(createTBAARoot("Art TBAA Root")) {
    std::memset(tbaa_special_type_, 0, sizeof(tbaa_special_type_));
    std::memset(tbaa_memory_jtype_, 0, sizeof(tbaa_memory_jtype_));

    // Pre-generate the MDNode for static branch prediction
    // 64 and 4 are the llvm.expect's default values
    expect_cond_[kLikely] = createBranchWeights(64, 4);
    expect_cond_[kUnlikely] = createBranchWeights(4, 64);
  }

  ::llvm::MDNode* GetTBAASpecialType(TBAASpecialType special_ty);
  ::llvm::MDNode* GetTBAAMemoryJType(TBAASpecialType special_ty, JType j_ty);

  ::llvm::MDNode* GetBranchWeights(ExpectCond expect) {
    DCHECK_LT(expect, MAX_EXPECT) << "MAX_EXPECT is not for branch weight";
    return expect_cond_[expect];
  }

 private:
  ::llvm::MDNode* const tbaa_root_;
  ::llvm::MDNode* tbaa_special_type_[MAX_TBAA_SPECIAL_TYPE];
  // There are 3 categories of memory types will not alias: array element, instance field, and
  // static field.
  ::llvm::MDNode* tbaa_memory_jtype_[3][MAX_JTYPE];

  ::llvm::MDNode* expect_cond_[MAX_EXPECT];
};


}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_MD_BUILDER_H_
