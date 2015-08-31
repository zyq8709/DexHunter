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

#ifndef ART_COMPILER_LLVM_INTRINSIC_HELPER_H_
#define ART_COMPILER_LLVM_INTRINSIC_HELPER_H_

#include "base/logging.h"

#include <llvm/ADT/DenseMap.h>

namespace llvm {
  class Function;
  class FunctionType;
  class LLVMContext;
  class Module;
}  // namespace llvm

namespace art {
namespace llvm {

class IRBuilder;

class IntrinsicHelper {
 public:
  enum IntrinsicId {
#define DEF_INTRINSICS_FUNC(ID, ...) ID,
#include "intrinsic_func_list.def"
    MaxIntrinsicId,

    // Pseudo-intrinsics Id
    UnknownId
  };

  enum IntrinsicAttribute {
    kAttrNone     = 0,

    // Intrinsic that neither modified the memory state nor refer to the global
    // state
    kAttrReadNone = 1 << 0,

    // Intrinsic that doesn't modify the memory state. Note that one should set
    // this flag carefully when the intrinsic may throw exception. Since the
    // thread state is implicitly modified when an exception is thrown.
    kAttrReadOnly = 1 << 1,

    // Note that intrinsic without kAttrNoThrow and kAttrDoThrow set means that
    // intrinsic generates exception in some cases

    // Intrinsic that never generates exception
    kAttrNoThrow  = 1 << 2,
    // Intrinsic that always generate exception
    kAttrDoThrow  = 1 << 3,
  };

  enum IntrinsicValType {
    kNone,

    kVoidTy,

    kJavaObjectTy,
    kJavaMethodTy,
    kJavaThreadTy,

    kInt1Ty,
    kInt8Ty,
    kInt16Ty,
    kInt32Ty,
    kInt64Ty,
    kFloatTy,
    kDoubleTy,

    kInt1ConstantTy,
    kInt8ConstantTy,
    kInt16ConstantTy,
    kInt32ConstantTy,
    kInt64ConstantTy,
    kFloatConstantTy,
    kDoubleConstantTy,

    kVarArgTy,
  };

  enum {
    kIntrinsicMaxArgc = 5
  };

  typedef struct IntrinsicInfo {
    const char* name_;
    unsigned attr_;
    IntrinsicValType ret_val_type_;
    IntrinsicValType arg_type_[kIntrinsicMaxArgc];
  } IntrinsicInfo;

 private:
  static const IntrinsicInfo Info[];

 public:
  static const IntrinsicInfo& GetInfo(IntrinsicId id) {
    DCHECK(id >= 0 && id < MaxIntrinsicId) << "Unknown ART intrinsics ID: "
                                           << id;
    return Info[id];
  }

  static const char* GetName(IntrinsicId id) {
    return (id <= MaxIntrinsicId) ? GetInfo(id).name_ : "InvalidIntrinsic";
  }

  static unsigned GetAttr(IntrinsicId id) {
    return GetInfo(id).attr_;
  }

 public:
  IntrinsicHelper(::llvm::LLVMContext& context, ::llvm::Module& module);

  ::llvm::Function* GetIntrinsicFunction(IntrinsicId id) {
    DCHECK(id >= 0 && id < MaxIntrinsicId) << "Unknown ART intrinsics ID: "
                                           << id;
    return intrinsic_funcs_[id];
  }

  IntrinsicId GetIntrinsicId(const ::llvm::Function* func) const {
    ::llvm::DenseMap<const ::llvm::Function*, IntrinsicId>::const_iterator
        i = intrinsic_funcs_map_.find(func);
    if (i == intrinsic_funcs_map_.end()) {
      return UnknownId;
    } else {
      return i->second;
    }
  }

 private:
  // FIXME: "+1" is to workaround the GCC bugs:
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949
  // Remove this when uses newer GCC (> 4.4.3)
  ::llvm::Function* intrinsic_funcs_[MaxIntrinsicId + 1];

  // Map a llvm::Function to its intrinsic id
  ::llvm::DenseMap<const ::llvm::Function*, IntrinsicId> intrinsic_funcs_map_;
};

}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_INTRINSIC_HELPER_H_
