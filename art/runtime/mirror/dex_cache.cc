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

#include "dex_cache.h"

#include "art_method-inl.h"
#include "base/logging.h"
#include "class_linker.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "globals.h"
#include "object.h"
#include "object-inl.h"
#include "object_array-inl.h"
#include "runtime.h"
#include "string.h"

namespace art {
namespace mirror {

void DexCache::Init(const DexFile* dex_file,
                    String* location,
                    ObjectArray<String>* strings,
                    ObjectArray<Class>* resolved_types,
                    ObjectArray<ArtMethod>* resolved_methods,
                    ObjectArray<ArtField>* resolved_fields,
                    ObjectArray<StaticStorageBase>* initialized_static_storage) {
  CHECK(dex_file != NULL);
  CHECK(location != NULL);
  CHECK(strings != NULL);
  CHECK(resolved_types != NULL);
  CHECK(resolved_methods != NULL);
  CHECK(resolved_fields != NULL);
  CHECK(initialized_static_storage != NULL);

  SetFieldPtr(OFFSET_OF_OBJECT_MEMBER(DexCache, dex_file_), dex_file, false);
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(DexCache, location_), location, false);
  SetFieldObject(StringsOffset(), strings, false);
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(DexCache, resolved_types_), resolved_types, false);
  SetFieldObject(ResolvedMethodsOffset(), resolved_methods, false);
  SetFieldObject(ResolvedFieldsOffset(), resolved_fields, false);
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(DexCache, initialized_static_storage_),
                 initialized_static_storage, false);

  Runtime* runtime = Runtime::Current();
  if (runtime->HasResolutionMethod()) {
    // Initialize the resolve methods array to contain trampolines for resolution.
    ArtMethod* trampoline = runtime->GetResolutionMethod();
    size_t length = resolved_methods->GetLength();
    for (size_t i = 0; i < length; i++) {
      resolved_methods->SetWithoutChecks(i, trampoline);
    }
  }
}

void DexCache::Fixup(ArtMethod* trampoline) {
  // Fixup the resolve methods array to contain trampoline for resolution.
  CHECK(trampoline != NULL);
  ObjectArray<ArtMethod>* resolved_methods = GetResolvedMethods();
  size_t length = resolved_methods->GetLength();
  for (size_t i = 0; i < length; i++) {
    if (resolved_methods->GetWithoutChecks(i) == NULL) {
      resolved_methods->SetWithoutChecks(i, trampoline);
    }
  }
}

}  // namespace mirror
}  // namespace art
