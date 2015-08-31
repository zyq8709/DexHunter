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

#include <llvm/Support/raw_ostream.h>

#include "base/logging.h"
#include "utils.h"

#include "sea_ir/ir/sea.h"
#include "sea_ir/code_gen/code_gen.h"
#include "sea_ir/types/type_inference.h"
#include "sea_ir/types/types.h"

namespace sea_ir {

void CodeGenPrepassVisitor::Visit(PhiInstructionNode* phi) {
  Region* r = phi->GetRegion();
  const std::vector<Region*>* predecessors = r->GetPredecessors();
  DCHECK(NULL != predecessors);
  DCHECK_GT(predecessors->size(), 0u);
  llvm::PHINode *llvm_phi  = llvm_data_->builder_.CreatePHI(
      llvm::Type::getInt32Ty(*llvm_data_->context_), predecessors->size(), phi->StringId());
  llvm_data_->AddValue(phi, llvm_phi);
}

void CodeGenPassVisitor::Initialize(SeaGraph* graph) {
  Region* root_region;
  ordered_regions_.clear();
  for (std::vector<Region*>::const_iterator cit = graph->GetRegions()->begin();
        cit != graph->GetRegions()->end(); cit++ ) {
    if ((*cit)->GetIDominator() == (*cit)) {
      root_region = *cit;
    }
  }
  ordered_regions_.push_back(root_region);
  for (unsigned int id = 0; id < ordered_regions_.size(); id++) {
    Region* current_region = ordered_regions_.at(id);
    const std::set<Region*>* dominated_regions = current_region->GetIDominatedSet();
    for (std::set<Region*>::const_iterator cit = dominated_regions->begin();
            cit != dominated_regions->end(); cit++ ) {
      ordered_regions_.push_back(*cit);
    }
  }
}

void CodeGenPostpassVisitor::Visit(SeaGraph* graph) { }
void CodeGenVisitor::Visit(SeaGraph* graph) { }
void CodeGenPrepassVisitor::Visit(SeaGraph* graph) {
  std::vector<SignatureNode*>* parameters = graph->GetParameterNodes();
  // TODO: It may be better to extract correct types from dex
  //       instead than from type inference.
  DCHECK(parameters != NULL);
  std::vector<llvm::Type*> parameter_types;
  for (std::vector<SignatureNode*>::const_iterator param_iterator = parameters->begin();
      param_iterator!= parameters->end(); param_iterator++) {
    const Type* param_type = graph->ti_->type_data_.FindTypeOf((*param_iterator)->Id());
    DCHECK(param_type->Equals(graph->ti_->type_cache_->Integer()))
      << "Code generation for types other than integer not implemented.";
    parameter_types.push_back(llvm::Type::getInt32Ty(*llvm_data_->context_));
  }

  // TODO: Get correct function return type.
  const Type* return_type = graph->ti_->type_data_.FindTypeOf(-1);
  DCHECK(return_type->Equals(graph->ti_->type_cache_->Integer()))
    << "Code generation for types other than integer not implemented.";
  llvm::FunctionType *function_type = llvm::FunctionType::get(
      llvm::Type::getInt32Ty(*llvm_data_->context_),
      parameter_types, false);

  llvm_data_->function_ = llvm::Function::Create(function_type,
      llvm::Function::ExternalLinkage, function_name_, &llvm_data_->module_);
  unsigned param_id = 0;
  for (llvm::Function::arg_iterator arg_it = llvm_data_->function_->arg_begin();
      param_id != llvm_data_->function_->arg_size(); ++arg_it, ++param_id) {
    // TODO: The "+1" is because of the Method parameter on position 0.
    DCHECK(parameters->size() > param_id) << "Insufficient parameters for function signature";
    // Build parameter register name for LLVM IR clarity.
    std::string arg_name = art::StringPrintf("r%d", parameters->at(param_id)->GetResultRegister());
    arg_it->setName(arg_name);
    SignatureNode* parameter = parameters->at(param_id);
    llvm_data_->AddValue(parameter, arg_it);
  }

  std::vector<Region*>* regions = &ordered_regions_;
  DCHECK_GT(regions->size(), 0u);
  // Then create all other basic blocks.
  for (std::vector<Region*>::const_iterator cit = regions->begin(); cit != regions->end(); cit++) {
    llvm::BasicBlock* new_basic_block = llvm::BasicBlock::Create(*llvm_data_->context_,
        (*cit)->StringId(), llvm_data_->function_);
    llvm_data_->AddBlock((*cit), new_basic_block);
  }
}

void CodeGenPrepassVisitor::Visit(Region* region) {
  llvm_data_->builder_.SetInsertPoint(llvm_data_->GetBlock(region));
}
void CodeGenPostpassVisitor::Visit(Region* region) {
  llvm_data_->builder_.SetInsertPoint(llvm_data_->GetBlock(region));
}
void CodeGenVisitor::Visit(Region* region) {
  llvm_data_->builder_.SetInsertPoint(llvm_data_->GetBlock(region));
}


void CodeGenVisitor::Visit(InstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  DCHECK(0);  // This whole function is useful only during development.
}

void CodeGenVisitor::Visit(UnnamedConstInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "1.Instruction: " << instr << std::endl;
  llvm_data_->AddValue(instruction,
      llvm::ConstantInt::get(*llvm_data_->context_, llvm::APInt(32, instruction->GetConstValue())));
}

void CodeGenVisitor::Visit(ConstInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "1.Instruction: " << instr << std::endl;
  llvm_data_->AddValue(instruction,
      llvm::ConstantInt::get(*llvm_data_->context_, llvm::APInt(32, instruction->GetConstValue())));
}
void CodeGenVisitor::Visit(ReturnInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "2.Instruction: " << instr << std::endl;
  DCHECK_GT(instruction->GetSSAProducers().size(), 0u);
  llvm::Value* return_value = llvm_data_->GetValue(instruction->GetSSAProducers().at(0));
  llvm_data_->builder_.CreateRet(return_value);
}
void CodeGenVisitor::Visit(IfNeInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "3.Instruction: " << instr << std::endl;
  std::vector<InstructionNode*> ssa_uses = instruction->GetSSAProducers();
  DCHECK_GT(ssa_uses.size(), 1u);
  InstructionNode* use_l = ssa_uses.at(0);
  llvm::Value* left = llvm_data_->GetValue(use_l);

  InstructionNode* use_r = ssa_uses.at(1);
  llvm::Value* right = llvm_data_->GetValue(use_r);
  llvm::Value* ifne = llvm_data_->builder_.CreateICmpNE(left, right, instruction->StringId());
  DCHECK(instruction->GetRegion() != NULL);
  std::vector<Region*>* successors = instruction->GetRegion()->GetSuccessors();
  DCHECK_GT(successors->size(), 0u);
  llvm::BasicBlock* then_block = llvm_data_->GetBlock(successors->at(0));
  llvm::BasicBlock* else_block = llvm_data_->GetBlock(successors->at(1));

  llvm_data_->builder_.CreateCondBr(ifne, then_block, else_block);
}

/*
void CodeGenVisitor::Visit(AddIntLitInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "4.Instruction: " << instr << std::endl;
  std::vector<InstructionNode*> ssa_uses = instruction->GetSSAUses();
  InstructionNode* use_l = ssa_uses.at(0);
  llvm::Value* left = llvm_data->GetValue(use_l);
  llvm::Value* right = llvm::ConstantInt::get(*llvm_data->context_,
      llvm::APInt(32, instruction->GetConstValue()));
  llvm::Value* result = llvm_data->builder_.CreateAdd(left, right);
  llvm_data->AddValue(instruction, result);
}
*/
void CodeGenVisitor::Visit(MoveResultInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "5.Instruction: " << instr << std::endl;
  // TODO: Currently, this "mov" instruction is simulated by "res = return_register + 0".
  // This is inefficient, but should be optimized out by the coalescing phase of the reg alloc.
  // The TODO is to either ensure that this happens, or to
  // remove the move-result instructions completely from the IR
  // by merging them with the invoke-* instructions,
  // since their purpose of minimizing the number of opcodes in dex is
  // not relevant for the IR. (Will need to have different
  // instruction subclasses for functions and procedures.)
  std::vector<InstructionNode*> ssa_uses = instruction->GetSSAProducers();
  InstructionNode* use_l = ssa_uses.at(0);
  llvm::Value* left = llvm_data_->GetValue(use_l);
  llvm::Value* right = llvm::ConstantInt::get(*llvm_data_->context_, llvm::APInt(32, 0));
  llvm::Value* result = llvm_data_->builder_.CreateAdd(left, right);
  llvm_data_->AddValue(instruction, result);
}
void CodeGenVisitor::Visit(InvokeStaticInstructionNode* invoke) {
  std::string instr = invoke->GetInstruction()->DumpString(NULL);
  std::cout << "6.Instruction: " << instr << std::endl;
  // TODO: Build callee LLVM function name.
  std::string symbol = "dex_";
  symbol += art::MangleForJni(PrettyMethod(invoke->GetCalledMethodIndex(), dex_file_));
  std::string function_name = "dex_int_00020Main_fibonacci_00028int_00029";
  llvm::Function *callee = llvm_data_->module_.getFunction(function_name);
  // TODO: Add proper checking of the matching between formal and actual signature.
  DCHECK(NULL != callee);
  std::vector<llvm::Value*> parameter_values;
  std::vector<InstructionNode*> parameter_sources = invoke->GetSSAProducers();
  // TODO: Replace first parameter with Method argument instead of 0.
  parameter_values.push_back(llvm::ConstantInt::get(*llvm_data_->context_, llvm::APInt(32, 0)));
  for (std::vector<InstructionNode*>::const_iterator cit = parameter_sources.begin();
      cit != parameter_sources.end(); ++cit) {
    llvm::Value* parameter_value = llvm_data_->GetValue((*cit));
    DCHECK(NULL != parameter_value);
    parameter_values.push_back(parameter_value);
  }
  llvm::Value* return_value = llvm_data_->builder_.CreateCall(callee,
      parameter_values, invoke->StringId());
  llvm_data_->AddValue(invoke, return_value);
}
void CodeGenVisitor::Visit(AddIntInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "7.Instruction: " << instr << std::endl;
  std::vector<InstructionNode*> ssa_uses = instruction->GetSSAProducers();
  DCHECK_GT(ssa_uses.size(), 1u);
  InstructionNode* use_l = ssa_uses.at(0);
  InstructionNode* use_r = ssa_uses.at(1);
  llvm::Value* left = llvm_data_->GetValue(use_l);
  llvm::Value* right = llvm_data_->GetValue(use_r);
  llvm::Value* result = llvm_data_->builder_.CreateAdd(left, right);
  llvm_data_->AddValue(instruction, result);
}
void CodeGenVisitor::Visit(GotoInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "8.Instruction: " << instr << std::endl;
  std::vector<sea_ir::Region*>* targets = instruction->GetRegion()->GetSuccessors();
  DCHECK_EQ(targets->size(), 1u);
  llvm::BasicBlock* target_block = llvm_data_->GetBlock(targets->at(0));
  llvm_data_->builder_.CreateBr(target_block);
}
void CodeGenVisitor::Visit(IfEqzInstructionNode* instruction) {
  std::string instr = instruction->GetInstruction()->DumpString(NULL);
  std::cout << "9. Instruction: " << instr << "; Id: " <<instruction << std::endl;
  std::vector<InstructionNode*> ssa_uses = instruction->GetSSAProducers();
  DCHECK_GT(ssa_uses.size(), 0u);
  InstructionNode* use_l = ssa_uses.at(0);
  llvm::Value* left = llvm_data_->GetValue(use_l);
  llvm::Value* ifeqz = llvm_data_->builder_.CreateICmpEQ(left,
      llvm::ConstantInt::get(*llvm_data_->context_, llvm::APInt::getNullValue(32)),
      instruction->StringId());
  DCHECK(instruction->GetRegion() != NULL);
  std::vector<Region*>* successors = instruction->GetRegion()->GetSuccessors();
  DCHECK_GT(successors->size(), 0u);
  llvm::BasicBlock* then_block = llvm_data_->GetBlock(successors->at(0));
  llvm::BasicBlock* else_block = llvm_data_->GetBlock(successors->at(1));
  llvm_data_->builder_.CreateCondBr(ifeqz, then_block, else_block);
}

void CodeGenPostpassVisitor::Visit(PhiInstructionNode* phi) {
  std::cout << "10. Instruction: Phi(" << phi->GetRegisterNumber() << ")" << std::endl;
  Region* r = phi->GetRegion();
  const std::vector<Region*>* predecessors = r->GetPredecessors();
  DCHECK(NULL != predecessors);
  DCHECK_GT(predecessors->size(), 0u);
  // Prepass (CodeGenPrepassVisitor) should create the phi function value.
  llvm::PHINode* llvm_phi = (llvm::PHINode*) llvm_data_->GetValue(phi);
  int predecessor_pos = 0;
  for (std::vector<Region*>::const_iterator cit = predecessors->begin();
      cit != predecessors->end(); ++cit) {
    std::vector<InstructionNode*>* defining_instructions = phi->GetSSAUses(predecessor_pos++);
    DCHECK_EQ(defining_instructions->size(), 1u);
    InstructionNode* defining_instruction = defining_instructions->at(0);
    DCHECK(NULL != defining_instruction);
    Region* incoming_region = *cit;
    llvm::BasicBlock* incoming_basic_block = llvm_data_->GetBlock(incoming_region);
    llvm::Value* incoming_value = llvm_data_->GetValue(defining_instruction);
    llvm_phi->addIncoming(incoming_value, incoming_basic_block);
  }
}

void CodeGenVisitor::Visit(SignatureNode* signature) {
  DCHECK_EQ(signature->GetDefinitions().size(), 1u) <<
      "Signature nodes must correspond to a single parameter register.";
}
void CodeGenPrepassVisitor::Visit(SignatureNode* signature) {
  DCHECK_EQ(signature->GetDefinitions().size(), 1u) <<
      "Signature nodes must correspond to a single parameter register.";
}
void CodeGenPostpassVisitor::Visit(SignatureNode* signature) {
  DCHECK_EQ(signature->GetDefinitions().size(), 1u) <<
      "Signature nodes must correspond to a single parameter register.";
}

}  // namespace sea_ir
