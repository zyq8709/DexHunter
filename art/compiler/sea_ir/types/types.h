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

#ifndef ART_COMPILER_SEA_IR_TYPES_TYPES_H_
#define ART_COMPILER_SEA_IR_TYPES_TYPES_H_

#include "safe_map.h"
#include "verifier/reg_type.h"
#include "verifier/reg_type_cache.h"

namespace sea_ir {

// TODO: Replace typedef with an actual class implementation when we have more types.
typedef art::verifier::RegType Type;

// Stores information about the result type of each instruction.
// Note: Main purpose is to encapsulate the map<instruction id, type*>,
//       so that we can replace the underlying storage at any time.
class TypeData {
 public:
  art::SafeMap<int, const Type*>* GetTypeMap() {
    return &type_map_;
  }
  // Returns the type associated with instruction with @instruction_id.
  const Type* FindTypeOf(int instruction_id) {
    art::SafeMap<int, const Type*>::const_iterator result_it = type_map_.find(instruction_id);
    if (type_map_.end() != result_it) {
      return result_it->second;
    }
    return NULL;
  }

  // Saves the fact that instruction @instruction_id produces a value of type @type.
  void SetTypeOf(int instruction_id, const Type* type) {
    type_map_.Overwrite(instruction_id, type);
  }

 private:
  art::SafeMap<int, const Type*> type_map_;
};



}  // namespace sea_ir
#endif  // ART_COMPILER_SEA_IR_TYPES_TYPES_H_
