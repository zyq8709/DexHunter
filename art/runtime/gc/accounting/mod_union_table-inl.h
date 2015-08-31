/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_INL_H_
#define ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_INL_H_

#include "mod_union_table.h"

#include "gc/space/space.h"

namespace art {
namespace gc {
namespace accounting {

// A mod-union table to record image references to the Zygote and alloc space.
class ModUnionTableToZygoteAllocspace : public ModUnionTableReferenceCache {
 public:
  explicit ModUnionTableToZygoteAllocspace(Heap* heap) : ModUnionTableReferenceCache(heap) {}

  bool AddReference(const mirror::Object* /* obj */, const mirror::Object* ref) {
    const std::vector<space::ContinuousSpace*>& spaces = GetHeap()->GetContinuousSpaces();
    typedef std::vector<space::ContinuousSpace*>::const_iterator It;
    for (It it = spaces.begin(); it != spaces.end(); ++it) {
      if ((*it)->Contains(ref)) {
        return (*it)->IsDlMallocSpace();
      }
    }
    // Assume it points to a large object.
    // TODO: Check.
    return true;
  }
};

// A mod-union table to record Zygote references to the alloc space.
class ModUnionTableToAllocspace : public ModUnionTableReferenceCache {
 public:
  explicit ModUnionTableToAllocspace(Heap* heap) : ModUnionTableReferenceCache(heap) {}

  bool AddReference(const mirror::Object* /* obj */, const mirror::Object* ref) {
    const std::vector<space::ContinuousSpace*>& spaces = GetHeap()->GetContinuousSpaces();
    typedef std::vector<space::ContinuousSpace*>::const_iterator It;
    for (It it = spaces.begin(); it != spaces.end(); ++it) {
      space::ContinuousSpace* space = *it;
      if (space->Contains(ref)) {
        // The allocation space is always considered for collection whereas the Zygote space is
        //
        return space->GetGcRetentionPolicy() == space::kGcRetentionPolicyAlwaysCollect;
      }
    }
    // Assume it points to a large object.
    // TODO: Check.
    return true;
  }
};

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_INL_H_
