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

#ifndef ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_INL_H_
#define ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_INL_H_

#include "reg_type.h"
#include "reg_type_cache.h"
#include "class_linker.h"

namespace art {
namespace verifier {
template <class Type>
Type* RegTypeCache::CreatePrimitiveTypeInstance(const std::string& descriptor) {
  mirror::Class* klass = NULL;
  // Try loading the class from linker.
  if (!descriptor.empty()) {
    klass = art::Runtime::Current()->GetClassLinker()->FindSystemClass(descriptor.c_str());
  }
  Type* entry = Type::CreateInstance(klass, descriptor, RegTypeCache::primitive_count_);
  RegTypeCache::primitive_count_++;
  return entry;
}

inline const art::verifier::RegType& RegTypeCache::GetFromId(uint16_t id) const {
  DCHECK_LT(id, entries_.size());
  RegType* result = entries_[id];
  DCHECK(result != NULL);
  return *result;
}
}  // namespace verifier
}  // namespace art
#endif  // ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_INL_H_
