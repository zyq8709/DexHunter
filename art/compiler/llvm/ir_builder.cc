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

#include "ir_builder.h"

#include "base/stringprintf.h"

#include <llvm/IR/Module.h>

namespace art {
namespace llvm {


//----------------------------------------------------------------------------
// General
//----------------------------------------------------------------------------

IRBuilder::IRBuilder(::llvm::LLVMContext& context, ::llvm::Module& module,
                     IntrinsicHelper& intrinsic_helper)
    : LLVMIRBuilder(context), module_(&module), mdb_(context), java_object_type_(NULL),
      java_method_type_(NULL), java_thread_type_(NULL), intrinsic_helper_(intrinsic_helper) {
  // Get java object type from module
  ::llvm::Type* jobject_struct_type = module.getTypeByName("JavaObject");
  CHECK(jobject_struct_type != NULL);
  java_object_type_ = jobject_struct_type->getPointerTo();

  // If type of Method is not explicitly defined in the module, use JavaObject*
  ::llvm::Type* type = module.getTypeByName("Method");
  if (type != NULL) {
    java_method_type_ = type->getPointerTo();
  } else {
    java_method_type_ = java_object_type_;
  }

  // If type of Thread is not explicitly defined in the module, use JavaObject*
  type = module.getTypeByName("Thread");
  if (type != NULL) {
    java_thread_type_ = type->getPointerTo();
  } else {
    java_thread_type_ = java_object_type_;
  }

  // Create JEnv* type
  ::llvm::Type* jenv_struct_type = ::llvm::StructType::create(context, "JEnv");
  jenv_type_ = jenv_struct_type->getPointerTo();

  // Get Art shadow frame struct type from module
  art_frame_type_ = module.getTypeByName("ShadowFrame");
  CHECK(art_frame_type_ != NULL);

  runtime_support_ = NULL;
}


//----------------------------------------------------------------------------
// Type Helper Function
//----------------------------------------------------------------------------

::llvm::Type* IRBuilder::getJType(JType jty) {
  switch (jty) {
  case kVoid:
    return getJVoidTy();

  case kBoolean:
    return getJBooleanTy();

  case kByte:
    return getJByteTy();

  case kChar:
    return getJCharTy();

  case kShort:
    return getJShortTy();

  case kInt:
    return getJIntTy();

  case kLong:
    return getJLongTy();

  case kFloat:
    return getJFloatTy();

  case kDouble:
    return getJDoubleTy();

  case kObject:
    return getJObjectTy();

  default:
    LOG(FATAL) << "Unknown java type: " << jty;
    return NULL;
  }
}

::llvm::StructType* IRBuilder::getShadowFrameTy(uint32_t vreg_size) {
  std::string name(StringPrintf("ShadowFrame%u", vreg_size));

  // Try to find the existing struct type definition
  if (::llvm::Type* type = module_->getTypeByName(name)) {
    CHECK(::llvm::isa< ::llvm::StructType>(type));
    return static_cast< ::llvm::StructType*>(type);
  }

  // Create new struct type definition
  ::llvm::Type* elem_types[] = {
    art_frame_type_,
    ::llvm::ArrayType::get(getInt32Ty(), vreg_size),
  };

  return ::llvm::StructType::create(elem_types, name);
}


}  // namespace llvm
}  // namespace art
