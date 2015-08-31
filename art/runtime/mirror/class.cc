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

#include "class.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "class-inl.h"
#include "class_linker.h"
#include "class_loader.h"
#include "dex_cache.h"
#include "dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "object-inl.h"
#include "object_array-inl.h"
#include "object_utils.h"
#include "runtime.h"
#include "sirt_ref.h"
#include "thread.h"
#include "throwable.h"
#include "utils.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

Class* Class::java_lang_Class_ = NULL;

void Class::SetClassClass(Class* java_lang_Class) {
  CHECK(java_lang_Class_ == NULL) << java_lang_Class_ << " " << java_lang_Class;
  CHECK(java_lang_Class != NULL);
  java_lang_Class_ = java_lang_Class;
}

void Class::ResetClass() {
  CHECK(java_lang_Class_ != NULL);
  java_lang_Class_ = NULL;
}

void Class::SetStatus(Status new_status, Thread* self) {
  Status old_status = GetStatus();
  bool class_linker_initialized = Runtime::Current()->GetClassLinker() != nullptr;
  if (LIKELY(class_linker_initialized)) {
    if (UNLIKELY(new_status <= old_status && new_status != kStatusError)) {
      LOG(FATAL) << "Unexpected change back of class status for " << PrettyClass(this) << " "
          << old_status << " -> " << new_status;
    }
    if (new_status >= kStatusResolved || old_status >= kStatusResolved) {
      // When classes are being resolved the resolution code should hold the lock.
      CHECK_EQ(GetThinLockId(), self->GetThinLockId())
            << "Attempt to change status of class while not holding its lock: "
            << PrettyClass(this) << " " << old_status << " -> " << new_status;
    }
  }
  if (new_status == kStatusError) {
    CHECK_NE(GetStatus(), kStatusError)
        << "Attempt to set as erroneous an already erroneous class " << PrettyClass(this);

    // Stash current exception.
    SirtRef<mirror::Object> old_throw_this_object(self, NULL);
    SirtRef<mirror::ArtMethod> old_throw_method(self, NULL);
    SirtRef<mirror::Throwable> old_exception(self, NULL);
    uint32_t old_throw_dex_pc;
    {
      ThrowLocation old_throw_location;
      mirror::Throwable* old_exception_obj = self->GetException(&old_throw_location);
      old_throw_this_object.reset(old_throw_location.GetThis());
      old_throw_method.reset(old_throw_location.GetMethod());
      old_exception.reset(old_exception_obj);
      old_throw_dex_pc = old_throw_location.GetDexPc();
      self->ClearException();
    }
    CHECK(old_exception.get() != NULL);

    // clear exception to call FindSystemClass
    self->ClearException();
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Class* eiie_class = class_linker->FindSystemClass("Ljava/lang/ExceptionInInitializerError;");
    CHECK(!self->IsExceptionPending());

    // Only verification errors, not initialization problems, should set a verify error.
    // This is to ensure that ThrowEarlierClassFailure will throw NoClassDefFoundError in that case.
    Class* exception_class = old_exception->GetClass();
    if (!eiie_class->IsAssignableFrom(exception_class)) {
      SetVerifyErrorClass(exception_class);
    }

    // Restore exception.
    ThrowLocation gc_safe_throw_location(old_throw_this_object.get(), old_throw_method.get(),
                                         old_throw_dex_pc);

    self->SetException(gc_safe_throw_location, old_exception.get());
  }
  CHECK(sizeof(Status) == sizeof(uint32_t)) << PrettyClass(this);
  SetField32(OFFSET_OF_OBJECT_MEMBER(Class, status_), new_status, false);
  // Classes that are being resolved or initialized need to notify waiters that the class status
  // changed. See ClassLinker::EnsureResolved and ClassLinker::WaitForInitializeClass.
  if ((old_status >= kStatusResolved || new_status >= kStatusResolved) &&
      class_linker_initialized) {
    NotifyAll(self);
  }
}

void Class::SetDexCache(DexCache* new_dex_cache) {
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, dex_cache_), new_dex_cache, false);
}

Object* Class::AllocObject(Thread* self) {
  DCHECK(!IsArrayClass()) << PrettyClass(this);
  DCHECK(IsInstantiable()) << PrettyClass(this);
  // TODO: decide whether we want this check. It currently fails during bootstrap.
  // DCHECK(!Runtime::Current()->IsStarted() || IsInitializing()) << PrettyClass(this);
  DCHECK_GE(this->object_size_, sizeof(Object));
  return Runtime::Current()->GetHeap()->AllocObject(self, this, this->object_size_);
}

void Class::SetClassSize(size_t new_class_size) {
  if (kIsDebugBuild && (new_class_size < GetClassSize())) {
    DumpClass(LOG(ERROR), kDumpClassFullDetail);
    CHECK_GE(new_class_size, GetClassSize()) << " class=" << PrettyTypeOf(this);
  }
  SetField32(OFFSET_OF_OBJECT_MEMBER(Class, class_size_), new_class_size, false);
}

// Return the class' name. The exact format is bizarre, but it's the specified behavior for
// Class.getName: keywords for primitive types, regular "[I" form for primitive arrays (so "int"
// but "[I"), and arrays of reference types written between "L" and ";" but with dots rather than
// slashes (so "java.lang.String" but "[Ljava.lang.String;"). Madness.
String* Class::ComputeName() {
  String* name = GetName();
  if (name != NULL) {
    return name;
  }
  std::string descriptor(ClassHelper(this).GetDescriptor());
  if ((descriptor[0] != 'L') && (descriptor[0] != '[')) {
    // The descriptor indicates that this is the class for
    // a primitive type; special-case the return value.
    const char* c_name = NULL;
    switch (descriptor[0]) {
    case 'Z': c_name = "boolean"; break;
    case 'B': c_name = "byte";    break;
    case 'C': c_name = "char";    break;
    case 'S': c_name = "short";   break;
    case 'I': c_name = "int";     break;
    case 'J': c_name = "long";    break;
    case 'F': c_name = "float";   break;
    case 'D': c_name = "double";  break;
    case 'V': c_name = "void";    break;
    default:
      LOG(FATAL) << "Unknown primitive type: " << PrintableChar(descriptor[0]);
    }
    name = String::AllocFromModifiedUtf8(Thread::Current(), c_name);
  } else {
    // Convert the UTF-8 name to a java.lang.String. The name must use '.' to separate package
    // components.
    if (descriptor.size() > 2 && descriptor[0] == 'L' && descriptor[descriptor.size() - 1] == ';') {
      descriptor.erase(0, 1);
      descriptor.erase(descriptor.size() - 1);
    }
    std::replace(descriptor.begin(), descriptor.end(), '/', '.');
    name = String::AllocFromModifiedUtf8(Thread::Current(), descriptor.c_str());
  }
  SetName(name);
  return name;
}

void Class::DumpClass(std::ostream& os, int flags) const {
  if ((flags & kDumpClassFullDetail) == 0) {
    os << PrettyClass(this);
    if ((flags & kDumpClassClassLoader) != 0) {
      os << ' ' << GetClassLoader();
    }
    if ((flags & kDumpClassInitialized) != 0) {
      os << ' ' << GetStatus();
    }
    os << "\n";
    return;
  }

  Class* super = GetSuperClass();
  ClassHelper kh(this);
  os << "----- " << (IsInterface() ? "interface" : "class") << " "
     << "'" << kh.GetDescriptor() << "' cl=" << GetClassLoader() << " -----\n",
  os << "  objectSize=" << SizeOf() << " "
     << "(" << (super != NULL ? super->SizeOf() : -1) << " from super)\n",
  os << StringPrintf("  access=0x%04x.%04x\n",
      GetAccessFlags() >> 16, GetAccessFlags() & kAccJavaFlagsMask);
  if (super != NULL) {
    os << "  super='" << PrettyClass(super) << "' (cl=" << super->GetClassLoader() << ")\n";
  }
  if (IsArrayClass()) {
    os << "  componentType=" << PrettyClass(GetComponentType()) << "\n";
  }
  if (kh.NumDirectInterfaces() > 0) {
    os << "  interfaces (" << kh.NumDirectInterfaces() << "):\n";
    for (size_t i = 0; i < kh.NumDirectInterfaces(); ++i) {
      Class* interface = kh.GetDirectInterface(i);
      const ClassLoader* cl = interface->GetClassLoader();
      os << StringPrintf("    %2zd: %s (cl=%p)\n", i, PrettyClass(interface).c_str(), cl);
    }
  }
  os << "  vtable (" << NumVirtualMethods() << " entries, "
     << (super != NULL ? super->NumVirtualMethods() : 0) << " in super):\n";
  for (size_t i = 0; i < NumVirtualMethods(); ++i) {
    os << StringPrintf("    %2zd: %s\n", i, PrettyMethod(GetVirtualMethodDuringLinking(i)).c_str());
  }
  os << "  direct methods (" << NumDirectMethods() << " entries):\n";
  for (size_t i = 0; i < NumDirectMethods(); ++i) {
    os << StringPrintf("    %2zd: %s\n", i, PrettyMethod(GetDirectMethod(i)).c_str());
  }
  if (NumStaticFields() > 0) {
    os << "  static fields (" << NumStaticFields() << " entries):\n";
    if (IsResolved() || IsErroneous()) {
      for (size_t i = 0; i < NumStaticFields(); ++i) {
        os << StringPrintf("    %2zd: %s\n", i, PrettyField(GetStaticField(i)).c_str());
      }
    } else {
      os << "    <not yet available>";
    }
  }
  if (NumInstanceFields() > 0) {
    os << "  instance fields (" << NumInstanceFields() << " entries):\n";
    if (IsResolved() || IsErroneous()) {
      for (size_t i = 0; i < NumInstanceFields(); ++i) {
        os << StringPrintf("    %2zd: %s\n", i, PrettyField(GetInstanceField(i)).c_str());
      }
    } else {
      os << "    <not yet available>";
    }
  }
}

void Class::SetReferenceInstanceOffsets(uint32_t new_reference_offsets) {
  if (new_reference_offsets != CLASS_WALK_SUPER) {
    // Sanity check that the number of bits set in the reference offset bitmap
    // agrees with the number of references
    size_t count = 0;
    for (Class* c = this; c != NULL; c = c->GetSuperClass()) {
      count += c->NumReferenceInstanceFieldsDuringLinking();
    }
    CHECK_EQ((size_t)__builtin_popcount(new_reference_offsets), count);
  }
  SetField32(OFFSET_OF_OBJECT_MEMBER(Class, reference_instance_offsets_),
             new_reference_offsets, false);
}

void Class::SetReferenceStaticOffsets(uint32_t new_reference_offsets) {
  if (new_reference_offsets != CLASS_WALK_SUPER) {
    // Sanity check that the number of bits set in the reference offset bitmap
    // agrees with the number of references
    CHECK_EQ((size_t)__builtin_popcount(new_reference_offsets),
             NumReferenceStaticFieldsDuringLinking());
  }
  SetField32(OFFSET_OF_OBJECT_MEMBER(Class, reference_static_offsets_),
             new_reference_offsets, false);
}

bool Class::IsInSamePackage(const StringPiece& descriptor1, const StringPiece& descriptor2) {
  size_t i = 0;
  while (descriptor1[i] != '\0' && descriptor1[i] == descriptor2[i]) {
    ++i;
  }
  if (descriptor1.find('/', i) != StringPiece::npos ||
      descriptor2.find('/', i) != StringPiece::npos) {
    return false;
  } else {
    return true;
  }
}

bool Class::IsInSamePackage(const Class* that) const {
  const Class* klass1 = this;
  const Class* klass2 = that;
  if (klass1 == klass2) {
    return true;
  }
  // Class loaders must match.
  if (klass1->GetClassLoader() != klass2->GetClassLoader()) {
    return false;
  }
  // Arrays are in the same package when their element classes are.
  while (klass1->IsArrayClass()) {
    klass1 = klass1->GetComponentType();
  }
  while (klass2->IsArrayClass()) {
    klass2 = klass2->GetComponentType();
  }
  // trivial check again for array types
  if (klass1 == klass2) {
    return true;
  }
  // Compare the package part of the descriptor string.
  return IsInSamePackage(ClassHelper(klass1).GetDescriptor(),
                         ClassHelper(klass2).GetDescriptor());
}

bool Class::IsClassClass() const {
  Class* java_lang_Class = GetClass()->GetClass();
  return this == java_lang_Class;
}

bool Class::IsStringClass() const {
  return this == String::GetJavaLangString();
}

bool Class::IsThrowableClass() const {
  return WellKnownClasses::ToClass(WellKnownClasses::java_lang_Throwable)->IsAssignableFrom(this);
}

bool Class::IsArtFieldClass() const {
  Class* java_lang_Class = GetClass();
  Class* java_lang_reflect_ArtField = java_lang_Class->GetInstanceField(0)->GetClass();
  return this == java_lang_reflect_ArtField;
}

bool Class::IsArtMethodClass() const {
  return this == ArtMethod::GetJavaLangReflectArtMethod();
}

void Class::SetClassLoader(ClassLoader* new_class_loader) {
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Class, class_loader_), new_class_loader, false);
}

ArtMethod* Class::FindInterfaceMethod(const StringPiece& name, const StringPiece& signature) const {
  // Check the current class before checking the interfaces.
  ArtMethod* method = FindDeclaredVirtualMethod(name, signature);
  if (method != NULL) {
    return method;
  }

  int32_t iftable_count = GetIfTableCount();
  IfTable* iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; i++) {
    method = iftable->GetInterface(i)->FindDeclaredVirtualMethod(name, signature);
    if (method != NULL) {
      return method;
    }
  }
  return NULL;
}

ArtMethod* Class::FindInterfaceMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const {
  // Check the current class before checking the interfaces.
  ArtMethod* method = FindDeclaredVirtualMethod(dex_cache, dex_method_idx);
  if (method != NULL) {
    return method;
  }

  int32_t iftable_count = GetIfTableCount();
  IfTable* iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; i++) {
    method = iftable->GetInterface(i)->FindDeclaredVirtualMethod(dex_cache, dex_method_idx);
    if (method != NULL) {
      return method;
    }
  }
  return NULL;
}


ArtMethod* Class::FindDeclaredDirectMethod(const StringPiece& name, const StringPiece& signature) const {
  MethodHelper mh;
  for (size_t i = 0; i < NumDirectMethods(); ++i) {
    ArtMethod* method = GetDirectMethod(i);
    mh.ChangeMethod(method);
    if (name == mh.GetName() && signature == mh.GetSignature()) {
      return method;
    }
  }
  return NULL;
}

ArtMethod* Class::FindDeclaredDirectMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const {
  if (GetDexCache() == dex_cache) {
    for (size_t i = 0; i < NumDirectMethods(); ++i) {
      ArtMethod* method = GetDirectMethod(i);
      if (method->GetDexMethodIndex() == dex_method_idx) {
        return method;
      }
    }
  }
  return NULL;
}

ArtMethod* Class::FindDirectMethod(const StringPiece& name, const StringPiece& signature) const {
  for (const Class* klass = this; klass != NULL; klass = klass->GetSuperClass()) {
    ArtMethod* method = klass->FindDeclaredDirectMethod(name, signature);
    if (method != NULL) {
      return method;
    }
  }
  return NULL;
}

ArtMethod* Class::FindDirectMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const {
  for (const Class* klass = this; klass != NULL; klass = klass->GetSuperClass()) {
    ArtMethod* method = klass->FindDeclaredDirectMethod(dex_cache, dex_method_idx);
    if (method != NULL) {
      return method;
    }
  }
  return NULL;
}

ArtMethod* Class::FindDeclaredVirtualMethod(const StringPiece& name,
                                         const StringPiece& signature) const {
  MethodHelper mh;
  for (size_t i = 0; i < NumVirtualMethods(); ++i) {
    ArtMethod* method = GetVirtualMethod(i);
    mh.ChangeMethod(method);
    if (name == mh.GetName() && signature == mh.GetSignature()) {
      return method;
    }
  }
  return NULL;
}

ArtMethod* Class::FindDeclaredVirtualMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const {
  if (GetDexCache() == dex_cache) {
    for (size_t i = 0; i < NumVirtualMethods(); ++i) {
      ArtMethod* method = GetVirtualMethod(i);
      if (method->GetDexMethodIndex() == dex_method_idx) {
        return method;
      }
    }
  }
  return NULL;
}

ArtMethod* Class::FindVirtualMethod(const StringPiece& name, const StringPiece& signature) const {
  for (const Class* klass = this; klass != NULL; klass = klass->GetSuperClass()) {
    ArtMethod* method = klass->FindDeclaredVirtualMethod(name, signature);
    if (method != NULL) {
      return method;
    }
  }
  return NULL;
}

ArtMethod* Class::FindVirtualMethod(const DexCache* dex_cache, uint32_t dex_method_idx) const {
  for (const Class* klass = this; klass != NULL; klass = klass->GetSuperClass()) {
    ArtMethod* method = klass->FindDeclaredVirtualMethod(dex_cache, dex_method_idx);
    if (method != NULL) {
      return method;
    }
  }
  return NULL;
}

ArtField* Class::FindDeclaredInstanceField(const StringPiece& name, const StringPiece& type) {
  // Is the field in this class?
  // Interfaces are not relevant because they can't contain instance fields.
  FieldHelper fh;
  for (size_t i = 0; i < NumInstanceFields(); ++i) {
    ArtField* f = GetInstanceField(i);
    fh.ChangeField(f);
    if (name == fh.GetName() && type == fh.GetTypeDescriptor()) {
      return f;
    }
  }
  return NULL;
}

ArtField* Class::FindDeclaredInstanceField(const DexCache* dex_cache, uint32_t dex_field_idx) {
  if (GetDexCache() == dex_cache) {
    for (size_t i = 0; i < NumInstanceFields(); ++i) {
      ArtField* f = GetInstanceField(i);
      if (f->GetDexFieldIndex() == dex_field_idx) {
        return f;
      }
    }
  }
  return NULL;
}

ArtField* Class::FindInstanceField(const StringPiece& name, const StringPiece& type) {
  // Is the field in this class, or any of its superclasses?
  // Interfaces are not relevant because they can't contain instance fields.
  for (Class* c = this; c != NULL; c = c->GetSuperClass()) {
    ArtField* f = c->FindDeclaredInstanceField(name, type);
    if (f != NULL) {
      return f;
    }
  }
  return NULL;
}

ArtField* Class::FindInstanceField(const DexCache* dex_cache, uint32_t dex_field_idx) {
  // Is the field in this class, or any of its superclasses?
  // Interfaces are not relevant because they can't contain instance fields.
  for (Class* c = this; c != NULL; c = c->GetSuperClass()) {
    ArtField* f = c->FindDeclaredInstanceField(dex_cache, dex_field_idx);
    if (f != NULL) {
      return f;
    }
  }
  return NULL;
}

ArtField* Class::FindDeclaredStaticField(const StringPiece& name, const StringPiece& type) {
  DCHECK(type != NULL);
  FieldHelper fh;
  for (size_t i = 0; i < NumStaticFields(); ++i) {
    ArtField* f = GetStaticField(i);
    fh.ChangeField(f);
    if (name == fh.GetName() && type == fh.GetTypeDescriptor()) {
      return f;
    }
  }
  return NULL;
}

ArtField* Class::FindDeclaredStaticField(const DexCache* dex_cache, uint32_t dex_field_idx) {
  if (dex_cache == GetDexCache()) {
    for (size_t i = 0; i < NumStaticFields(); ++i) {
      ArtField* f = GetStaticField(i);
      if (f->GetDexFieldIndex() == dex_field_idx) {
        return f;
      }
    }
  }
  return NULL;
}

ArtField* Class::FindStaticField(const StringPiece& name, const StringPiece& type) {
  // Is the field in this class (or its interfaces), or any of its
  // superclasses (or their interfaces)?
  ClassHelper kh;
  for (Class* k = this; k != NULL; k = k->GetSuperClass()) {
    // Is the field in this class?
    ArtField* f = k->FindDeclaredStaticField(name, type);
    if (f != NULL) {
      return f;
    }
    // Is this field in any of this class' interfaces?
    kh.ChangeClass(k);
    for (uint32_t i = 0; i < kh.NumDirectInterfaces(); ++i) {
      Class* interface = kh.GetDirectInterface(i);
      f = interface->FindStaticField(name, type);
      if (f != NULL) {
        return f;
      }
    }
  }
  return NULL;
}

ArtField* Class::FindStaticField(const DexCache* dex_cache, uint32_t dex_field_idx) {
  ClassHelper kh;
  for (Class* k = this; k != NULL; k = k->GetSuperClass()) {
    // Is the field in this class?
    ArtField* f = k->FindDeclaredStaticField(dex_cache, dex_field_idx);
    if (f != NULL) {
      return f;
    }
    // Is this field in any of this class' interfaces?
    kh.ChangeClass(k);
    for (uint32_t i = 0; i < kh.NumDirectInterfaces(); ++i) {
      Class* interface = kh.GetDirectInterface(i);
      f = interface->FindStaticField(dex_cache, dex_field_idx);
      if (f != NULL) {
        return f;
      }
    }
  }
  return NULL;
}

ArtField* Class::FindField(const StringPiece& name, const StringPiece& type) {
  // Find a field using the JLS field resolution order
  ClassHelper kh;
  for (Class* k = this; k != NULL; k = k->GetSuperClass()) {
    // Is the field in this class?
    ArtField* f = k->FindDeclaredInstanceField(name, type);
    if (f != NULL) {
      return f;
    }
    f = k->FindDeclaredStaticField(name, type);
    if (f != NULL) {
      return f;
    }
    // Is this field in any of this class' interfaces?
    kh.ChangeClass(k);
    for (uint32_t i = 0; i < kh.NumDirectInterfaces(); ++i) {
      Class* interface = kh.GetDirectInterface(i);
      f = interface->FindStaticField(name, type);
      if (f != NULL) {
        return f;
      }
    }
  }
  return NULL;
}

static void SetPreverifiedFlagOnMethods(mirror::ObjectArray<mirror::ArtMethod>* methods)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (methods != NULL) {
    for (int32_t index = 0, end = methods->GetLength(); index < end; ++index) {
      mirror::ArtMethod* method = methods->GetWithoutChecks(index);
      DCHECK(method != NULL);
      method->SetPreverified();
    }
  }
}

void Class::SetPreverifiedFlagOnAllMethods() {
  DCHECK(IsVerified());
  SetPreverifiedFlagOnMethods(GetDirectMethods());
  SetPreverifiedFlagOnMethods(GetVirtualMethods());
}

}  // namespace mirror
}  // namespace art
