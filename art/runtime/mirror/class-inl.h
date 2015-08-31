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

#ifndef ART_RUNTIME_MIRROR_CLASS_INL_H_
#define ART_RUNTIME_MIRROR_CLASS_INL_H_

#include "class.h"

#include "art_field.h"
#include "art_method.h"
#include "class_loader.h"
#include "dex_cache.h"
#include "iftable.h"
#include "object_array-inl.h"
#include "runtime.h"
#include "string.h"

namespace art {
namespace mirror {

inline size_t Class::GetObjectSize() const {
  DCHECK(!IsVariableSize()) << " class=" << PrettyTypeOf(this);
  DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
  size_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, object_size_), false);
  DCHECK_GE(result, sizeof(Object)) << " class=" << PrettyTypeOf(this);
  return result;
}

inline Class* Class::GetSuperClass() const {
  // Can only get super class for loaded classes (hack for when runtime is
  // initializing)
  DCHECK(IsLoaded() || !Runtime::Current()->IsStarted()) << IsLoaded();
  return GetFieldObject<Class*>(OFFSET_OF_OBJECT_MEMBER(Class, super_class_), false);
}

inline ClassLoader* Class::GetClassLoader() const {
  return GetFieldObject<ClassLoader*>(OFFSET_OF_OBJECT_MEMBER(Class, class_loader_), false);
}

inline DexCache* Class::GetDexCache() const {
  return GetFieldObject<DexCache*>(OFFSET_OF_OBJECT_MEMBER(Class, dex_cache_), false);
}

inline ObjectArray<ArtMethod>* Class::GetDirectMethods() const {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldObject<ObjectArray<ArtMethod>*>(
      OFFSET_OF_OBJECT_MEMBER(Class, direct_methods_), false);
}

inline void Class::SetDirectMethods(ObjectArray<ArtMethod>* new_direct_methods)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(NULL == GetFieldObject<ObjectArray<ArtMethod>*>(
      OFFSET_OF_OBJECT_MEMBER(Class, direct_methods_), false));
  DCHECK_NE(0, new_direct_methods->GetLength());
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, direct_methods_),
                 new_direct_methods, false);
}

inline ArtMethod* Class::GetDirectMethod(int32_t i) const
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return GetDirectMethods()->Get(i);
}

inline void Class::SetDirectMethod(uint32_t i, ArtMethod* f)  // TODO: uint16_t
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectArray<ArtMethod>* direct_methods =
      GetFieldObject<ObjectArray<ArtMethod>*>(
          OFFSET_OF_OBJECT_MEMBER(Class, direct_methods_), false);
  direct_methods->Set(i, f);
}

// Returns the number of static, private, and constructor methods.
inline size_t Class::NumDirectMethods() const {
  return (GetDirectMethods() != NULL) ? GetDirectMethods()->GetLength() : 0;
}

inline ObjectArray<ArtMethod>* Class::GetVirtualMethods() const {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldObject<ObjectArray<ArtMethod>*>(
      OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_), false);
}

inline void Class::SetVirtualMethods(ObjectArray<ArtMethod>* new_virtual_methods) {
  // TODO: we reassign virtual methods to grow the table for miranda
  // methods.. they should really just be assigned once
  DCHECK_NE(0, new_virtual_methods->GetLength());
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_),
                 new_virtual_methods, false);
}

inline size_t Class::NumVirtualMethods() const {
  return (GetVirtualMethods() != NULL) ? GetVirtualMethods()->GetLength() : 0;
}

inline ArtMethod* Class::GetVirtualMethod(uint32_t i) const
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(IsResolved() || IsErroneous());
  return GetVirtualMethods()->Get(i);
}

inline ArtMethod* Class::GetVirtualMethodDuringLinking(uint32_t i) const
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(IsLoaded() || IsErroneous());
  return GetVirtualMethods()->Get(i);
}

inline void Class::SetVirtualMethod(uint32_t i, ArtMethod* f)  // TODO: uint16_t
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectArray<ArtMethod>* virtual_methods =
      GetFieldObject<ObjectArray<ArtMethod>*>(
          OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_), false);
  virtual_methods->Set(i, f);
}

inline ObjectArray<ArtMethod>* Class::GetVTable() const {
  DCHECK(IsResolved() || IsErroneous());
  return GetFieldObject<ObjectArray<ArtMethod>*>(OFFSET_OF_OBJECT_MEMBER(Class, vtable_), false);
}

inline ObjectArray<ArtMethod>* Class::GetVTableDuringLinking() const {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldObject<ObjectArray<ArtMethod>*>(OFFSET_OF_OBJECT_MEMBER(Class, vtable_), false);
}

inline void Class::SetVTable(ObjectArray<ArtMethod>* new_vtable)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, vtable_), new_vtable, false);
}

inline bool Class::Implements(const Class* klass) const {
  DCHECK(klass != NULL);
  DCHECK(klass->IsInterface()) << PrettyClass(this);
  // All interfaces implemented directly and by our superclass, and
  // recursively all super-interfaces of those interfaces, are listed
  // in iftable_, so we can just do a linear scan through that.
  int32_t iftable_count = GetIfTableCount();
  IfTable* iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; i++) {
    if (iftable->GetInterface(i) == klass) {
      return true;
    }
  }
  return false;
}

// Determine whether "this" is assignable from "src", where both of these
// are array classes.
//
// Consider an array class, e.g. Y[][], where Y is a subclass of X.
//   Y[][]            = Y[][] --> true (identity)
//   X[][]            = Y[][] --> true (element superclass)
//   Y                = Y[][] --> false
//   Y[]              = Y[][] --> false
//   Object           = Y[][] --> true (everything is an object)
//   Object[]         = Y[][] --> true
//   Object[][]       = Y[][] --> true
//   Object[][][]     = Y[][] --> false (too many []s)
//   Serializable     = Y[][] --> true (all arrays are Serializable)
//   Serializable[]   = Y[][] --> true
//   Serializable[][] = Y[][] --> false (unless Y is Serializable)
//
// Don't forget about primitive types.
//   Object[]         = int[] --> false
//
inline bool Class::IsArrayAssignableFromArray(const Class* src) const {
  DCHECK(IsArrayClass())  << PrettyClass(this);
  DCHECK(src->IsArrayClass()) << PrettyClass(src);
  return GetComponentType()->IsAssignableFrom(src->GetComponentType());
}

inline bool Class::IsAssignableFromArray(const Class* src) const {
  DCHECK(!IsInterface()) << PrettyClass(this);  // handled first in IsAssignableFrom
  DCHECK(src->IsArrayClass()) << PrettyClass(src);
  if (!IsArrayClass()) {
    // If "this" is not also an array, it must be Object.
    // src's super should be java_lang_Object, since it is an array.
    Class* java_lang_Object = src->GetSuperClass();
    DCHECK(java_lang_Object != NULL) << PrettyClass(src);
    DCHECK(java_lang_Object->GetSuperClass() == NULL) << PrettyClass(src);
    return this == java_lang_Object;
  }
  return IsArrayAssignableFromArray(src);
}

inline bool Class::IsSubClass(const Class* klass) const {
  DCHECK(!IsInterface()) << PrettyClass(this);
  DCHECK(!IsArrayClass()) << PrettyClass(this);
  const Class* current = this;
  do {
    if (current == klass) {
      return true;
    }
    current = current->GetSuperClass();
  } while (current != NULL);
  return false;
}

inline ArtMethod* Class::FindVirtualMethodForInterface(ArtMethod* method) const {
  Class* declaring_class = method->GetDeclaringClass();
  DCHECK(declaring_class != NULL) << PrettyClass(this);
  DCHECK(declaring_class->IsInterface()) << PrettyMethod(method);
  // TODO cache to improve lookup speed
  int32_t iftable_count = GetIfTableCount();
  IfTable* iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; i++) {
    if (iftable->GetInterface(i) == declaring_class) {
      return iftable->GetMethodArray(i)->Get(method->GetMethodIndex());
    }
  }
  return NULL;
}

inline ArtMethod* Class::FindVirtualMethodForVirtual(ArtMethod* method) const
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(!method->GetDeclaringClass()->IsInterface() || method->IsMiranda());
  // The argument method may from a super class.
  // Use the index to a potentially overridden one for this instance's class.
  return GetVTable()->Get(method->GetMethodIndex());
}

inline ArtMethod* Class::FindVirtualMethodForSuper(ArtMethod* method) const
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(!method->GetDeclaringClass()->IsInterface());
  return GetSuperClass()->GetVTable()->Get(method->GetMethodIndex());
}

inline ArtMethod* Class::FindVirtualMethodForVirtualOrInterface(ArtMethod* method) const {
  if (method->IsDirect()) {
    return method;
  }
  if (method->GetDeclaringClass()->IsInterface() && !method->IsMiranda()) {
    return FindVirtualMethodForInterface(method);
  }
  return FindVirtualMethodForVirtual(method);
}

inline IfTable* Class::GetIfTable() const {
  return GetFieldObject<IfTable*>(OFFSET_OF_OBJECT_MEMBER(Class, iftable_), false);
}

inline int32_t Class::GetIfTableCount() const {
  IfTable* iftable = GetIfTable();
  if (iftable == NULL) {
    return 0;
  }
  return iftable->Count();
}

inline void Class::SetIfTable(IfTable* new_iftable) {
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, iftable_), new_iftable, false);
}

inline ObjectArray<ArtField>* Class::GetIFields() const {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldObject<ObjectArray<ArtField>*>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_), false);
}

inline void Class::SetIFields(ObjectArray<ArtField>* new_ifields)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(NULL == GetFieldObject<ObjectArray<ArtField>*>(
      OFFSET_OF_OBJECT_MEMBER(Class, ifields_), false));
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, ifields_), new_ifields, false);
}

inline ObjectArray<ArtField>* Class::GetSFields() const {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldObject<ObjectArray<ArtField>*>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_), false);
}

inline void Class::SetSFields(ObjectArray<ArtField>* new_sfields)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(NULL == GetFieldObject<ObjectArray<ArtField>*>(
      OFFSET_OF_OBJECT_MEMBER(Class, sfields_), false));
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, sfields_), new_sfields, false);
}

inline size_t Class::NumStaticFields() const {
  return (GetSFields() != NULL) ? GetSFields()->GetLength() : 0;
}

inline ArtField* Class::GetStaticField(uint32_t i) const  // TODO: uint16_t
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return GetSFields()->Get(i);
}

inline void Class::SetStaticField(uint32_t i, ArtField* f)  // TODO: uint16_t
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectArray<ArtField>* sfields= GetFieldObject<ObjectArray<ArtField>*>(
      OFFSET_OF_OBJECT_MEMBER(Class, sfields_), false);
  sfields->Set(i, f);
}

inline size_t Class::NumInstanceFields() const {
  return (GetIFields() != NULL) ? GetIFields()->GetLength() : 0;
}

inline ArtField* Class::GetInstanceField(uint32_t i) const  // TODO: uint16_t
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK_NE(NumInstanceFields(), 0U);
  return GetIFields()->Get(i);
}

inline void Class::SetInstanceField(uint32_t i, ArtField* f)  // TODO: uint16_t
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ObjectArray<ArtField>* ifields= GetFieldObject<ObjectArray<ArtField>*>(
      OFFSET_OF_OBJECT_MEMBER(Class, ifields_), false);
  ifields->Set(i, f);
}

inline void Class::SetVerifyErrorClass(Class* klass) {
  CHECK(klass != NULL) << PrettyClass(this);
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, verify_error_class_), klass, false);
}

inline uint32_t Class::GetAccessFlags() const {
  // Check class is loaded or this is java.lang.String that has a
  // circularity issue during loading the names of its members
  DCHECK(IsLoaded() || IsErroneous() ||
         this == String::GetJavaLangString() ||
         this == ArtField::GetJavaLangReflectArtField() ||
         this == ArtMethod::GetJavaLangReflectArtMethod());
  return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_), false);
}

inline String* Class::GetName() const {
  return GetFieldObject<String*>(OFFSET_OF_OBJECT_MEMBER(Class, name_), false);
}
inline void Class::SetName(String* name) {
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, name_), name, false);
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_INL_H_
