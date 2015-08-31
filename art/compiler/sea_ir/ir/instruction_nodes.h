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

#ifndef ART_COMPILER_SEA_IR_IR_INSTRUCTION_NODES_H_
#define ART_COMPILER_SEA_IR_IR_INSTRUCTION_NODES_H_
#include "dex_instruction-inl.h"
#include "sea_ir/ir/sea_node.h"
#include "sea_ir/ir/visitor.h"


namespace sea_ir {

enum SpecialRegisters {
  NO_REGISTER = -1,             // Usually signifies that there is no register
                                // that respects the condition you asked for.
  RETURN_REGISTER = -2,         // Written by the invoke* instructions, read by move-results.
  UNNAMED_CONST_REGISTER = -3   // Written by UnnamedConst* instructions, read by *Lit* instruction.
};

class IRVisitor;

// This class represents an instruction in SEA IR.
// As we add support for specific classes of instructions,
// the number of InstructionNode objects should dwindle, while the
// number of subclasses and instances of subclasses will go up.
class InstructionNode: public SeaNode {
 public:
  static std::vector<sea_ir::InstructionNode*> Create(const art::Instruction* in);
  // Returns the Dalvik instruction around which this InstructionNode is wrapped.
  const art::Instruction* GetInstruction() const {
    DCHECK(NULL != instruction_) << "Tried to access NULL instruction in an InstructionNode.";
    return instruction_;
  }
  // Returns the register that is defined by the current instruction, or NO_REGISTER otherwise.
  virtual int GetResultRegister() const;
  // Returns the set of registers defined by the current instruction.
  virtual std::vector<int> GetDefinitions() const;
  // Returns the set of register numbers that are used by the instruction.
  virtual std::vector<int> GetUses() const;
  // Mark the current instruction as a downward exposed definition.
  void MarkAsDEDef();
  // Rename the use of @reg_no to refer to the instruction @definition,
  // essentially creating SSA form.
  void RenameToSSA(int reg_no, InstructionNode* definition) {
    definition_edges_.insert(std::pair<int, InstructionNode*>(reg_no, definition));
    DCHECK(NULL != definition) << "SSA definition for register " << reg_no
        << " used in instruction " << Id() << " not found.";
    definition->AddSSAUse(this);
  }
  // Returns the ordered set of Instructions that define the input operands of this instruction.
  // Precondition: SeaGraph.ConvertToSSA().
  virtual std::vector<InstructionNode*> GetSSAProducers() {
    std::vector<int> uses = GetUses();
    std::vector<InstructionNode*> ssa_uses;
    for (std::vector<int>::const_iterator cit = uses.begin(); cit != uses.end(); cit++) {
      ssa_uses.push_back((*definition_edges_.find(*cit)).second);
    }
    return ssa_uses;
  }
  std::map<int, InstructionNode* >* GetSSAProducersMap() {
    return &definition_edges_;
  }
  std::vector<InstructionNode*>* GetSSAConsumers() {
    return &used_in_;
  }
  virtual void AddSSAUse(InstructionNode* use) {
    used_in_.push_back(use);
  }
  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }
  // Set the region to which this instruction belongs.
  Region* GetRegion() {
    DCHECK(NULL != region_);
    return region_;
  }
  // Get the region to which this instruction belongs.
  void SetRegion(Region* region) {
    region_ = region;
  }

 protected:
  explicit InstructionNode(const art::Instruction* in):
      SeaNode(), instruction_(in), used_in_(), de_def_(false), region_(NULL) { }

 protected:
  const art::Instruction* const instruction_;
  std::map<int, InstructionNode* > definition_edges_;  // Maps used registers to their definitions.
  // Stores pointers to instructions that use the result of the current instruction.
  std::vector<InstructionNode*> used_in_;
  bool de_def_;
  Region* region_;
};

class ConstInstructionNode: public InstructionNode {
 public:
  explicit ConstInstructionNode(const art::Instruction* inst):
      InstructionNode(inst) { }

  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }

  virtual int32_t GetConstValue() const {
    return GetInstruction()->VRegB_11n();
  }
};

class UnnamedConstInstructionNode: public ConstInstructionNode {
 public:
  explicit UnnamedConstInstructionNode(const art::Instruction* inst, int32_t value):
      ConstInstructionNode(inst), value_(value) { }

  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }

  int GetResultRegister() const {
    return UNNAMED_CONST_REGISTER;
  }

  int32_t GetConstValue() const {
    return value_;
  }

 private:
  const int32_t value_;
};

class ReturnInstructionNode: public InstructionNode {
 public:
  explicit ReturnInstructionNode(const art::Instruction* inst): InstructionNode(inst) { }
  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }
};

class IfNeInstructionNode: public InstructionNode {
 public:
  explicit IfNeInstructionNode(const art::Instruction* inst): InstructionNode(inst) {
    DCHECK(InstructionTools::IsDefinition(inst) == false);
  }
  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }
};



class MoveResultInstructionNode: public InstructionNode {
 public:
  explicit MoveResultInstructionNode(const art::Instruction* inst): InstructionNode(inst) { }
  std::vector<int> GetUses() const {
    std::vector<int> uses;  // Using vector<> instead of set<> because order matters.
    uses.push_back(RETURN_REGISTER);
    return uses;
  }
  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }
};

class InvokeStaticInstructionNode: public InstructionNode {
 public:
  explicit InvokeStaticInstructionNode(const art::Instruction* inst): InstructionNode(inst),
    method_index_(inst->VRegB_35c()) { }
  int GetResultRegister() const {
    return RETURN_REGISTER;
  }

  int GetCalledMethodIndex() const {
    return method_index_;
  }
  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }

 private:
  const uint32_t method_index_;
};

class AddIntInstructionNode: public InstructionNode {
 public:
  explicit AddIntInstructionNode(const art::Instruction* inst): InstructionNode(inst) { }
  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }
};

class AddIntLitInstructionNode: public AddIntInstructionNode {
 public:
  explicit AddIntLitInstructionNode(const art::Instruction* inst):
      AddIntInstructionNode(inst) { }

  std::vector<int> GetUses() const {
    std::vector<int> uses =  AddIntInstructionNode::GetUses();
    uses.push_back(UNNAMED_CONST_REGISTER);
    return uses;
    }

  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }
};

class GotoInstructionNode: public InstructionNode {
 public:
  explicit GotoInstructionNode(const art::Instruction* inst): InstructionNode(inst) { }
  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }
};

class IfEqzInstructionNode: public InstructionNode {
 public:
  explicit IfEqzInstructionNode(const art::Instruction* inst): InstructionNode(inst) {
    DCHECK(InstructionTools::IsDefinition(inst) == false);
  }
  void Accept(IRVisitor* v) {
    v->Visit(this);
    v->Traverse(this);
  }
};
}  // namespace sea_ir
#endif  // ART_COMPILER_SEA_IR_IR_INSTRUCTION_NODES_H_
