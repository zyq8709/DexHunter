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

#ifndef ART_COMPILER_SEA_IR_TYPES_TYPE_INFERENCE_H_
#define ART_COMPILER_SEA_IR_TYPES_TYPE_INFERENCE_H_

#include "safe_map.h"
#include "dex_file-inl.h"
#include "sea_ir/types/types.h"

namespace sea_ir {

class SeaGraph;
class InstructionNode;

// The type inference in SEA IR is different from the verifier in that it is concerned
// with a rich type hierarchy (TODO) usable in optimization and does not perform
// precise verification (which is the job of the verifier).
class TypeInference {
 public:
  TypeInference() : type_cache_(new art::verifier::RegTypeCache(false)) {
  }

  // Computes the types for the method with SEA IR representation provided by @graph.
  void ComputeTypes(SeaGraph* graph);

  art::SafeMap<int, const Type*>* GetTypeMap() {
    return type_data_.GetTypeMap();
  }
  // Returns true if @descriptor corresponds to a primitive type.
  static bool IsPrimitiveDescriptor(char descriptor);
  TypeData type_data_;    // TODO: Make private, add accessor and not publish a SafeMap above.
  art::verifier::RegTypeCache* const type_cache_;    // TODO: Make private.
};

// Stores information about the exact type of  a function.
class FunctionTypeInfo {
 public:
  // Finds method information about the method encoded by a SEA IR graph.
  // @graph provides the input method SEA IR representation.
  // @types provides the input cache of types from which the
  //        parameter types of the function are found.
  FunctionTypeInfo(const SeaGraph* graph, art::verifier::RegTypeCache* types);
  // Finds method information about the method encoded by
  // an invocation instruction in a SEA IR graph.
  // @graph provides the input method SEA IR representation.
  // @inst  is an invocation instruction for the desired method.
  // @types provides the input cache of types from which the
  //        parameter types of the function are found.
  FunctionTypeInfo(const SeaGraph* graph, InstructionNode* inst,
      art::verifier::RegTypeCache* types);
  // Returns the ordered vector of types corresponding to the function arguments.
  std::vector<const Type*> GetDeclaredArgumentTypes();
  // Returns the declared return value type.
  const Type* GetReturnValueType();
  // Returns the type corresponding to the class that declared the method.
  const Type& GetDeclaringClass() {
    return *declaring_class_;
  }

  bool IsConstructor() const {
    return (method_access_flags_ & kAccConstructor) != 0;
  }

  bool IsStatic() const {
    return (method_access_flags_ & kAccStatic) != 0;
  }

 protected:
  const Type* declaring_class_;
  const art::DexFile* dex_file_;
  const uint32_t dex_method_idx_;
  art::verifier::RegTypeCache* type_cache_;
  const uint32_t method_access_flags_;  // Method's access flags.
};
}  // namespace sea_ir

#endif  // ART_COMPILER_SEA_IR_TYPES_TYPE_INFERENCE_H_
