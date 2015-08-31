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

#ifndef ART_COMPILER_SEA_IR_IR_VISITOR_H_
#define ART_COMPILER_SEA_IR_IR_VISITOR_H_

namespace sea_ir {

class SeaGraph;
class Region;
class InstructionNode;
class PhiInstructionNode;
class SignatureNode;
class UnnamedConstInstructionNode;
class ConstInstructionNode;
class ReturnInstructionNode;
class IfNeInstructionNode;
class AddIntLit8InstructionNode;
class MoveResultInstructionNode;
class InvokeStaticInstructionNode;
class AddIntInstructionNode;
class AddIntLitInstructionNode;
class GotoInstructionNode;
class IfEqzInstructionNode;




class IRVisitor {
 public:
  explicit IRVisitor(): ordered_regions_() { }
  virtual void Initialize(SeaGraph* graph) = 0;
  virtual void Visit(SeaGraph* graph) = 0;
  virtual void Visit(Region* region) = 0;
  virtual void Visit(PhiInstructionNode* region) = 0;
  virtual void Visit(SignatureNode* region) = 0;

  virtual void Visit(InstructionNode* region) = 0;
  virtual void Visit(ConstInstructionNode* instruction) = 0;
  virtual void Visit(UnnamedConstInstructionNode* instruction) = 0;
  virtual void Visit(ReturnInstructionNode* instruction) = 0;
  virtual void Visit(IfNeInstructionNode* instruction) = 0;
  virtual void Visit(MoveResultInstructionNode* instruction) = 0;
  virtual void Visit(InvokeStaticInstructionNode* instruction) = 0;
  virtual void Visit(AddIntInstructionNode* instruction) = 0;
  virtual void Visit(GotoInstructionNode* instruction) = 0;
  virtual void Visit(IfEqzInstructionNode* instruction) = 0;

  // Note: This flavor of visitor separates the traversal functions from the actual visiting part
  //       so that the Visitor subclasses don't duplicate code and can't get the traversal wrong.
  //       The disadvantage is the increased number of functions (and calls).
  virtual void Traverse(SeaGraph* graph);
  virtual void Traverse(Region* region);
  // The following functions are meant to be empty and not pure virtual,
  // because the parameter classes have no children to traverse.
  virtual void Traverse(InstructionNode* region) { }
  virtual void Traverse(ConstInstructionNode* instruction) { }
  virtual void Traverse(ReturnInstructionNode* instruction) { }
  virtual void Traverse(IfNeInstructionNode* instruction) { }
  virtual void Traverse(AddIntLit8InstructionNode* instruction) { }
  virtual void Traverse(MoveResultInstructionNode* instruction) { }
  virtual void Traverse(InvokeStaticInstructionNode* instruction) { }
  virtual void Traverse(AddIntInstructionNode* instruction) { }
  virtual void Traverse(GotoInstructionNode* instruction) { }
  virtual void Traverse(IfEqzInstructionNode* instruction) { }
  virtual void Traverse(PhiInstructionNode* phi) { }
  virtual void Traverse(SignatureNode* sig) { }
  virtual ~IRVisitor() { }

 protected:
  std::vector<Region*> ordered_regions_;
};
}  // namespace sea_ir
#endif  // ART_COMPILER_SEA_IR_IR_VISITOR_H_
