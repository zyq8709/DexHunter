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

#ifndef ART_RUNTIME_MIRROR_ART_METHOD_H_
#define ART_RUNTIME_MIRROR_ART_METHOD_H_

#include "class.h"
#include "dex_file.h"
#include "invoke_type.h"
#include "locks.h"
#include "modifiers.h"
#include "object.h"

namespace art {

struct ArtMethodOffsets;
struct ConstructorMethodOffsets;
union JValue;
struct MethodClassOffsets;
class MethodHelper;
class StringPiece;
class ShadowFrame;

namespace mirror {

class StaticStorageBase;

typedef void (EntryPointFromInterpreter)(Thread* self, MethodHelper& mh,
    const DexFile::CodeItem* code_item, ShadowFrame* shadow_frame, JValue* result);

// C++ mirror of java.lang.reflect.Method and java.lang.reflect.Constructor
class MANAGED ArtMethod : public Object {
 public:
  Class* GetDeclaringClass() const;

  void SetDeclaringClass(Class *new_declaring_class) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset DeclaringClassOffset() {
    return MemberOffset(OFFSETOF_MEMBER(ArtMethod, declaring_class_));
  }

  static MemberOffset EntryPointFromCompiledCodeOffset() {
    return MemberOffset(OFFSETOF_MEMBER(ArtMethod, entry_point_from_compiled_code_));
  }

  uint32_t GetAccessFlags() const;

  void SetAccessFlags(uint32_t new_access_flags) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, access_flags_), new_access_flags, false);
  }

  // Approximate what kind of method call would be used for this method.
  InvokeType GetInvokeType() const;

  // Returns true if the method is declared public.
  bool IsPublic() const {
    return (GetAccessFlags() & kAccPublic) != 0;
  }

  // Returns true if the method is declared private.
  bool IsPrivate() const {
    return (GetAccessFlags() & kAccPrivate) != 0;
  }

  // Returns true if the method is declared static.
  bool IsStatic() const {
    return (GetAccessFlags() & kAccStatic) != 0;
  }

  // Returns true if the method is a constructor.
  bool IsConstructor() const {
    return (GetAccessFlags() & kAccConstructor) != 0;
  }

  // Returns true if the method is static, private, or a constructor.
  bool IsDirect() const {
    return IsDirect(GetAccessFlags());
  }

  static bool IsDirect(uint32_t access_flags) {
    return (access_flags & (kAccStatic | kAccPrivate | kAccConstructor)) != 0;
  }

  // Returns true if the method is declared synchronized.
  bool IsSynchronized() const {
    uint32_t synchonized = kAccSynchronized | kAccDeclaredSynchronized;
    return (GetAccessFlags() & synchonized) != 0;
  }

  bool IsFinal() const {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  bool IsMiranda() const {
    return (GetAccessFlags() & kAccMiranda) != 0;
  }

  bool IsNative() const {
    return (GetAccessFlags() & kAccNative) != 0;
  }

  bool IsAbstract() const {
    return (GetAccessFlags() & kAccAbstract) != 0;
  }

  bool IsSynthetic() const {
    return (GetAccessFlags() & kAccSynthetic) != 0;
  }

  bool IsProxyMethod() const;

  bool IsPreverified() const {
    return (GetAccessFlags() & kAccPreverified) != 0;
  }

  void SetPreverified() {
    SetAccessFlags(GetAccessFlags() | kAccPreverified);
  }

  bool CheckIncompatibleClassChange(InvokeType type) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uint16_t GetMethodIndex() const;

  size_t GetVtableIndex() const {
    return GetMethodIndex();
  }

  void SetMethodIndex(uint16_t new_method_index) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, method_index_), new_method_index, false);
  }

  static MemberOffset MethodIndexOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, method_index_);
  }

  uint32_t GetCodeItemOffset() const {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, code_item_offset_), false);
  }

  void SetCodeItemOffset(uint32_t new_code_off) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, code_item_offset_), new_code_off, false);
  }

  // Number of 32bit registers that would be required to hold all the arguments
  static size_t NumArgRegisters(const StringPiece& shorty);

  uint32_t GetDexMethodIndex() const;

  void SetDexMethodIndex(uint32_t new_idx) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, method_dex_index_), new_idx, false);
  }

  ObjectArray<String>* GetDexCacheStrings() const;
  void SetDexCacheStrings(ObjectArray<String>* new_dex_cache_strings)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset DexCacheStringsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_cache_strings_);
  }

  static MemberOffset DexCacheResolvedMethodsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_cache_resolved_methods_);
  }

  static MemberOffset DexCacheResolvedTypesOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_cache_resolved_types_);
  }

  static MemberOffset DexCacheInitializedStaticStorageOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod,
        dex_cache_initialized_static_storage_);
  }

  ObjectArray<ArtMethod>* GetDexCacheResolvedMethods() const;
  void SetDexCacheResolvedMethods(ObjectArray<ArtMethod>* new_dex_cache_methods)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ObjectArray<Class>* GetDexCacheResolvedTypes() const;
  void SetDexCacheResolvedTypes(ObjectArray<Class>* new_dex_cache_types)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ObjectArray<StaticStorageBase>* GetDexCacheInitializedStaticStorage() const;
  void SetDexCacheInitializedStaticStorage(ObjectArray<StaticStorageBase>* new_value)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Find the method that this method overrides
  ArtMethod* FindOverriddenMethod() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Invoke(Thread* self, uint32_t* args, uint32_t args_size, JValue* result, char result_type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  EntryPointFromInterpreter* GetEntryPointFromInterpreter() const {
    return GetFieldPtr<EntryPointFromInterpreter*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, entry_point_from_interpreter_), false);
  }

  void SetEntryPointFromInterpreter(EntryPointFromInterpreter* entry_point_from_interpreter) {
    SetFieldPtr<EntryPointFromInterpreter*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, entry_point_from_interpreter_), entry_point_from_interpreter, false);
  }

  const void* GetEntryPointFromCompiledCode() const {
    return GetFieldPtr<const void*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, entry_point_from_compiled_code_), false);
  }

  void SetEntryPointFromCompiledCode(const void* entry_point_from_compiled_code) {
    SetFieldPtr<const void*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, entry_point_from_compiled_code_), entry_point_from_compiled_code, false);
  }

  uint32_t GetCodeSize() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsWithinCode(uintptr_t pc) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    uintptr_t code = reinterpret_cast<uintptr_t>(GetEntryPointFromCompiledCode());
    if (code == 0) {
      return pc == 0;
    }
    /*
     * During a stack walk, a return PC may point to the end of the code + 1
     * (in the case that the last instruction is a call that isn't expected to
     * return.  Thus, we check <= code + GetCodeSize().
     */
    return (code <= pc && pc <= code + GetCodeSize());
  }

  void AssertPcIsWithinCode(uintptr_t pc) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uint32_t GetOatCodeOffset() const;

  void SetOatCodeOffset(uint32_t code_offset);

  static MemberOffset GetEntryPointFromCompiledCodeOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, entry_point_from_compiled_code_);
  }

  // Callers should wrap the uint8_t* in a MappingTable instance for convenient access.
  const uint8_t* GetMappingTable() const {
    return GetFieldPtr<const uint8_t*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, mapping_table_), false);
  }

  void SetMappingTable(const uint8_t* mapping_table) {
    SetFieldPtr<const uint8_t*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, mapping_table_),
                                 mapping_table, false);
  }

  uint32_t GetOatMappingTableOffset() const;

  void SetOatMappingTableOffset(uint32_t mapping_table_offset);

  // Callers should wrap the uint8_t* in a VmapTable instance for convenient access.
  const uint8_t* GetVmapTable() const {
    return GetFieldPtr<const uint8_t*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, vmap_table_), false);
  }

  void SetVmapTable(const uint8_t* vmap_table) {
    SetFieldPtr<const uint8_t*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, vmap_table_), vmap_table, false);
  }

  uint32_t GetOatVmapTableOffset() const;

  void SetOatVmapTableOffset(uint32_t vmap_table_offset);

  const uint8_t* GetNativeGcMap() const {
    return GetFieldPtr<uint8_t*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, gc_map_), false);
  }
  void SetNativeGcMap(const uint8_t* data) {
    SetFieldPtr<const uint8_t*>(OFFSET_OF_OBJECT_MEMBER(ArtMethod, gc_map_), data, false);
  }

  // When building the oat need a convenient place to stuff the offset of the native GC map.
  void SetOatNativeGcMapOffset(uint32_t gc_map_offset);
  uint32_t GetOatNativeGcMapOffset() const;

  size_t GetFrameSizeInBytes() const {
    DCHECK_EQ(sizeof(size_t), sizeof(uint32_t));
    size_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, frame_size_in_bytes_), false);
    DCHECK_LE(static_cast<size_t>(kStackAlignment), result);
    return result;
  }

  void SetFrameSizeInBytes(size_t new_frame_size_in_bytes) {
    DCHECK_EQ(sizeof(size_t), sizeof(uint32_t));
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, frame_size_in_bytes_),
               new_frame_size_in_bytes, false);
  }

  size_t GetReturnPcOffsetInBytes() const {
    return GetFrameSizeInBytes() - kPointerSize;
  }

  size_t GetSirtOffsetInBytes() const {
    CHECK(IsNative());
    return kPointerSize;
  }

  bool IsRegistered() const;

  void RegisterNative(Thread* self, const void* native_method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void UnregisterNative(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset NativeMethodOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, native_method_);
  }

  const void* GetNativeMethod() const {
    return reinterpret_cast<const void*>(GetField32(NativeMethodOffset(), false));
  }

  void SetNativeMethod(const void*);

  static MemberOffset GetMethodIndexOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, method_index_);
  }

  uint32_t GetCoreSpillMask() const {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, core_spill_mask_), false);
  }

  void SetCoreSpillMask(uint32_t core_spill_mask) {
    // Computed during compilation
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, core_spill_mask_), core_spill_mask, false);
  }

  uint32_t GetFpSpillMask() const {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, fp_spill_mask_), false);
  }

  void SetFpSpillMask(uint32_t fp_spill_mask) {
    // Computed during compilation
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtMethod, fp_spill_mask_), fp_spill_mask, false);
  }

  // Is this a CalleSaveMethod or ResolutionMethod and therefore doesn't adhere to normal
  // conventions for a method of managed code. Returns false for Proxy methods.
  bool IsRuntimeMethod() const;

  // Is this a hand crafted method used for something like describing callee saves?
  bool IsCalleeSaveMethod() const;

  bool IsResolutionMethod() const;

  uintptr_t NativePcOffset(const uintptr_t pc) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Converts a native PC to a dex PC.
  uint32_t ToDexPc(const uintptr_t pc) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Converts a dex PC to a native PC.
  uintptr_t ToNativePc(const uint32_t dex_pc) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Find the catch block for the given exception type and dex_pc. When a catch block is found,
  // indicates whether the found catch block is responsible for clearing the exception or whether
  // a move-exception instruction is present.
  uint32_t FindCatchBlock(Class* exception_type, uint32_t dex_pc, bool* has_no_move_exception) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void SetClass(Class* java_lang_reflect_ArtMethod);

  static Class* GetJavaLangReflectArtMethod() {
    return java_lang_reflect_ArtMethod_;
  }

  static void ResetClass();

 protected:
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  // The class we are a part of
  Class* declaring_class_;

  // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
  ObjectArray<StaticStorageBase>* dex_cache_initialized_static_storage_;

  // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
  ObjectArray<ArtMethod>* dex_cache_resolved_methods_;

  // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
  ObjectArray<Class>* dex_cache_resolved_types_;

  // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
  ObjectArray<String>* dex_cache_strings_;

  // Access flags; low 16 bits are defined by spec.
  uint32_t access_flags_;

  // Offset to the CodeItem.
  uint32_t code_item_offset_;

  // Architecture-dependent register spill mask
  uint32_t core_spill_mask_;

  // Compiled code associated with this method for callers from managed code.
  // May be compiled managed code or a bridge for invoking a native method.
  // TODO: Break apart this into portable and quick.
  const void* entry_point_from_compiled_code_;

  // Called by the interpreter to execute this method.
  EntryPointFromInterpreter* entry_point_from_interpreter_;

  // Architecture-dependent register spill mask
  uint32_t fp_spill_mask_;

  // Total size in bytes of the frame
  size_t frame_size_in_bytes_;

  // Garbage collection map of native PC offsets (quick) or dex PCs (portable) to reference bitmaps.
  const uint8_t* gc_map_;

  // Mapping from native pc to dex pc
  const uint32_t* mapping_table_;

  // Index into method_ids of the dex file associated with this method
  uint32_t method_dex_index_;

  // For concrete virtual methods, this is the offset of the method in Class::vtable_.
  //
  // For abstract methods in an interface class, this is the offset of the method in
  // "iftable_->Get(n)->GetMethodArray()".
  //
  // For static and direct methods this is the index in the direct methods table.
  uint32_t method_index_;

  // The target native method registered with this method
  const void* native_method_;

  // When a register is promoted into a register, the spill mask holds which registers hold dex
  // registers. The first promoted register's corresponding dex register is vmap_table_[1], the Nth
  // is vmap_table_[N]. vmap_table_[0] holds the length of the table.
  const uint16_t* vmap_table_;

  static Class* java_lang_reflect_ArtMethod_;

 private:
  friend struct art::ArtMethodOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArtMethod);
};

class MANAGED ArtMethodClass : public Class {
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArtMethodClass);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ART_METHOD_H_
