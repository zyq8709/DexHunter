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

#ifndef ART_COMPILER_SEA_IR_CODE_GEN_CODE_GEN_H_
#define ART_COMPILER_SEA_IR_CODE_GEN_CODE_GEN_H_

#include "instruction_set.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "sea_ir/ir/visitor.h"

namespace sea_ir {
// Abstracts away the containers we use to map SEA IR objects to LLVM IR objects.
class CodeGenData {
 public:
  explicit CodeGenData(): context_(&llvm::getGlobalContext()), module_("sea_ir", *context_),
      builder_(*context_), function_(), blocks_(), values_() { }
  // Returns the llvm::BasicBlock* corresponding to the sea_ir::Region with id @region_id.
  llvm::BasicBlock* GetBlock(int region_id) {
    std::map<int, llvm::BasicBlock*>::iterator block_it = blocks_.find(region_id);
    DCHECK(block_it != blocks_.end());
    return block_it->second;
  }
  // Returns the llvm::BasicBlock* corresponding top the sea_ir::Region @region.
  llvm::BasicBlock* GetBlock(Region* region) {
    return GetBlock(region->Id());
  }
  // Records @block as corresponding to the sea_ir::Region with id @region_id.
  void AddBlock(int region_id, llvm::BasicBlock* block) {
    blocks_.insert(std::pair<int, llvm::BasicBlock*>(region_id, block));
  }
  // Records @block as corresponding to the sea_ir::Region with @region.
  void AddBlock(Region* region, llvm::BasicBlock* block) {
    AddBlock(region->Id(), block);
  }

  llvm::Value* GetValue(int instruction_id) {
    std::map<int, llvm::Value*>::iterator value_it = values_.find(instruction_id);
    DCHECK(value_it != values_.end());
    return value_it->second;
  }
  // Returns the llvm::Value* corresponding to the output of @instruction.
  llvm::Value* GetValue(InstructionNode* instruction) {
    return GetValue(instruction->Id());
  }
  // Records @value as corresponding to the sea_ir::InstructionNode with id @instruction_id.
  void AddValue(int instruction_id, llvm::Value* value) {
    values_.insert(std::pair<int, llvm::Value*>(instruction_id, value));
  }
  // Records @value as corresponding to the sea_ir::InstructionNode  @instruction.
  void AddValue(InstructionNode* instruction, llvm::Value* value) {
      AddValue(instruction->Id(), value);
  }
  // Generates and returns in @elf the executable code corresponding to the llvm module
  //
  std::string GetElf(art::InstructionSet instruction_set);

  llvm::LLVMContext* const context_;
  llvm::Module module_;
  llvm::IRBuilder<> builder_;
  llvm::Function* function_;

 private:
  std::map<int, llvm::BasicBlock*> blocks_;
  std::map<int, llvm::Value*> values_;
};

class CodeGenPassVisitor: public IRVisitor {
 public:
  explicit CodeGenPassVisitor(CodeGenData* cgd): llvm_data_(cgd) { }
  CodeGenPassVisitor(): llvm_data_(new CodeGenData()) { }
  // Initialize any data structure needed before the start of visiting.
  virtual void Initialize(SeaGraph* graph);
  CodeGenData* GetData() {
    return llvm_data_;
  }
  void Write(std::string file) {
      llvm_data_->module_.dump();
      llvm::verifyFunction(*llvm_data_->function_);
    }

 protected:
  CodeGenData* const llvm_data_;
};

class CodeGenPrepassVisitor: public CodeGenPassVisitor {
 public:
  explicit CodeGenPrepassVisitor(const std::string& function_name):
    function_name_(function_name) { }
  void Visit(SeaGraph* graph);
  void Visit(SignatureNode* region);
  void Visit(Region* region);
  void Visit(InstructionNode* instruction) { }

  void Visit(UnnamedConstInstructionNode* instruction) { }
  void Visit(ConstInstructionNode* instruction) { }
  void Visit(ReturnInstructionNode* instruction) { }
  void Visit(IfNeInstructionNode* instruction) { }
  // void Visit(AddIntLitInstructionNode* instruction) { }
  void Visit(MoveResultInstructionNode* instruction) { }
  void Visit(InvokeStaticInstructionNode* instruction) { }
  void Visit(AddIntInstructionNode* instruction) { }
  void Visit(GotoInstructionNode* instruction) { }
  void Visit(IfEqzInstructionNode* instruction) { }
  void Visit(PhiInstructionNode* region);

 private:
  std::string function_name_;
};

class CodeGenPostpassVisitor: public CodeGenPassVisitor {
 public:
  explicit CodeGenPostpassVisitor(CodeGenData* code_gen_data): CodeGenPassVisitor(code_gen_data) { }
  void Visit(SeaGraph* graph);
  void Visit(SignatureNode* region);
  void Visit(Region* region);
  void Visit(InstructionNode* region) { }
  void Visit(UnnamedConstInstructionNode* instruction) { }
  void Visit(ConstInstructionNode* instruction) { }
  void Visit(ReturnInstructionNode* instruction) { }
  void Visit(IfNeInstructionNode* instruction) { }
  // void Visit(AddIntLitInstructionNode* instruction) { }
  void Visit(MoveResultInstructionNode* instruction) { }
  void Visit(InvokeStaticInstructionNode* instruction) { }
  void Visit(AddIntInstructionNode* instruction) { }
  void Visit(GotoInstructionNode* instruction) { }
  void Visit(IfEqzInstructionNode* instruction) { }
  void Visit(PhiInstructionNode* region);
};

class CodeGenVisitor: public CodeGenPassVisitor {
 public:
  explicit CodeGenVisitor(CodeGenData* code_gen_data,
      const art::DexFile& dex_file): CodeGenPassVisitor(code_gen_data), dex_file_(dex_file) { }
  void Visit(SeaGraph* graph);
  void Visit(SignatureNode* region);
  void Visit(Region* region);
  void Visit(InstructionNode* region);
  void Visit(UnnamedConstInstructionNode* instruction);
  void Visit(ConstInstructionNode* instruction);
  void Visit(ReturnInstructionNode* instruction);
  void Visit(IfNeInstructionNode* instruction);
  void Visit(MoveResultInstructionNode* instruction);
  void Visit(InvokeStaticInstructionNode* instruction);
  void Visit(AddIntInstructionNode* instruction);
  void Visit(GotoInstructionNode* instruction);
  void Visit(IfEqzInstructionNode* instruction);
  void Visit(PhiInstructionNode* region) { }

 private:
  std::string function_name_;
  const art::DexFile& dex_file_;
};
}  // namespace sea_ir
#endif  // ART_COMPILER_SEA_IR_CODE_GEN_CODE_GEN_H_
