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

#include "reg_type_cache-inl.h"

#include "base/casts.h"
#include "dex_file-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"

namespace art {
namespace verifier {

bool RegTypeCache::primitive_initialized_ = false;
uint16_t RegTypeCache::primitive_start_ = 0;
uint16_t RegTypeCache::primitive_count_ = 0;

static bool MatchingPrecisionForClass(RegType* entry, bool precise)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (entry->IsPreciseReference() == precise) {
    // We were or weren't looking for a precise reference and we found what we need.
    return true;
  } else {
    if (!precise && entry->GetClass()->CannotBeAssignedFromOtherTypes()) {
      // We weren't looking for a precise reference, as we're looking up based on a descriptor, but
      // we found a matching entry based on the descriptor. Return the precise entry in that case.
      return true;
    }
    return false;
  }
}

void RegTypeCache::FillPrimitiveTypes() {
  entries_.push_back(UndefinedType::GetInstance());
  entries_.push_back(ConflictType::GetInstance());
  entries_.push_back(BooleanType::GetInstance());
  entries_.push_back(ByteType::GetInstance());
  entries_.push_back(ShortType::GetInstance());
  entries_.push_back(CharType::GetInstance());
  entries_.push_back(IntegerType::GetInstance());
  entries_.push_back(LongLoType::GetInstance());
  entries_.push_back(LongHiType::GetInstance());
  entries_.push_back(FloatType::GetInstance());
  entries_.push_back(DoubleLoType::GetInstance());
  entries_.push_back(DoubleHiType::GetInstance());
  DCHECK_EQ(entries_.size(), primitive_count_);
}

const RegType& RegTypeCache::FromDescriptor(mirror::ClassLoader* loader, const char* descriptor,
                                            bool precise) {
  DCHECK(RegTypeCache::primitive_initialized_);
  if (descriptor[1] == '\0') {
    switch (descriptor[0]) {
      case 'Z':
        return Boolean();
      case 'B':
        return Byte();
      case 'S':
        return Short();
      case 'C':
        return Char();
      case 'I':
        return Integer();
      case 'J':
        return LongLo();
      case 'F':
        return Float();
      case 'D':
        return DoubleLo();
      case 'V':  // For void types, conflict types.
      default:
        return Conflict();
    }
  } else if (descriptor[0] == 'L' || descriptor[0] == '[') {
    return From(loader, descriptor, precise);
  } else {
    return Conflict();
  }
};

const RegType& RegTypeCache::RegTypeFromPrimitiveType(Primitive::Type prim_type) const {
  CHECK(RegTypeCache::primitive_initialized_);
  switch (prim_type) {
    case Primitive::kPrimBoolean:
      return *BooleanType::GetInstance();
    case Primitive::kPrimByte:
      return *ByteType::GetInstance();
    case Primitive::kPrimShort:
      return *ShortType::GetInstance();
    case Primitive::kPrimChar:
      return *CharType::GetInstance();
    case Primitive::kPrimInt:
      return *IntegerType::GetInstance();
    case Primitive::kPrimLong:
      return *LongLoType::GetInstance();
    case Primitive::kPrimFloat:
      return *FloatType::GetInstance();
    case Primitive::kPrimDouble:
      return *DoubleLoType::GetInstance();
    case Primitive::kPrimVoid:
    default:
      return *ConflictType::GetInstance();
  }
}

bool RegTypeCache::MatchDescriptor(size_t idx, const char* descriptor, bool precise) {
  RegType* entry = entries_[idx];
  if (entry->descriptor_ != descriptor) {
    return false;
  }
  if (entry->HasClass()) {
    return MatchingPrecisionForClass(entry, precise);
  }
  // There is no notion of precise unresolved references, the precise information is just dropped
  // on the floor.
  DCHECK(entry->IsUnresolvedReference());
  return true;
}

mirror::Class* RegTypeCache::ResolveClass(const char* descriptor, mirror::ClassLoader* loader) {
  // Class was not found, must create new type.
  // Try resolving class
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  mirror::Class* klass = NULL;
  if (can_load_classes_) {
    klass = class_linker->FindClass(descriptor, loader);
  } else {
    klass = class_linker->LookupClass(descriptor, loader);
    if (klass != NULL && !klass->IsLoaded()) {
      // We found the class but without it being loaded its not safe for use.
      klass = NULL;
    }
  }
  return klass;
}

void RegTypeCache::ClearException() {
  if (can_load_classes_) {
    DCHECK(Thread::Current()->IsExceptionPending());
    Thread::Current()->ClearException();
  } else {
    DCHECK(!Thread::Current()->IsExceptionPending());
  }
}

const RegType& RegTypeCache::From(mirror::ClassLoader* loader, const char* descriptor,
                                  bool precise) {
  // Try looking up the class in the cache first.
  for (size_t i = primitive_count_; i < entries_.size(); i++) {
    if (MatchDescriptor(i, descriptor, precise)) {
      return *(entries_[i]);
    }
  }
  // Class not found in the cache, will create a new type for that.
  // Try resolving class.
  mirror::Class* klass = ResolveClass(descriptor, loader);
  if (klass != NULL) {
    // Class resolved, first look for the class in the list of entries
    // Class was not found, must create new type.
    // To pass the verification, the type should be imprecise,
    // instantiable or an interface with the precise type set to false.
    DCHECK(!precise || klass->IsInstantiable());
    // Create a precise type if:
    // 1- Class is final and NOT an interface. a precise interface is meaningless !!
    // 2- Precise Flag passed as true.
    RegType* entry;
    // Create an imprecise type if we can't tell for a fact that it is precise.
    if (klass->CannotBeAssignedFromOtherTypes() || precise) {
      DCHECK(!(klass->IsAbstract()) || klass->IsArrayClass());
      DCHECK(!klass->IsInterface());
      entry = new PreciseReferenceType(klass, descriptor, entries_.size());
    } else {
      entry = new ReferenceType(klass, descriptor, entries_.size());
    }
    entries_.push_back(entry);
    return *entry;
  } else {  // Class not resolved.
    // We tried loading the class and failed, this might get an exception raised
    // so we want to clear it before we go on.
    ClearException();
    if (IsValidDescriptor(descriptor)) {
      RegType* entry = new UnresolvedReferenceType(descriptor, entries_.size());
      entries_.push_back(entry);
      return *entry;
    } else {
      // The descriptor is broken return the unknown type as there's nothing sensible that
      // could be done at runtime
      return Conflict();
    }
  }
}

const RegType& RegTypeCache::FromClass(const char* descriptor, mirror::Class* klass, bool precise) {
  if (klass->IsPrimitive()) {
    // Note: precise isn't used for primitive classes. A char is assignable to an int. All
    // primitive classes are final.
    return RegTypeFromPrimitiveType(klass->GetPrimitiveType());
  } else {
    // Look for the reference in the list of entries to have.
    for (size_t i = primitive_count_; i < entries_.size(); i++) {
      RegType* cur_entry = entries_[i];
      if (cur_entry->klass_ == klass && MatchingPrecisionForClass(cur_entry, precise)) {
        return *cur_entry;
      }
    }
    // No reference to the class was found, create new reference.
    RegType* entry;
    if (precise) {
      entry = new PreciseReferenceType(klass, descriptor, entries_.size());
    } else {
      entry = new ReferenceType(klass, descriptor, entries_.size());
    }
    entries_.push_back(entry);
    return *entry;
  }
}

RegTypeCache::~RegTypeCache() {
  CHECK_LE(primitive_count_, entries_.size());
  // Delete only the non primitive types.
  if (entries_.size() == kNumPrimitives) {
    // All entries are primitive, nothing to delete.
    return;
  }
  std::vector<RegType*>::iterator non_primitive_begin = entries_.begin();
  std::advance(non_primitive_begin, kNumPrimitives);
  STLDeleteContainerPointers(non_primitive_begin, entries_.end());
}

void RegTypeCache::ShutDown() {
  if (RegTypeCache::primitive_initialized_) {
    UndefinedType::Destroy();
    ConflictType::Destroy();
    BooleanType::Destroy();
    ByteType::Destroy();
    ShortType::Destroy();
    CharType::Destroy();
    IntegerType::Destroy();
    LongLoType::Destroy();
    LongHiType::Destroy();
    FloatType::Destroy();
    DoubleLoType::Destroy();
    DoubleHiType::Destroy();
    RegTypeCache::primitive_initialized_ = false;
    RegTypeCache::primitive_count_ = 0;
  }
}

void RegTypeCache::CreatePrimitiveTypes() {
  CreatePrimitiveTypeInstance<UndefinedType>("");
  CreatePrimitiveTypeInstance<ConflictType>("");
  CreatePrimitiveTypeInstance<BooleanType>("Z");
  CreatePrimitiveTypeInstance<ByteType>("B");
  CreatePrimitiveTypeInstance<ShortType>("S");
  CreatePrimitiveTypeInstance<CharType>("C");
  CreatePrimitiveTypeInstance<IntegerType>("I");
  CreatePrimitiveTypeInstance<LongLoType>("J");
  CreatePrimitiveTypeInstance<LongHiType>("J");
  CreatePrimitiveTypeInstance<FloatType>("F");
  CreatePrimitiveTypeInstance<DoubleLoType>("D");
  CreatePrimitiveTypeInstance<DoubleHiType>("D");
}

const RegType& RegTypeCache::FromUnresolvedMerge(const RegType& left, const RegType& right) {
  std::set<uint16_t> types;
  if (left.IsUnresolvedMergedReference()) {
    RegType& non_const(const_cast<RegType&>(left));
    types = (down_cast<UnresolvedMergedType*>(&non_const))->GetMergedTypes();
  } else {
    types.insert(left.GetId());
  }
  if (right.IsUnresolvedMergedReference()) {
    RegType& non_const(const_cast<RegType&>(right));
    std::set<uint16_t> right_types = (down_cast<UnresolvedMergedType*>(&non_const))->GetMergedTypes();
    types.insert(right_types.begin(), right_types.end());
  } else {
    types.insert(right.GetId());
  }
  // Check if entry already exists.
  for (size_t i = primitive_count_; i < entries_.size(); i++) {
    RegType* cur_entry = entries_[i];
    if (cur_entry->IsUnresolvedMergedReference()) {
      std::set<uint16_t> cur_entry_types =
          (down_cast<UnresolvedMergedType*>(cur_entry))->GetMergedTypes();
      if (cur_entry_types == types) {
        return *cur_entry;
      }
    }
  }
  // Create entry.
  RegType* entry = new UnresolvedMergedType(left.GetId(), right.GetId(), this, entries_.size());
  entries_.push_back(entry);
  if (kIsDebugBuild) {
    UnresolvedMergedType* tmp_entry = down_cast<UnresolvedMergedType*>(entry);
    std::set<uint16_t> check_types = tmp_entry->GetMergedTypes();
    CHECK(check_types == types);
  }
  return *entry;
}

const RegType& RegTypeCache::FromUnresolvedSuperClass(const RegType& child) {
  // Check if entry already exists.
  for (size_t i = primitive_count_; i < entries_.size(); i++) {
    RegType* cur_entry = entries_[i];
    if (cur_entry->IsUnresolvedSuperClass()) {
      UnresolvedSuperClass* tmp_entry =
          down_cast<UnresolvedSuperClass*>(cur_entry);
      uint16_t unresolved_super_child_id =
          tmp_entry->GetUnresolvedSuperClassChildId();
      if (unresolved_super_child_id == child.GetId()) {
        return *cur_entry;
      }
    }
  }
  RegType* entry = new UnresolvedSuperClass(child.GetId(), this, entries_.size());
  entries_.push_back(entry);
  return *entry;
}

const RegType& RegTypeCache::Uninitialized(const RegType& type, uint32_t allocation_pc) {
  RegType* entry = NULL;
  RegType* cur_entry = NULL;
  const std::string& descriptor(type.GetDescriptor());
  if (type.IsUnresolvedTypes()) {
    for (size_t i = primitive_count_; i < entries_.size(); i++) {
      cur_entry = entries_[i];
      if (cur_entry->IsUnresolvedAndUninitializedReference() &&
          down_cast<UnresolvedUninitializedRefType*>(cur_entry)->GetAllocationPc() == allocation_pc &&
          (cur_entry->GetDescriptor() == descriptor)) {
        return *cur_entry;
      }
    }
    entry = new UnresolvedUninitializedRefType(descriptor, allocation_pc, entries_.size());
  } else {
    mirror::Class* klass = type.GetClass();
    for (size_t i = primitive_count_; i < entries_.size(); i++) {
      cur_entry = entries_[i];
      if (cur_entry->IsUninitializedReference() &&
          down_cast<UninitializedReferenceType*>(cur_entry)
              ->GetAllocationPc() == allocation_pc &&
          cur_entry->GetClass() == klass) {
        return *cur_entry;
      }
    }
    entry = new UninitializedReferenceType(klass, descriptor, allocation_pc, entries_.size());
  }
  entries_.push_back(entry);
  return *entry;
}

const RegType& RegTypeCache::FromUninitialized(const RegType& uninit_type) {
  RegType* entry;

  if (uninit_type.IsUnresolvedTypes()) {
    const std::string& descriptor(uninit_type.GetDescriptor());
    for (size_t i = primitive_count_; i < entries_.size(); i++) {
      RegType* cur_entry = entries_[i];
      if (cur_entry->IsUnresolvedReference() &&
          cur_entry->GetDescriptor() == descriptor) {
        return *cur_entry;
      }
    }
    entry = new UnresolvedReferenceType(descriptor.c_str(), entries_.size());
  } else {
    mirror::Class* klass = uninit_type.GetClass();
    if (uninit_type.IsUninitializedThisReference() && !klass->IsFinal()) {
      // For uninitialized "this reference" look for reference types that are not precise.
      for (size_t i = primitive_count_; i < entries_.size(); i++) {
        RegType* cur_entry = entries_[i];
        if (cur_entry->IsReference() && cur_entry->GetClass() == klass) {
          return *cur_entry;
        }
      }
      entry = new ReferenceType(klass, "", entries_.size());
    } else if (klass->IsInstantiable()) {
      // We're uninitialized because of allocation, look or create a precise type as allocations
      // may only create objects of that type.
      for (size_t i = primitive_count_; i < entries_.size(); i++) {
        RegType* cur_entry = entries_[i];
        if (cur_entry->IsPreciseReference() && cur_entry->GetClass() == klass) {
          return *cur_entry;
        }
      }
      entry = new PreciseReferenceType(klass, uninit_type.GetDescriptor(), entries_.size());
    } else {
      return Conflict();
    }
  }
  entries_.push_back(entry);
  return *entry;
}

const RegType& RegTypeCache::ByteConstant() {
  return FromCat1Const(std::numeric_limits<jbyte>::min(), false);
}

const RegType& RegTypeCache::ShortConstant() {
  return FromCat1Const(std::numeric_limits<jshort>::min(), false);
}

const RegType& RegTypeCache::IntConstant() {
  return FromCat1Const(std::numeric_limits<jint>::max(), false);
}

const RegType& RegTypeCache::UninitializedThisArgument(const RegType& type) {
  RegType* entry;
  const std::string& descriptor(type.GetDescriptor());
  if (type.IsUnresolvedTypes()) {
    for (size_t i = primitive_count_; i < entries_.size(); i++) {
      RegType* cur_entry = entries_[i];
      if (cur_entry->IsUnresolvedAndUninitializedThisReference() &&
          cur_entry->GetDescriptor() == descriptor) {
        return *cur_entry;
      }
    }
    entry = new UnresolvedUninitializedThisRefType(descriptor, entries_.size());
  } else {
    mirror::Class* klass = type.GetClass();
    for (size_t i = primitive_count_; i < entries_.size(); i++) {
      RegType* cur_entry = entries_[i];
      if (cur_entry->IsUninitializedThisReference() && cur_entry->GetClass() == klass) {
        return *cur_entry;
      }
    }
    entry = new UninitializedThisReferenceType(klass, descriptor, entries_.size());
  }
  entries_.push_back(entry);
  return *entry;
}

const RegType& RegTypeCache::FromCat1Const(int32_t value, bool precise) {
  for (size_t i = primitive_count_; i < entries_.size(); i++) {
    RegType* cur_entry = entries_[i];
    if (cur_entry->klass_ == NULL && cur_entry->IsConstant() &&
        cur_entry->IsPreciseConstant() == precise &&
        (down_cast<ConstantType*>(cur_entry))->ConstantValue() == value) {
      return *cur_entry;
    }
  }
  RegType* entry;
  if (precise) {
    entry = new PreciseConstType(value, entries_.size());
  } else {
    entry = new ImpreciseConstType(value, entries_.size());
  }
  entries_.push_back(entry);
  return *entry;
}

const RegType& RegTypeCache::FromCat2ConstLo(int32_t value, bool precise) {
  for (size_t i = primitive_count_; i < entries_.size(); i++) {
    RegType* cur_entry = entries_[i];
    if (cur_entry->IsConstantLo() && (cur_entry->IsPrecise() == precise) &&
        (down_cast<ConstantType*>(cur_entry))->ConstantValueLo() == value) {
      return *cur_entry;
    }
  }
  RegType* entry;
  if (precise) {
    entry = new PreciseConstLoType(value, entries_.size());
  } else {
    entry = new ImpreciseConstLoType(value, entries_.size());
  }
  entries_.push_back(entry);
  return *entry;
}

const RegType& RegTypeCache::FromCat2ConstHi(int32_t value, bool precise) {
  for (size_t i = primitive_count_; i < entries_.size(); i++) {
    RegType* cur_entry = entries_[i];
    if (cur_entry->IsConstantHi() && (cur_entry->IsPrecise() == precise) &&
        (down_cast<ConstantType*>(cur_entry))->ConstantValueHi() == value) {
      return *cur_entry;
    }
  }
  RegType* entry;
  if (precise) {
    entry = new PreciseConstHiType(value, entries_.size());
  } else {
    entry = new ImpreciseConstHiType(value, entries_.size());
  }
  entries_.push_back(entry);
  return *entry;
}

const RegType& RegTypeCache::GetComponentType(const RegType& array, mirror::ClassLoader* loader) {
  CHECK(array.IsArrayTypes());
  if (array.IsUnresolvedTypes()) {
    const std::string& descriptor(array.GetDescriptor());
    const std::string component(descriptor.substr(1, descriptor.size() - 1));
    return FromDescriptor(loader, component.c_str(), false);
  } else {
    mirror::Class* klass = array.GetClass()->GetComponentType();
    return FromClass(ClassHelper(klass).GetDescriptor(), klass,
                     klass->CannotBeAssignedFromOtherTypes());
  }
}

void RegTypeCache::Dump(std::ostream& os) {
  for (size_t i = 0; i < entries_.size(); i++) {
    RegType* cur_entry = entries_[i];
    if (cur_entry != NULL) {
      os << i << ": " << cur_entry->Dump() << "\n";
    }
  }
}

}  // namespace verifier
}  // namespace art
