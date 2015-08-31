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

#ifndef ART_RUNTIME_MIRROR_ART_METHOD_INL_H_
#define ART_RUNTIME_MIRROR_ART_METHOD_INL_H_

#include "art_method.h"

#include "dex_file.h"
#include "entrypoints/entrypoint_utils.h"
#include "object_array.h"
#include "runtime.h"

namespace art {
namespace mirror {

inline Class* ArtMethod::GetDeclaringClass() const {
  Class* result = GetFieldObject<Class*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, declaring_class_), false);
  DCHECK(result != NULL) << this;
  DCHECK(result->IsIdxLoaded() || result->IsErroneous()) << this;
  return result;
}

inline void ArtMethod::SetDeclaringClass(Class *new_declaring_class) {
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(ArtMethod, declaring_class_), new_declaring_class, false);
}

inline uint32_t ArtMethod::GetAccessFlags() const {
  DCHECK(GetDeclaringClass()->IsIdxLoaded() || GetDeclaringClass()->IsErroneous());
  return GetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, access_flags_), false);
}

inline uint16_t ArtMethod::GetMethodIndex() const {
  DCHECK(GetDeclaringClass()->IsResolved() || GetDeclaringClass()->IsErroneous());
  return GetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, method_index_), false);
}

inline uint32_t ArtMethod::GetDexMethodIndex() const {
#ifdef ART_SEA_IR_MODE
  // TODO: Re-add this check for (PORTABLE + SMALL + ) SEA IR when PORTABLE IS fixed!
  // DCHECK(GetDeclaringClass()->IsLoaded() || GetDeclaringClass()->IsErroneous());
#else
  DCHECK(GetDeclaringClass()->IsLoaded() || GetDeclaringClass()->IsErroneous());
#endif
  return GetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, method_dex_index_), false);
}

inline ObjectArray<String>* ArtMethod::GetDexCacheStrings() const {
  return GetFieldObject<ObjectArray<String>*>(
      OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_cache_strings_), false);
}

inline ObjectArray<ArtMethod>* ArtMethod::GetDexCacheResolvedMethods() const {
  return GetFieldObject<ObjectArray<ArtMethod>*>(
      OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_cache_resolved_methods_), false);
}

inline ObjectArray<Class>* ArtMethod::GetDexCacheResolvedTypes() const {
  return GetFieldObject<ObjectArray<Class>*>(
      OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_cache_resolved_types_), false);
}

inline ObjectArray<StaticStorageBase>* ArtMethod::GetDexCacheInitializedStaticStorage() const {
  return GetFieldObject<ObjectArray<StaticStorageBase>*>(
      OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_cache_initialized_static_storage_),
      false);
}

inline uint32_t ArtMethod::GetCodeSize() const {
  DCHECK(!IsRuntimeMethod() && !IsProxyMethod()) << PrettyMethod(this);
  uintptr_t code = reinterpret_cast<uintptr_t>(GetEntryPointFromCompiledCode());
  if (code == 0) {
    return 0;
  }
  // TODO: make this Thumb2 specific
  code &= ~0x1;
  return reinterpret_cast<uint32_t*>(code)[-1];
}

inline bool ArtMethod::CheckIncompatibleClassChange(InvokeType type) {
  switch (type) {
    case kStatic:
      return !IsStatic();
    case kDirect:
      return !IsDirect() || IsStatic();
    case kVirtual: {
      Class* methods_class = GetDeclaringClass();
      return IsDirect() || (methods_class->IsInterface() && !IsMiranda());
    }
    case kSuper:
      return false;  // TODO: appropriate checks for call to super class.
    case kInterface: {
      Class* methods_class = GetDeclaringClass();
      return IsDirect() || !(methods_class->IsInterface() || methods_class->IsObjectClass());
    }
    default:
      LOG(FATAL) << "Unreachable - invocation type: " << type;
      return true;
  }
}

inline void ArtMethod::AssertPcIsWithinCode(uintptr_t pc) const {
  if (!kIsDebugBuild) {
    return;
  }
  if (IsNative() || IsRuntimeMethod() || IsProxyMethod()) {
    return;
  }
  if (pc == GetQuickInstrumentationExitPc()) {
    return;
  }
  const void* code = GetEntryPointFromCompiledCode();
  if (code == GetCompiledCodeToInterpreterBridge() || code == GetQuickInstrumentationEntryPoint()) {
    return;
  }
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  if (code == GetResolutionTrampoline(class_linker)) {
    return;
  }
  DCHECK(IsWithinCode(pc))
      << PrettyMethod(this)
      << " pc=" << std::hex << pc
      << " code=" << code
      << " size=" << GetCodeSize();
}

inline uint32_t ArtMethod::GetOatCodeOffset() const {
  DCHECK(!Runtime::Current()->IsStarted());
  return reinterpret_cast<uint32_t>(GetEntryPointFromCompiledCode());
}

inline void ArtMethod::SetOatCodeOffset(uint32_t code_offset) {
  DCHECK(!Runtime::Current()->IsStarted());
  SetEntryPointFromCompiledCode(reinterpret_cast<void*>(code_offset));
}

inline uint32_t ArtMethod::GetOatMappingTableOffset() const {
  DCHECK(!Runtime::Current()->IsStarted());
  return reinterpret_cast<uint32_t>(GetMappingTable());
}

inline void ArtMethod::SetOatMappingTableOffset(uint32_t mapping_table_offset) {
  DCHECK(!Runtime::Current()->IsStarted());
  SetMappingTable(reinterpret_cast<const uint8_t*>(mapping_table_offset));
}

inline uint32_t ArtMethod::GetOatVmapTableOffset() const {
  DCHECK(!Runtime::Current()->IsStarted());
  return reinterpret_cast<uint32_t>(GetVmapTable());
}

inline void ArtMethod::SetOatVmapTableOffset(uint32_t vmap_table_offset) {
  DCHECK(!Runtime::Current()->IsStarted());
  SetVmapTable(reinterpret_cast<uint8_t*>(vmap_table_offset));
}

inline void ArtMethod::SetOatNativeGcMapOffset(uint32_t gc_map_offset) {
  DCHECK(!Runtime::Current()->IsStarted());
  SetNativeGcMap(reinterpret_cast<uint8_t*>(gc_map_offset));
}

inline uint32_t ArtMethod::GetOatNativeGcMapOffset() const {
  DCHECK(!Runtime::Current()->IsStarted());
  return reinterpret_cast<uint32_t>(GetNativeGcMap());
}

inline bool ArtMethod::IsRuntimeMethod() const {
  return GetDexMethodIndex() == DexFile::kDexNoIndex;
}

inline bool ArtMethod::IsCalleeSaveMethod() const {
  if (!IsRuntimeMethod()) {
    return false;
  }
  Runtime* runtime = Runtime::Current();
  bool result = false;
  for (int i = 0; i < Runtime::kLastCalleeSaveType; i++) {
    if (this == runtime->GetCalleeSaveMethod(Runtime::CalleeSaveType(i))) {
      result = true;
      break;
    }
  }
  return result;
}

inline bool ArtMethod::IsResolutionMethod() const {
  bool result = this == Runtime::Current()->GetResolutionMethod();
  // Check that if we do think it is phony it looks like the resolution method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}
}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ART_METHOD_INL_H_
