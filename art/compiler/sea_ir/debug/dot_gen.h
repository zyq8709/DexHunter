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

#ifndef ART_COMPILER_SEA_IR_DEBUG_DOT_GEN_H_
#define ART_COMPILER_SEA_IR_DEBUG_DOT_GEN_H_

#include "safe_map.h"
#include "base/stringprintf.h"
#include "file_output_stream.h"
#include "os.h"
#include "sea_ir/ir/sea.h"
#include "sea_ir/types/type_inference.h"

namespace sea_ir {

class DotConversionOptions {
 public:
  DotConversionOptions(): save_use_edges_(false) { }
  bool WillSaveUseEdges() const {
    return save_use_edges_;
  }
 private:
  bool save_use_edges_;
};

class DotGenerationVisitor: public IRVisitor {
 public:
  explicit DotGenerationVisitor(const DotConversionOptions* const options,
      art::SafeMap<int, const Type*>* types): graph_(), types_(types), options_(options) { }

  virtual void Initialize(SeaGraph* graph);
  // Saves the ssa def->use edges corresponding to @instruction.
  void ToDotSSAEdges(InstructionNode* instruction);
  void ToDotSSAEdges(PhiInstructionNode* instruction);
  void Visit(SeaGraph* graph) {
    dot_text_ += "digraph seaOfNodes {\ncompound=true\n";
  }
  void Visit(SignatureNode* parameter);

  // Appends to @result a dot language formatted string representing the node and
  //    (by convention) outgoing edges, so that the composition of theToDot() of all nodes
  //    builds a complete dot graph (without prolog and epilog though).
  void Visit(Region* region);
  void Visit(InstructionNode* instruction);
  void Visit(PhiInstructionNode* phi);
  void Visit(UnnamedConstInstructionNode* instruction);

  void Visit(ConstInstructionNode* instruction) {
    Visit(reinterpret_cast<InstructionNode*>(instruction));
  }
  void Visit(ReturnInstructionNode* instruction) {
    Visit(reinterpret_cast<InstructionNode*>(instruction));
  }
  void Visit(IfNeInstructionNode* instruction) {
    Visit(reinterpret_cast<InstructionNode*>(instruction));
  }
  void Visit(MoveResultInstructionNode* instruction) {
    Visit(reinterpret_cast<InstructionNode*>(instruction));
  }
  void Visit(InvokeStaticInstructionNode* instruction) {
    Visit(reinterpret_cast<InstructionNode*>(instruction));
  }
  void Visit(AddIntInstructionNode* instruction) {
    Visit(reinterpret_cast<InstructionNode*>(instruction));
  }
  void Visit(GotoInstructionNode* instruction) {
    Visit(reinterpret_cast<InstructionNode*>(instruction));
  }
  void Visit(IfEqzInstructionNode* instruction) {
    Visit(reinterpret_cast<InstructionNode*>(instruction));
  }

  std::string GetResult() const {
    return dot_text_;
  }

 private:
  std::string dot_text_;
  SeaGraph* graph_;
  art::SafeMap<int, const Type*>* types_;
  const DotConversionOptions* const options_;
};

// Stores options for turning a SEA IR graph to a .dot file.
class DotConversion {
 public:
  DotConversion(): options_() { }
  // Saves to @filename the .dot representation of @graph with the options @options.
  void DumpSea(SeaGraph* graph, std::string filename,
      art::SafeMap<int, const Type*>* types) const {
    LOG(INFO) << "Starting to write SEA string to file " << filename << std::endl;
    DotGenerationVisitor dgv = DotGenerationVisitor(&options_, types);
    graph->Accept(&dgv);
    // TODO: UniquePtr to close file properly. Switch to BufferedOutputStream.
    art::File* file = art::OS::CreateEmptyFile(filename.c_str());
    art::FileOutputStream fos(file);
    std::string graph_as_string = dgv.GetResult();
    graph_as_string += "}";
    fos.WriteFully(graph_as_string.c_str(), graph_as_string.size());
    LOG(INFO) << "Written SEA string to file.";
  }

 private:
  DotConversionOptions options_;
};

}  // namespace sea_ir
#endif  // ART_COMPILER_SEA_IR_DEBUG_DOT_GEN_H_
