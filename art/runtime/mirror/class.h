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

#ifndef ART_RUNTIME_MIRROR_CLASS_H_
#define ART_RUNTIME_MIRROR_CLASS_H_

#include "modifiers.h"
#include "object.h"
#include "primitive.h"

/*
 * A magic value for refOffsets. Ignore the bits and walk the super
 * chain when this is the value.
 * [This is an unlikely "natural" value, since it would be 30 non-ref instance
 * fields followed by 2 ref instance fields.]
 */
#define CLASS_WALK_SUPER 3U
#define CLASS_BITS_PER_WORD (sizeof(uint32_t) * 8)
#define CLASS_OFFSET_ALIGNMENT 4
#define CLASS_HIGH_BIT (1U << (CLASS_BITS_PER_WORD - 1))
/*
 * Given an offset, return the bit number which would encode that offset.
 * Local use only.
 */
#define _CLASS_BIT_NUMBER_FROM_OFFSET(byteOffset) \
    ((unsigned int)(byteOffset) / \
     CLASS_OFFSET_ALIGNMENT)
/*
 * Is the given offset too large to be encoded?
 */
#define CLASS_CAN_ENCODE_OFFSET(byteOffset) \
    (_CLASS_BIT_NUMBER_FROM_OFFSET(byteOffset) < CLASS_BITS_PER_WORD)
/*
 * Return a single bit, encoding the offset.
 * Undefined if the offset is too large, as defined above.
 */
#define CLASS_BIT_FROM_OFFSET(byteOffset) \
    (CLASS_HIGH_BIT >> _CLASS_BIT_NUMBER_FROM_OFFSET(byteOffset))
/*
 * Return an offset, given a bit number as returned from CLZ.
 */
#define CLASS_OFFSET_FROM_CLZ(rshift) \
    MemberOffset((static_cast<int>(rshift) * CLASS_OFFSET_ALIGNMENT))

namespace art {

struct ClassClassOffsets;
struct ClassOffsets;
class StringPiece;

namespace mirror {

class ArtField;
class ClassLoader;
class DexCache;
class IfTable;

// Type for the InitializedStaticStorage table. Currently the Class
// provides the static storage. However, this might change to an Array
// to improve image sharing, so we use this type to avoid assumptions
// on the current storage.
class MANAGED StaticStorageBase : public Object {
};

// C++ mirror of java.lang.Class
class MANAGED Class : public StaticStorageBase {
 public:
  // Class Status
  //
  // kStatusNotReady: If a Class cannot be found in the class table by
  // FindClass, it allocates an new one with AllocClass in the
  // kStatusNotReady and calls LoadClass. Note if it does find a
  // class, it may not be kStatusResolved and it will try to push it
  // forward toward kStatusResolved.
  //
  // kStatusIdx: LoadClass populates with Class with information from
  // the DexFile, moving the status to kStatusIdx, indicating that the
  // Class value in super_class_ has not been populated. The new Class
  // can then be inserted into the classes table.
  //
  // kStatusLoaded: After taking a lock on Class, the ClassLinker will
  // attempt to move a kStatusIdx class forward to kStatusLoaded by
  // using ResolveClass to initialize the super_class_ and ensuring the
  // interfaces are resolved.
  //
  // kStatusResolved: Still holding the lock on Class, the ClassLinker
  // shows linking is complete and fields of the Class populated by making
  // it kStatusResolved. Java allows circularities of the form where a super
  // class has a field that is of the type of the sub class. We need to be able
  // to fully resolve super classes while resolving types for fields.
  //
  // kStatusRetryVerificationAtRuntime: The verifier sets a class to
  // this state if it encounters a soft failure at compile time. This
  // often happens when there are unresolved classes in other dex
  // files, and this status marks a class as needing to be verified
  // again at runtime.
  //
  // TODO: Explain the other states
  enum Status {
    kStatusError = -1,
    kStatusNotReady = 0,
    kStatusIdx = 1,  // Loaded, DEX idx in super_class_type_idx_ and interfaces_type_idx_.
    kStatusLoaded = 2,  // DEX idx values resolved.
    kStatusResolved = 3,  // Part of linking.
    kStatusVerifying = 4,  // In the process of being verified.
    kStatusRetryVerificationAtRuntime = 5,  // Compile time verification failed, retry at runtime.
    kStatusVerifyingAtRuntime = 6,  // Retrying verification at runtime.
    kStatusVerified = 7,  // Logically part of linking; done pre-init.
    kStatusInitializing = 8,  // Class init in progress.
    kStatusInitialized = 9,  // Ready to go.
  };

  Status GetStatus() const {
    DCHECK_EQ(sizeof(Status), sizeof(uint32_t));
    return static_cast<Status>(GetField32(OFFSET_OF_OBJECT_MEMBER(Class, status_), true));
  }

  void SetStatus(Status new_status, Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns true if the class has failed to link.
  bool IsErroneous() const {
    return GetStatus() == kStatusError;
  }

  // Returns true if the class has been loaded.
  bool IsIdxLoaded() const {
    return GetStatus() >= kStatusIdx;
  }

  // Returns true if the class has been loaded.
  bool IsLoaded() const {
    return GetStatus() >= kStatusLoaded;
  }

  // Returns true if the class has been linked.
  bool IsResolved() const {
    return GetStatus() >= kStatusResolved;
  }

  // Returns true if the class was compile-time verified.
  bool IsCompileTimeVerified() const {
    return GetStatus() >= kStatusRetryVerificationAtRuntime;
  }

  // Returns true if the class has been verified.
  bool IsVerified() const {
    return GetStatus() >= kStatusVerified;
  }

  // Returns true if the class is initializing.
  bool IsInitializing() const {
    return GetStatus() >= kStatusInitializing;
  }

  // Returns true if the class is initialized.
  bool IsInitialized() const {
    return GetStatus() == kStatusInitialized;
  }

  uint32_t GetAccessFlags() const;

  void SetAccessFlags(uint32_t new_access_flags) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_), new_access_flags, false);
  }

  // Returns true if the class is an interface.
  bool IsInterface() const {
    return (GetAccessFlags() & kAccInterface) != 0;
  }

  // Returns true if the class is declared public.
  bool IsPublic() const {
    return (GetAccessFlags() & kAccPublic) != 0;
  }

  // Returns true if the class is declared final.
  bool IsFinal() const {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  bool IsFinalizable() const {
    return (GetAccessFlags() & kAccClassIsFinalizable) != 0;
  }

  void SetFinalizable() {
    uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_), false);
    SetAccessFlags(flags | kAccClassIsFinalizable);
  }

  // Returns true if the class is abstract.
  bool IsAbstract() const {
    return (GetAccessFlags() & kAccAbstract) != 0;
  }

  // Returns true if the class is an annotation.
  bool IsAnnotation() const {
    return (GetAccessFlags() & kAccAnnotation) != 0;
  }

  // Returns true if the class is synthetic.
  bool IsSynthetic() const {
    return (GetAccessFlags() & kAccSynthetic) != 0;
  }

  bool IsReferenceClass() const {
    return (GetAccessFlags() & kAccClassIsReference) != 0;
  }

  bool IsWeakReferenceClass() const {
    return (GetAccessFlags() & kAccClassIsWeakReference) != 0;
  }

  bool IsSoftReferenceClass() const {
    return (GetAccessFlags() & kAccReferenceFlagsMask) == kAccClassIsReference;
  }

  bool IsFinalizerReferenceClass() const {
    return (GetAccessFlags() & kAccClassIsFinalizerReference) != 0;
  }

  bool IsPhantomReferenceClass() const {
    return (GetAccessFlags() & kAccClassIsPhantomReference) != 0;
  }

  // Can references of this type be assigned to by things of another type? For non-array types
  // this is a matter of whether sub-classes may exist - which they can't if the type is final.
  // For array classes, where all the classes are final due to there being no sub-classes, an
  // Object[] may be assigned to by a String[] but a String[] may not be assigned to by other
  // types as the component is final.
  bool CannotBeAssignedFromOtherTypes() const {
    if (!IsArrayClass()) {
      return IsFinal();
    } else {
      Class* component = GetComponentType();
      if (component->IsPrimitive()) {
        return false;
      } else {
        return component->CannotBeAssignedFromOtherTypes();
      }
    }
  }

  String* GetName() const;  // Returns the cached name.
  void SetName(String* name) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);  // Sets the cached name.
  // Computes the name, then sets the cached value.
  String* ComputeName() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsProxyClass() const {
    // Read access flags without using getter as whether something is a proxy can be check in
    // any loaded state
    // TODO: switch to a check if the super class is java.lang.reflect.Proxy?
    uint32_t access_flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_), false);
    return (access_flags & kAccClassIsProxy) != 0;
  }

  Primitive::Type GetPrimitiveType() const {
    DCHECK_EQ(sizeof(Primitive::Type), sizeof(int32_t));
    return static_cast<Primitive::Type>(
        GetField32(OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_), false));
  }

  void SetPrimitiveType(Primitive::Type new_type) {
    DCHECK_EQ(sizeof(Primitive::Type), sizeof(int32_t));
    SetField32(OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_), new_type, false);
  }

  // Returns true if the class is a primitive type.
  bool IsPrimitive() const {
    return GetPrimitiveType() != Primitive::kPrimNot;
  }

  bool IsPrimitiveBoolean() const {
    return GetPrimitiveType() == Primitive::kPrimBoolean;
  }

  bool IsPrimitiveByte() const {
    return GetPrimitiveType() == Primitive::kPrimByte;
  }

  bool IsPrimitiveChar() const {
    return GetPrimitiveType() == Primitive::kPrimChar;
  }

  bool IsPrimitiveShort() const {
    return GetPrimitiveType() == Primitive::kPrimShort;
  }

  bool IsPrimitiveInt() const {
    return GetPrimitiveType() == Primitive::kPrimInt;
  }

  bool IsPrimitiveLong() const {
    return GetPrimitiveType() == Primitive::kPrimLong;
  }

  bool IsPrimitiveFloat() const {
    return GetPrimitiveType() == Primitive::kPrimFloat;
  }

  bool IsPrimitiveDouble() const {
    return GetPrimitiveType() == Primitive::kPrimDouble;
  }

  bool IsPrimitiveVoid() const {
    return GetPrimitiveType() == Primitive::kPrimVoid;
  }

  bool IsPrimitiveArray() const {
    return IsArrayClass() && GetComponentType()->IsPrimitive();
  }

  // Depth of class from java.lang.Object
  size_t Depth() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    size_t depth = 0;
    for (Class* klass = this; klass->GetSuperClass() != NULL; klass = klass->GetSuperClass()) {
      depth++;
    }
    return depth;
  }

  bool IsArrayClass() const {
    return GetComponentType() != NULL;
  }

  bool IsClassClass() const;

  bool IsStringClass() const;

  bool IsThrowableClass() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsArtFieldClass() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsArtMethodClass() const;

  Class* GetComponentType() const {
    return GetFieldObject<Class*>(OFFSET_OF_OBJECT_MEMBER(Class, component_type_), false);
  }

  void SetComponentType(Class* new_component_type) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(GetComponentType() == NULL);
    DCHECK(new_component_type != NULL);
    SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, component_type_), new_component_type, false);
  }

  size_t GetComponentSize() const {
    return Primitive::ComponentSize(GetComponentType()->GetPrimitiveType());
  }

  bool IsObjectClass() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return !IsPrimitive() && GetSuperClass() == NULL;
  }
  bool IsInstantiable() const {
    return (!IsPrimitive() && !IsInterface() && !IsAbstract()) || ((IsAbstract()) && IsArrayClass());
  }

  bool IsObjectArrayClass() const {
    return GetComponentType() != NULL && !GetComponentType()->IsPrimitive();
  }

  // Creates a raw object instance but does not invoke the default constructor.
  Object* AllocObject(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsVariableSize() const {
    // Classes and arrays vary in size, and so the object_size_ field cannot
    // be used to get their instance size
    return IsClassClass() || IsArrayClass();
  }

  size_t SizeOf() const {
    DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, class_size_), false);
  }

  size_t GetClassSize() const {
    DCHECK_EQ(sizeof(size_t), sizeof(uint32_t));
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, class_size_), false);
  }

  void SetClassSize(size_t new_class_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t GetObjectSize() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetObjectSize(size_t new_object_size) {
    DCHECK(!IsVariableSize());
    DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
    return SetField32(OFFSET_OF_OBJECT_MEMBER(Class, object_size_), new_object_size, false);
  }

  // Returns true if this class is in the same packages as that class.
  bool IsInSamePackage(const Class* that) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool IsInSamePackage(const StringPiece& descriptor1, const StringPiece& descriptor2);

  // Returns true if this class can access that class.
  bool CanAccess(Class* that) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return that->IsPublic() || this->IsInSamePackage(that);
  }

  // Can this class access a member in the provided class with the provided member access flags?
  // Note that access to the class isn't checked in case the declaring class is protected and the
  // method has been exposed by a public sub-class
  bool CanAccessMember(Class* access_to, uint32_t member_flags) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Classes can access all of their own members
    if (this == access_to) {
      return true;
    }
    // Public members are trivially accessible
    if (member_flags & kAccPublic) {
      return true;
    }
    // Private members are trivially not accessible
    if (member_flags & kAccPrivate) {
      return false;
    }
    // Check for protected access from a sub-class, which may or may not be in the same package.
    if (member_flags & kAccProtected) {
      if (this->IsSubClass(access_to)) {
        return true;
      }
    }
    // Allow protected access from other classes in the same package.
    return this->IsInSamePackage(access_to);
  }

  bool IsSubClass(const Class* klass) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can src be assigned to this class? For example, String can be assigned to Object (by an
  // upcast), however, an Object cannot be assigned to a String as a potentially exception throwing
  // downcast would be necessary. Similarly for interfaces, a class that implements (or an interface
  // that extends) another can be assigned to its parent, but not vice-versa. All Classes may assign
  // to themselves. Classes for primitive types may not assign to each other.
  inline bool IsAssignableFrom(const Class* src) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(src != NULL);
    if (this == src) {
      // Can always assign to things of the same type.
      return true;
    } else if (IsObjectClass()) {
      // Can assign any reference to java.lang.Object.
      return !src->IsPrimitive();
    } else if (IsInterface()) {
      return src->Implements(this);
    } else if (src->IsArrayClass()) {
      return IsAssignableFromArray(src);
    } else {
      return !src->IsInterface() && src->IsSubClass(this);
    }
  }

  Class* GetSuperClass() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetSuperClass(Class *new_super_class) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // super class is assigned once, except during class linker initialization
    Class* old_super_class = GetFieldObject<Class*>(
        OFFSET_OF_OBJECT_MEMBER(Class, super_class_), false);
    DCHECK(old_super_class == NULL || old_super_class == new_super_class);
    DCHECK(new_super_class != NULL);
    SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, super_class_), new_super_class, false);
  }

  bool HasSuperClass() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetSuperClass() != NULL;
  }

  static MemberOffset SuperClassOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Class, super_class_));
  }

  ClassLoader* GetClassLoader() const;

  void SetClassLoader(ClassLoader* new_cl) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset DexCacheOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Class, dex_cache_));
  }

  enum {
    kDumpClassFullDetail = 1,
    kDumpClassClassLoader = (1 << 1),
    kDumpClassInitialized = (1 << 2),
  };

  void DumpClass(std::ostream& os, int flags) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  DexCache* GetDexCache() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetDexCache(DexCache* new_dex_cache) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ObjectArray<ArtMethod>* GetDirectMethods() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetDirectMethods(ObjectArray<ArtMethod>* new_direct_methods)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* GetDirectMethod(int32_t i) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetDirectMethod(uint32_t i, ArtMethod* f)  // TODO: uint16_t
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the number of static, private, and constructor methods.
  size_t NumDirectMethods() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ObjectArray<ArtMethod>* GetVirtualMethods() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetVirtualMethods(ObjectArray<ArtMethod>* new_virtual_methods)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the number of non-inherited virtual methods.
  size_t NumVirtualMethods() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* GetVirtualMethod(uint32_t i) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* GetVirtualMethodDuringLinking(uint32_t i) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetVirtualMethod(uint32_t i, ArtMethod* f)  // TODO: uint16_t
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ObjectArray<ArtMethod>* GetVTable() const;

  ObjectArray<ArtMethod>* GetVTableDuringLinking() const;

  void SetVTable(ObjectArray<ArtMethod>* new_vtable)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset VTableOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, vtable_);
  }

  // Given a method implemented by this class but potentially from a super class, return the
  // specific implementation method for this class.
  ArtMethod* FindVirtualMethodForVirtual(ArtMethod* method) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Given a method implemented by this class' super class, return the specific implementation
  // method for this class.
  ArtMethod* FindVirtualMethodForSuper(ArtMethod* method) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Given a method implemented by this class, but potentially from a
  // super class or interface, return the specific implementation
  // method for this class.
  ArtMethod* FindVirtualMethodForInterface(ArtMethod* method) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) ALWAYS_INLINE;

  ArtMethod* FindInterfaceMethod(const StringPiece& name, const StringPiece& descriptor) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindInterfaceMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindVirtualMethodForVirtualOrInterface(ArtMethod* method) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredVirtualMethod(const StringPiece& name, const StringPiece& signature) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredVirtualMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindVirtualMethod(const StringPiece& name, const StringPiece& descriptor) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindVirtualMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredDirectMethod(const StringPiece& name, const StringPiece& signature) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredDirectMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDirectMethod(const StringPiece& name, const StringPiece& signature) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDirectMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t GetIfTableCount() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  IfTable* GetIfTable() const;

  void SetIfTable(IfTable* new_iftable) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get instance fields of the class (See also GetSFields).
  ObjectArray<ArtField>* GetIFields() const;

  void SetIFields(ObjectArray<ArtField>* new_ifields) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t NumInstanceFields() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* GetInstanceField(uint32_t i) const  // TODO: uint16_t
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetInstanceField(uint32_t i, ArtField* f)  // TODO: uint16_t
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the number of instance fields containing reference types.
  size_t NumReferenceInstanceFields() const {
    DCHECK(IsResolved() || IsErroneous());
    DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_), false);
  }

  size_t NumReferenceInstanceFieldsDuringLinking() const {
    DCHECK(IsLoaded() || IsErroneous());
    DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_), false);
  }

  void SetNumReferenceInstanceFields(size_t new_num) {
    DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
    SetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_), new_num, false);
  }

  uint32_t GetReferenceInstanceOffsets() const {
    DCHECK(IsResolved() || IsErroneous());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, reference_instance_offsets_), false);
  }

  void SetReferenceInstanceOffsets(uint32_t new_reference_offsets)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Beginning of static field data
  static MemberOffset FieldsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, fields_);
  }

  // Returns the number of static fields containing reference types.
  size_t NumReferenceStaticFields() const {
    DCHECK(IsResolved() || IsErroneous());
    DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_), false);
  }

  size_t NumReferenceStaticFieldsDuringLinking() const {
    DCHECK(IsLoaded() || IsErroneous());
    DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_), false);
  }

  void SetNumReferenceStaticFields(size_t new_num) {
    DCHECK_EQ(sizeof(size_t), sizeof(int32_t));
    SetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_), new_num, false);
  }

  // Gets the static fields of the class.
  ObjectArray<ArtField>* GetSFields() const;

  void SetSFields(ObjectArray<ArtField>* new_sfields) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t NumStaticFields() const;

  ArtField* GetStaticField(uint32_t i) const;  // TODO: uint16_t

  void SetStaticField(uint32_t i, ArtField* f);  // TODO: uint16_t

  uint32_t GetReferenceStaticOffsets() const {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, reference_static_offsets_), false);
  }

  void SetReferenceStaticOffsets(uint32_t new_reference_offsets);

  // Find a static or instance field using the JLS resolution order
  ArtField* FindField(const StringPiece& name, const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the given instance field in this class or a superclass.
  ArtField* FindInstanceField(const StringPiece& name, const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the given instance field in this class or a superclass, only searches classes that
  // have the same dex cache.
  ArtField* FindInstanceField(const DexCache* dex_cache, uint32_t dex_field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* FindDeclaredInstanceField(const StringPiece& name, const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* FindDeclaredInstanceField(const DexCache* dex_cache, uint32_t dex_field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the given static field in this class or a superclass.
  ArtField* FindStaticField(const StringPiece& name, const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the given static field in this class or superclass, only searches classes that
  // have the same dex cache.
  ArtField* FindStaticField(const DexCache* dex_cache, uint32_t dex_field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* FindDeclaredStaticField(const StringPiece& name, const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* FindDeclaredStaticField(const DexCache* dex_cache, uint32_t dex_field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  pid_t GetClinitThreadId() const {
    DCHECK(IsIdxLoaded() || IsErroneous());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, clinit_thread_id_), false);
  }

  void SetClinitThreadId(pid_t new_clinit_thread_id) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(Class, clinit_thread_id_), new_clinit_thread_id, false);
  }

  Class* GetVerifyErrorClass() const {
    // DCHECK(IsErroneous());
    return GetFieldObject<Class*>(OFFSET_OF_OBJECT_MEMBER(Class, verify_error_class_), false);
  }

  uint16_t GetDexClassDefIndex() const {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, dex_class_def_idx_), false);
  }

  void SetDexClassDefIndex(uint16_t class_def_idx) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(Class, dex_class_def_idx_), class_def_idx, false);
  }

  uint16_t GetDexTypeIndex() const {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, dex_type_idx_), false);
  }

  void SetDexTypeIndex(uint16_t type_idx) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(Class, dex_type_idx_), type_idx, false);
  }

  static Class* GetJavaLangClass() {
    DCHECK(java_lang_Class_ != NULL);
    return java_lang_Class_;
  }

  // Can't call this SetClass or else gets called instead of Object::SetClass in places.
  static void SetClassClass(Class* java_lang_Class);
  static void ResetClass();

  // When class is verified, set the kAccPreverified flag on each method.
  void SetPreverifiedFlagOnAllMethods() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void SetVerifyErrorClass(Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool Implements(const Class* klass) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsArrayAssignableFromArray(const Class* klass) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsAssignableFromArray(const Class* klass) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // defining class loader, or NULL for the "bootstrap" system loader
  ClassLoader* class_loader_;

  // For array classes, the component class object for instanceof/checkcast
  // (for String[][][], this will be String[][]). NULL for non-array classes.
  Class* component_type_;

  // DexCache of resolved constant pool entries (will be NULL for classes generated by the
  // runtime such as arrays and primitive classes).
  DexCache* dex_cache_;

  // static, private, and <init> methods
  ObjectArray<ArtMethod>* direct_methods_;

  // instance fields
  //
  // These describe the layout of the contents of an Object.
  // Note that only the fields directly declared by this class are
  // listed in ifields; fields declared by a superclass are listed in
  // the superclass's Class.ifields.
  //
  // All instance fields that refer to objects are guaranteed to be at
  // the beginning of the field list.  num_reference_instance_fields_
  // specifies the number of reference fields.
  ObjectArray<ArtField>* ifields_;

  // The interface table (iftable_) contains pairs of a interface class and an array of the
  // interface methods. There is one pair per interface supported by this class.  That means one
  // pair for each interface we support directly, indirectly via superclass, or indirectly via a
  // superinterface.  This will be null if neither we nor our superclass implement any interfaces.
  //
  // Why we need this: given "class Foo implements Face", declare "Face faceObj = new Foo()".
  // Invoke faceObj.blah(), where "blah" is part of the Face interface.  We can't easily use a
  // single vtable.
  //
  // For every interface a concrete class implements, we create an array of the concrete vtable_
  // methods for the methods in the interface.
  IfTable* iftable_;

  // descriptor for the class such as "java.lang.Class" or "[C". Lazily initialized by ComputeName
  String* name_;

  // Static fields
  ObjectArray<ArtField>* sfields_;

  // The superclass, or NULL if this is java.lang.Object, an interface or primitive type.
  Class* super_class_;

  // If class verify fails, we must return same error on subsequent tries.
  Class* verify_error_class_;

  // Virtual methods defined in this class; invoked through vtable.
  ObjectArray<ArtMethod>* virtual_methods_;

  // Virtual method table (vtable), for use by "invoke-virtual".  The vtable from the superclass is
  // copied in, and virtual methods from our class either replace those from the super or are
  // appended. For abstract classes, methods may be created in the vtable that aren't in
  // virtual_ methods_ for miranda methods.
  ObjectArray<ArtMethod>* vtable_;

  // Access flags; low 16 bits are defined by VM spec.
  uint32_t access_flags_;

  // Total size of the Class instance; used when allocating storage on gc heap.
  // See also object_size_.
  size_t class_size_;

  // Tid used to check for recursive <clinit> invocation.
  pid_t clinit_thread_id_;

  // ClassDef index in dex file, -1 if no class definition such as an array.
  // TODO: really 16bits
  int32_t dex_class_def_idx_;

  // Type index in dex file.
  // TODO: really 16bits
  int32_t dex_type_idx_;

  // Number of instance fields that are object refs.
  size_t num_reference_instance_fields_;

  // Number of static fields that are object refs,
  size_t num_reference_static_fields_;

  // Total object size; used when allocating storage on gc heap.
  // (For interfaces and abstract classes this will be zero.)
  // See also class_size_.
  size_t object_size_;

  // Primitive type value, or Primitive::kPrimNot (0); set for generated primitive classes.
  Primitive::Type primitive_type_;

  // Bitmap of offsets of ifields.
  uint32_t reference_instance_offsets_;

  // Bitmap of offsets of sfields.
  uint32_t reference_static_offsets_;

  // State of class initialization.
  Status status_;

  // TODO: ?
  // initiating class loader list
  // NOTE: for classes with low serialNumber, these are unused, and the
  // values are kept in a table in gDvm.
  // InitiatingLoaderList initiating_loader_list_;

  // Location of first static field.
  uint32_t fields_[0];

  // java.lang.Class
  static Class* java_lang_Class_;

  friend struct art::ClassOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Class);
};

std::ostream& operator<<(std::ostream& os, const Class::Status& rhs);

class MANAGED ClassClass : public Class {
 private:
  int64_t serialVersionUID_;
  friend struct art::ClassClassOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(ClassClass);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_H_
