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

#ifndef ART_COMPILER_SEA_IR_IR_SEA_NODE_H_
#define ART_COMPILER_SEA_IR_IR_SEA_NODE_H_

#include "base/stringprintf.h"

namespace sea_ir {
class Region;
class IRVisitor;

class IVisitable {
 public:
  virtual void Accept(IRVisitor* visitor) = 0;
  virtual ~IVisitable() {}
};

// This abstract class provides the essential services that
// we want each SEA IR element to have.
// At the moment, these are:
// - an id and corresponding string representation.
// - a .dot graph language representation for .dot output.
//
// Note that SEA IR nodes could also be Regions, Projects
// which are not instructions.
class SeaNode: public IVisitable {
 public:
  explicit SeaNode():id_(GetNewId()), string_id_() {
    string_id_ = art::StringPrintf("%d", id_);
  }

  // Adds CFG predecessors and successors to each block.
  void AddSuccessor(Region* successor);
  void AddPredecessor(Region* predecesor);

  // Returns the id of the current block as string
  const std::string& StringId() const {
    return string_id_;
  }
  // Returns the id of this node as int. The id is supposed to be unique among
  // all instances of all subclasses of this class.
  int Id() const {
    return id_;
  }

  virtual ~SeaNode() { }

 protected:
  static int GetNewId() {
    return current_max_node_id_++;
  }

  const int id_;
  std::string string_id_;

 private:
  static int current_max_node_id_;
  // Creating new instances of sea node objects should not be done through copy or assignment
  // operators because that would lead to duplication of their unique ids.
  DISALLOW_COPY_AND_ASSIGN(SeaNode);
};
}  // namespace sea_ir
#endif  // ART_COMPILER_SEA_IR_IR_SEA_NODE_H_
