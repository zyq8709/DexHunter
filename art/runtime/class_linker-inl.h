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

#ifndef ART_RUNTIME_CLASS_LINKER_INL_H_
#define ART_RUNTIME_CLASS_LINKER_INL_H_

#include "class_linker.h"

#include "mirror/art_field.h"
#include "mirror/dex_cache.h"
#include "mirror/iftable.h"
#include "mirror/object_array.h"

namespace art {

inline mirror::String* ClassLinker::ResolveString(uint32_t string_idx,
                                           const mirror::ArtMethod* referrer) {
  mirror::String* resolved_string = referrer->GetDexCacheStrings()->Get(string_idx);
  if (UNLIKELY(resolved_string == NULL)) {
    mirror::Class* declaring_class = referrer->GetDeclaringClass();
    mirror::DexCache* dex_cache = declaring_class->GetDexCache();
    const DexFile& dex_file = *dex_cache->GetDexFile();
    resolved_string = ResolveString(dex_file, string_idx, dex_cache);
  }
  return resolved_string;
}

inline mirror::Class* ClassLinker::ResolveType(uint16_t type_idx,
                                               const mirror::ArtMethod* referrer) {
  mirror::Class* resolved_type = referrer->GetDexCacheResolvedTypes()->Get(type_idx);
  if (UNLIKELY(resolved_type == NULL)) {
    mirror::Class* declaring_class = referrer->GetDeclaringClass();
    mirror::DexCache* dex_cache = declaring_class->GetDexCache();
    mirror::ClassLoader* class_loader = declaring_class->GetClassLoader();
    const DexFile& dex_file = *dex_cache->GetDexFile();
    resolved_type = ResolveType(dex_file, type_idx, dex_cache, class_loader);
  }
  return resolved_type;
}

inline mirror::Class* ClassLinker::ResolveType(uint16_t type_idx, const mirror::ArtField* referrer) {
  mirror::Class* declaring_class = referrer->GetDeclaringClass();
  mirror::DexCache* dex_cache = declaring_class->GetDexCache();
  mirror::Class* resolved_type = dex_cache->GetResolvedType(type_idx);
  if (UNLIKELY(resolved_type == NULL)) {
    mirror::ClassLoader* class_loader = declaring_class->GetClassLoader();
    const DexFile& dex_file = *dex_cache->GetDexFile();
    resolved_type = ResolveType(dex_file, type_idx, dex_cache, class_loader);
  }
  return resolved_type;
}

inline mirror::ArtMethod* ClassLinker::ResolveMethod(uint32_t method_idx,
                                                     const mirror::ArtMethod* referrer,
                                                     InvokeType type) {
  mirror::ArtMethod* resolved_method =
      referrer->GetDexCacheResolvedMethods()->Get(method_idx);
  if (UNLIKELY(resolved_method == NULL || resolved_method->IsRuntimeMethod())) {
    mirror::Class* declaring_class = referrer->GetDeclaringClass();
    mirror::DexCache* dex_cache = declaring_class->GetDexCache();
    mirror::ClassLoader* class_loader = declaring_class->GetClassLoader();
    const DexFile& dex_file = *dex_cache->GetDexFile();
    resolved_method = ResolveMethod(dex_file, method_idx, dex_cache, class_loader, referrer, type);
  }
  return resolved_method;
}

inline mirror::ArtField* ClassLinker::ResolveField(uint32_t field_idx,
                                                   const mirror::ArtMethod* referrer,
                                                   bool is_static) {
  mirror::ArtField* resolved_field =
      referrer->GetDeclaringClass()->GetDexCache()->GetResolvedField(field_idx);
  if (UNLIKELY(resolved_field == NULL)) {
    mirror::Class* declaring_class = referrer->GetDeclaringClass();
    mirror::DexCache* dex_cache = declaring_class->GetDexCache();
    mirror::ClassLoader* class_loader = declaring_class->GetClassLoader();
    const DexFile& dex_file = *dex_cache->GetDexFile();
    resolved_field = ResolveField(dex_file, field_idx, dex_cache, class_loader, is_static);
  }
  return resolved_field;
}

template <class T>
inline mirror::ObjectArray<T>* ClassLinker::AllocObjectArray(Thread* self, size_t length) {
  return mirror::ObjectArray<T>::Alloc(self, GetClassRoot(kObjectArrayClass), length);
}

inline mirror::ObjectArray<mirror::Class>* ClassLinker::AllocClassArray(Thread* self,
                                                                        size_t length) {
  return mirror::ObjectArray<mirror::Class>::Alloc(self, GetClassRoot(kClassArrayClass), length);
}

inline mirror::ObjectArray<mirror::String>* ClassLinker::AllocStringArray(Thread* self,
                                                                          size_t length) {
  return mirror::ObjectArray<mirror::String>::Alloc(self, GetClassRoot(kJavaLangStringArrayClass),
                                                    length);
}

inline mirror::ObjectArray<mirror::ArtMethod>* ClassLinker::AllocArtMethodArray(Thread* self,
                                                                                size_t length) {
  return mirror::ObjectArray<mirror::ArtMethod>::Alloc(self,
      GetClassRoot(kJavaLangReflectArtMethodArrayClass), length);
}

inline mirror::IfTable* ClassLinker::AllocIfTable(Thread* self, size_t ifcount) {
  return down_cast<mirror::IfTable*>(
      mirror::IfTable::Alloc(self, GetClassRoot(kObjectArrayClass), ifcount * mirror::IfTable::kMax));
}

inline mirror::ObjectArray<mirror::ArtField>* ClassLinker::AllocArtFieldArray(Thread* self,
                                                                              size_t length) {
  return mirror::ObjectArray<mirror::ArtField>::Alloc(self,
                                                      GetClassRoot(kJavaLangReflectArtFieldArrayClass),
                                                      length);
}

inline mirror::Class* ClassLinker::GetClassRoot(ClassRoot class_root)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(class_roots_ != NULL);
  mirror::Class* klass = class_roots_->Get(class_root);
  DCHECK(klass != NULL);
  return klass;
}

}  // namespace art

#endif  // ART_RUNTIME_CLASS_LINKER_INL_H_
