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


#include "md_builder.h"

#include "llvm/IR/MDBuilder.h"

#include <string>

namespace art {
namespace llvm {


::llvm::MDNode* MDBuilder::GetTBAASpecialType(TBAASpecialType sty_id) {
  DCHECK_GE(sty_id, 0) << "Unknown TBAA special type: " << sty_id;
  DCHECK_LT(sty_id, MAX_TBAA_SPECIAL_TYPE) << "Unknown TBAA special type: " << sty_id;
  DCHECK(tbaa_root_ != NULL);

  ::llvm::MDNode*& spec_ty = tbaa_special_type_[sty_id];
  if (spec_ty == NULL) {
    switch (sty_id) {
    case kTBAARegister:
      spec_ty = createTBAANode("Register", tbaa_root_);
      break;
    case kTBAAStackTemp:
      spec_ty = createTBAANode("StackTemp", tbaa_root_);
      break;
    case kTBAAHeapArray:
      spec_ty = createTBAANode("HeapArray", tbaa_root_);
      break;
    case kTBAAHeapInstance:
      spec_ty = createTBAANode("HeapInstance", tbaa_root_);
      break;
    case kTBAAHeapStatic:
      spec_ty = createTBAANode("HeapStatic", tbaa_root_);
      break;
    case kTBAAJRuntime:
      spec_ty = createTBAANode("JRuntime", tbaa_root_);
      break;
    case kTBAARuntimeInfo:
      spec_ty = createTBAANode("RuntimeInfo", GetTBAASpecialType(kTBAAJRuntime));
      break;
    case kTBAAShadowFrame:
      spec_ty = createTBAANode("ShadowFrame", GetTBAASpecialType(kTBAAJRuntime));
      break;
    case kTBAAConstJObject:
      spec_ty = createTBAANode("ConstJObject", tbaa_root_, true);
      break;
    default:
      LOG(FATAL) << "Unknown TBAA special type: " << sty_id;
      break;
    }
  }
  return spec_ty;
}

::llvm::MDNode* MDBuilder::GetTBAAMemoryJType(TBAASpecialType sty_id, JType jty_id) {
  DCHECK(sty_id == kTBAAHeapArray ||
         sty_id == kTBAAHeapInstance ||
         sty_id == kTBAAHeapStatic) << "SpecialType must be array, instance, or static";

  DCHECK_GE(jty_id, 0) << "Unknown JType: " << jty_id;
  DCHECK_LT(jty_id, MAX_JTYPE) << "Unknown JType: " << jty_id;
  DCHECK_NE(jty_id, kVoid) << "Can't load/store Void type!";

  std::string name;
  size_t sty_mapped_index = 0;
  switch (sty_id) {
  case kTBAAHeapArray:    sty_mapped_index = 0; name = "HeapArray "; break;
  case kTBAAHeapInstance: sty_mapped_index = 1; name = "HeapInstance "; break;
  case kTBAAHeapStatic:   sty_mapped_index = 2; name = "HeapStatic "; break;
  default:
    LOG(FATAL) << "Unknown TBAA special type: " << sty_id;
    break;
  }

  ::llvm::MDNode*& spec_ty = tbaa_memory_jtype_[sty_mapped_index][jty_id];
  if (spec_ty != NULL) {
    return spec_ty;
  }

  switch (jty_id) {
  case kBoolean: name += "Boolean"; break;
  case kByte:    name += "Byte"; break;
  case kChar:    name += "Char"; break;
  case kShort:   name += "Short"; break;
  case kInt:     name += "Int"; break;
  case kLong:    name += "Long"; break;
  case kFloat:   name += "Float"; break;
  case kDouble:  name += "Double"; break;
  case kObject:  name += "Object"; break;
  default:
    LOG(FATAL) << "Unknown JType: " << jty_id;
    break;
  }

  spec_ty = createTBAANode(name, GetTBAASpecialType(sty_id));
  return spec_ty;
}


}  // namespace llvm
}  // namespace art
