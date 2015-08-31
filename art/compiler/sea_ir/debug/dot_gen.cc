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
#include "sea_ir/debug/dot_gen.h"

namespace sea_ir {

void DotGenerationVisitor::Initialize(SeaGraph* graph) {
  graph_ = graph;
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

void DotGenerationVisitor::ToDotSSAEdges(InstructionNode* instruction) {
  std::map<int, InstructionNode*>* definition_edges = instruction->GetSSAProducersMap();
  // SSA definitions:
  for (std::map<int, InstructionNode*>::const_iterator
      def_it = definition_edges->begin();
      def_it != definition_edges->end(); def_it++) {
    if (NULL != def_it->second) {
      dot_text_ += def_it->second->StringId() + " -> ";
      dot_text_ += instruction->StringId() + "[color=gray,label=\"";
      dot_text_ += art::StringPrintf("vR = %d", def_it->first);
      art::SafeMap<int, const Type*>::const_iterator type_it = types_->find(def_it->second->Id());
      if (type_it != types_->end()) {
        art::ScopedObjectAccess soa(art::Thread::Current());
        dot_text_ += "(" + type_it->second->Dump() + ")";
      } else {
        dot_text_ += "()";
      }
      dot_text_ += "\"] ; // SSA edge\n";
    }
  }

  // SSA used-by:
  if (options_->WillSaveUseEdges()) {
    std::vector<InstructionNode*>* used_in = instruction->GetSSAConsumers();
    for (std::vector<InstructionNode*>::const_iterator cit = used_in->begin();
        cit != used_in->end(); cit++) {
      dot_text_ += (*cit)->StringId() + " -> " + instruction->StringId() + "[color=gray,label=\"";
      dot_text_ += "\"] ; // SSA used-by edge\n";
    }
  }
}

void DotGenerationVisitor::ToDotSSAEdges(PhiInstructionNode* instruction) {
  std::vector<InstructionNode*> definition_edges = instruction->GetSSAProducers();
  // SSA definitions:
  for (std::vector<InstructionNode*>::const_iterator
      def_it = definition_edges.begin();
      def_it != definition_edges.end(); def_it++) {
    if (NULL != *def_it) {
      dot_text_ += (*def_it)->StringId() + " -> ";
      dot_text_ += instruction->StringId() + "[color=gray,label=\"";
      dot_text_ += art::StringPrintf("vR = %d", instruction->GetRegisterNumber());
      art::SafeMap<int, const Type*>::const_iterator type_it = types_->find((*def_it)->Id());
      if (type_it != types_->end()) {
        art::ScopedObjectAccess soa(art::Thread::Current());
        dot_text_ += "(" + type_it->second->Dump() + ")";
      } else {
        dot_text_ += "()";
      }
      dot_text_ += "\"] ; // SSA edge\n";
    }
  }

  // SSA used-by:
  if (options_->WillSaveUseEdges()) {
    std::vector<InstructionNode*>* used_in = instruction->GetSSAConsumers();
    for (std::vector<InstructionNode*>::const_iterator cit = used_in->begin();
        cit != used_in->end(); cit++) {
      dot_text_ += (*cit)->StringId() + " -> " + instruction->StringId() + "[color=gray,label=\"";
      dot_text_ += "\"] ; // SSA used-by edge\n";
    }
  }
}

void DotGenerationVisitor::Visit(SignatureNode* parameter) {
  dot_text_ += parameter->StringId() +" [label=\"[" + parameter->StringId() + "] signature:";
  dot_text_ += art::StringPrintf("r%d", parameter->GetResultRegister());
  dot_text_ += "\"] // signature node\n";
  ToDotSSAEdges(parameter);
}

// Appends to @result a dot language formatted string representing the node and
//    (by convention) outgoing edges, so that the composition of theToDot() of all nodes
//    builds a complete dot graph (without prolog and epilog though).
void DotGenerationVisitor::Visit(Region* region) {
  dot_text_ += "\n// Region: \nsubgraph " + region->StringId();
  dot_text_ += " { label=\"region " + region->StringId() + "(rpo=";
  dot_text_ += art::StringPrintf("%d", region->GetRPO());
  if (NULL != region->GetIDominator()) {
    dot_text_ += " dom=" + region->GetIDominator()->StringId();
  }
  dot_text_ += ")\";\n";

  std::vector<PhiInstructionNode*>* phi_instructions = region->GetPhiNodes();
  for (std::vector<PhiInstructionNode*>::const_iterator cit = phi_instructions->begin();
        cit != phi_instructions->end(); cit++) {
    dot_text_ += (*cit)->StringId() +";\n";
  }
  std::vector<InstructionNode*>* instructions = region->GetInstructions();
  for (std::vector<InstructionNode*>::const_iterator cit = instructions->begin();
        cit != instructions->end(); cit++) {
      dot_text_ += (*cit)->StringId() +";\n";
    }

  dot_text_ += "} // End Region.\n";
  std::vector<Region*>* successors =  region->GetSuccessors();
  for (std::vector<Region*>::const_iterator cit = successors->begin(); cit != successors->end();
      cit++) {
    DCHECK(NULL != *cit) << "Null successor found for SeaNode" <<
        region->GetLastChild()->StringId() << ".";
    dot_text_ += region->GetLastChild()->StringId() + " -> " +
        (*cit)->GetLastChild()->StringId() +
        "[lhead=" + (*cit)->StringId() + ", " + "ltail=" + region->StringId() + "];\n\n";
  }
}
void DotGenerationVisitor::Visit(InstructionNode* instruction) {
  dot_text_ += "// Instruction ("+instruction->StringId()+"): \n" + instruction->StringId() +
      " [label=\"[" + instruction->StringId() + "] " +
      instruction->GetInstruction()->DumpString(graph_->GetDexFile()) + "\"";
  dot_text_ += "];\n";
  ToDotSSAEdges(instruction);
}

void DotGenerationVisitor::Visit(UnnamedConstInstructionNode* instruction) {
  dot_text_ += "// Instruction ("+instruction->StringId()+"): \n" + instruction->StringId() +
        " [label=\"[" + instruction->StringId() + "] const/x v-3, #" +
        art::StringPrintf("%d", instruction->GetConstValue()) + "\"";
  dot_text_ += "];\n";
  ToDotSSAEdges(instruction);
}

void DotGenerationVisitor::Visit(PhiInstructionNode* phi) {
  dot_text_ += "// PhiInstruction: \n" + phi->StringId() +
      " [label=\"[" + phi->StringId() + "] PHI(";
  dot_text_ += art::StringPrintf("%d", phi->GetRegisterNumber());
  dot_text_ += ")\"";
  dot_text_ += "];\n";
  ToDotSSAEdges(phi);
}
}  // namespace sea_ir
