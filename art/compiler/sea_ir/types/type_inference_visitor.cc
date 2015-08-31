/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "scoped_thread_state_change.h"
#include "sea_ir/types/type_inference_visitor.h"
#include "sea_ir/types/type_inference.h"
#include "sea_ir/ir/sea.h"

namespace sea_ir {

void TypeInferenceVisitor::Visit(SeaGraph* graph) {
  FunctionTypeInfo fti(graph_, type_cache_);
  const Type* return_type = fti.GetReturnValueType();
  crt_type_.push_back(return_type);
}

void TypeInferenceVisitor::Visit(SignatureNode* parameter) {
  FunctionTypeInfo fti(graph_, type_cache_);
  std::vector<const Type*> arguments = fti.GetDeclaredArgumentTypes();
  DCHECK_LT(parameter->GetPositionInSignature(), arguments.size())
    << "Signature node position not present in signature.";
  crt_type_.push_back(arguments.at(parameter->GetPositionInSignature()));
}

void TypeInferenceVisitor::Visit(UnnamedConstInstructionNode* instruction) {
  crt_type_.push_back(&type_cache_->Integer());
}

void TypeInferenceVisitor::Visit(PhiInstructionNode* instruction) {
  std::vector<const Type*> types_to_merge = GetOperandTypes(instruction);
  const Type* result_type = MergeTypes(types_to_merge);
  crt_type_.push_back(result_type);
}

void TypeInferenceVisitor::Visit(AddIntInstructionNode* instruction) {
  std::vector<const Type*> operand_types = GetOperandTypes(instruction);
  for (std::vector<const Type*>::const_iterator cit = operand_types.begin();
      cit != operand_types.end(); cit++) {
    if (*cit != NULL) {
      DCHECK((*cit)->IsInteger());
    }
  }
  crt_type_.push_back(&type_cache_->Integer());
}

void TypeInferenceVisitor::Visit(MoveResultInstructionNode* instruction) {
  std::vector<const Type*> operand_types = GetOperandTypes(instruction);
  const Type* operand_type = operand_types.at(0);
  crt_type_.push_back(operand_type);
}

void TypeInferenceVisitor::Visit(InvokeStaticInstructionNode* instruction) {
  FunctionTypeInfo fti(graph_, instruction, type_cache_);
  const Type* result_type = fti.GetReturnValueType();
  crt_type_.push_back(result_type);
}

std::vector<const Type*> TypeInferenceVisitor::GetOperandTypes(
    InstructionNode* instruction) const {
  std::vector<InstructionNode*> sources = instruction->GetSSAProducers();
  std::vector<const Type*> types_to_merge;
  for (std::vector<InstructionNode*>::const_iterator cit = sources.begin(); cit != sources.end();
      cit++) {
    const Type* source_type = type_data_->FindTypeOf((*cit)->Id());
    if (source_type != NULL) {
      types_to_merge.push_back(source_type);
    }
  }
  return types_to_merge;
}

const Type* TypeInferenceVisitor::MergeTypes(std::vector<const Type*>& types) const {
  const Type* type = NULL;
  if (types.size() > 0) {
    type = *(types.begin());
    if (types.size() > 1) {
      for (std::vector<const Type*>::const_iterator cit = types.begin();
          cit != types.end(); cit++) {
        if (!type->Equals(**cit)) {
          type = MergeTypes(type, *cit);
        }
      }
    }
  }
  return type;
}

const Type* TypeInferenceVisitor::MergeTypes(const Type* t1, const Type* t2) const {
  DCHECK(t2 != NULL);
  DCHECK(t1 != NULL);
  art::ScopedObjectAccess soa(art::Thread::Current());
  const Type* result = &(t1->Merge(*t2, type_cache_));
  return result;
}

}   // namespace sea_ir
