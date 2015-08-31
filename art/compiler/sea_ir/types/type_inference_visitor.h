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

#ifndef ART_COMPILER_SEA_IR_TYPES_TYPE_INFERENCE_VISITOR_H_
#define ART_COMPILER_SEA_IR_TYPES_TYPE_INFERENCE_VISITOR_H_


#include "dex_file-inl.h"
#include "sea_ir/ir/visitor.h"
#include "sea_ir/types/types.h"

namespace sea_ir {

// The TypeInferenceVisitor visits each instruction and computes its type taking into account
//   the current type of the operands. The type is stored in the visitor.
// We may be better off by using a separate visitor type hierarchy that has return values
//   or that passes data as parameters, than to use fields to store information that should
//   in fact be returned after visiting each element. Ideally, I would prefer to use templates
//   to specify the returned value type, but I am not aware of a possible implementation
//   that does not horribly duplicate the visitor infrastructure code (version 1: no return value,
//   version 2: with template return value).
class TypeInferenceVisitor: public IRVisitor {
 public:
  TypeInferenceVisitor(SeaGraph* graph, TypeData* type_data,
      art::verifier::RegTypeCache* types):
    graph_(graph), type_data_(type_data), type_cache_(types), crt_type_() {
  }
  // There are no type related actions to be performed on these classes.
  void Initialize(SeaGraph* graph) { }
  void Visit(SeaGraph* graph);
  void Visit(Region* region) { }

  void Visit(PhiInstructionNode* instruction);
  void Visit(SignatureNode* parameter);
  void Visit(InstructionNode* instruction) { }
  void Visit(UnnamedConstInstructionNode* instruction);
  void Visit(ConstInstructionNode* instruction) { }
  void Visit(ReturnInstructionNode* instruction) { }
  void Visit(IfNeInstructionNode* instruction) { }
  void Visit(MoveResultInstructionNode* instruction);
  void Visit(InvokeStaticInstructionNode* instruction);
  void Visit(AddIntInstructionNode* instruction);
  void Visit(GotoInstructionNode* instruction) { }
  void Visit(IfEqzInstructionNode* instruction) { }

  const Type* MergeTypes(std::vector<const Type*>& types) const;
  const Type* MergeTypes(const Type* t1, const Type* t2) const;
  std::vector<const Type*> GetOperandTypes(InstructionNode* instruction) const;
  const Type* GetType() {
    // TODO: Currently multiple defined types are not supported.
    if (!crt_type_.empty()) {
      const Type* single_type = crt_type_.at(0);
      crt_type_.clear();
      return single_type;
    }
    return NULL;
  }

 protected:
  const SeaGraph* const graph_;
  TypeData* type_data_;
  art::verifier::RegTypeCache* type_cache_;
  std::vector<const Type*> crt_type_;             // Stored temporarily between two calls to Visit.
};

}  // namespace sea_ir

#endif  // ART_COMPILER_SEA_IR_TYPES_TYPE_INFERENCE_VISITOR_H_
