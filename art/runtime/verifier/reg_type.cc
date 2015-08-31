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

#include "reg_type.h"


#include "base/casts.h"
#include "dex_file-inl.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "object_utils.h"
#include "reg_type_cache-inl.h"
#include "scoped_thread_state_change.h"

#include <limits>
#include <sstream>

namespace art {
namespace verifier {

UndefinedType* UndefinedType::instance_ = NULL;
ConflictType* ConflictType::instance_ = NULL;
BooleanType* BooleanType::instance = NULL;
ByteType* ByteType::instance_ = NULL;
ShortType* ShortType::instance_ = NULL;
CharType* CharType::instance_ = NULL;
FloatType* FloatType::instance_ = NULL;
LongLoType* LongLoType::instance_ = NULL;
LongHiType* LongHiType::instance_ = NULL;
DoubleLoType* DoubleLoType::instance_ = NULL;
DoubleHiType* DoubleHiType::instance_ = NULL;
IntegerType* IntegerType::instance_ = NULL;

int32_t RegType::ConstantValue() const {
  ScopedObjectAccess soa(Thread::Current());
  LOG(FATAL) << "Unexpected call to ConstantValue: " << *this;
  return 0;
}

int32_t RegType::ConstantValueLo() const {
  ScopedObjectAccess soa(Thread::Current());
  LOG(FATAL) << "Unexpected call to ConstantValueLo: " << *this;
  return 0;
}

int32_t RegType::ConstantValueHi() const {
  ScopedObjectAccess soa(Thread::Current());
  LOG(FATAL) << "Unexpected call to ConstantValueHi: " << *this;
  return 0;
}

PrimitiveType::PrimitiveType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
    : RegType(klass, descriptor, cache_id) {
  CHECK(klass != NULL);
  CHECK(!descriptor.empty());
}

Cat1Type::Cat1Type(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
    : PrimitiveType(klass, descriptor, cache_id) {
}

Cat2Type::Cat2Type(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
    : PrimitiveType(klass, descriptor, cache_id) {
}

std::string PreciseConstType::Dump() const {
  std::stringstream result;
  uint32_t val = ConstantValue();
  if (val == 0) {
    CHECK(IsPreciseConstant());
    result << "Zero/null";
  } else {
    result << "Precise ";
    if (IsConstantShort()) {
      result << StringPrintf("Constant: %d", val);
    } else {
      result << StringPrintf("Constant: 0x%x", val);
    }
  }
  return result.str();
}

std::string BooleanType::Dump() const {
  return "boolean";
}

std::string ConflictType::Dump() const {
    return "Conflict";
}

std::string ByteType::Dump() const {
  return "Byte";
}

std::string ShortType::Dump() const {
  return "short";
}

std::string CharType::Dump() const {
  return "Char";
}

std::string FloatType::Dump() const {
  return "float";
}

std::string LongLoType::Dump() const {
  return "long (Low Half)";
}

std::string LongHiType::Dump() const {
  return "long (High Half)";
}

std::string DoubleLoType::Dump() const {
  return "Double (Low Half)";
}

std::string DoubleHiType::Dump() const {
  return "Double (High Half)";
}

std::string IntegerType::Dump() const {
    return "Integer";
}

DoubleHiType* DoubleHiType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                           uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new DoubleHiType(klass, descriptor, cache_id);
  }
  return instance_;
}

DoubleHiType* DoubleHiType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void DoubleHiType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

DoubleLoType* DoubleLoType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                           uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new DoubleLoType(klass, descriptor, cache_id);
  }
  return instance_;
}

DoubleLoType* DoubleLoType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void DoubleLoType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

LongLoType* LongLoType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                       uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new LongLoType(klass, descriptor, cache_id);
  }
  return instance_;
}

LongHiType* LongHiType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                       uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new LongHiType(klass, descriptor, cache_id);
  }
  return instance_;
}

LongHiType* LongHiType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void LongHiType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

LongLoType* LongLoType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void LongLoType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

FloatType* FloatType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                     uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new FloatType(klass, descriptor, cache_id);
  }
  return instance_;
}
FloatType* FloatType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void FloatType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

CharType* CharType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                   uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new CharType(klass, descriptor, cache_id);
  }
  return instance_;
}

CharType* CharType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void CharType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

ShortType* ShortType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                     uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new ShortType(klass, descriptor, cache_id);
  }
  return instance_;
}

ShortType* ShortType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void ShortType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

ByteType* ByteType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                   uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new ByteType(klass, descriptor, cache_id);
  }
  return instance_;
}

ByteType* ByteType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void ByteType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

IntegerType* IntegerType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                         uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new IntegerType(klass, descriptor, cache_id);
  }
  return instance_;
}

IntegerType* IntegerType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void IntegerType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

ConflictType* ConflictType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                           uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new ConflictType(klass, descriptor, cache_id);
  }
  return instance_;
}

ConflictType* ConflictType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void ConflictType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

BooleanType* BooleanType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                         uint16_t cache_id) {
  if (BooleanType::instance == NULL) {
    instance = new BooleanType(klass, descriptor, cache_id);
  }
  return BooleanType::instance;
}

BooleanType* BooleanType::GetInstance() {
  CHECK(BooleanType::instance != NULL);
  return BooleanType::instance;
}

void BooleanType::Destroy() {
  if (BooleanType::instance != NULL) {
    delete instance;
    instance = NULL;
  }
}

std::string UndefinedType::Dump() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return "Undefined";
}

UndefinedType* UndefinedType::CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                             uint16_t cache_id) {
  if (instance_ == NULL) {
    instance_ = new UndefinedType(klass, descriptor, cache_id);
  }
  return instance_;
}

UndefinedType* UndefinedType::GetInstance() {
  CHECK(instance_ != NULL);
  return instance_;
}

void UndefinedType::Destroy() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

PreciseReferenceType::PreciseReferenceType(mirror::Class* klass, const std::string& descriptor,
                                           uint16_t cache_id)
    : RegType(klass, descriptor, cache_id) {
  DCHECK(klass->IsInstantiable());
}

std::string UnresolvedMergedType::Dump() const {
  std::stringstream result;
  std::set<uint16_t> types = GetMergedTypes();
  result << "UnresolvedMergedReferences(";
  auto it = types.begin();
  result << reg_type_cache_->GetFromId(*it).Dump();
  for (++it; it != types.end(); ++it) {
    result << ", ";
    result << reg_type_cache_->GetFromId(*it).Dump();
  }
  result << ")";
  return result.str();
}

std::string UnresolvedSuperClass::Dump() const {
  std::stringstream result;
  uint16_t super_type_id = GetUnresolvedSuperClassChildId();
  result << "UnresolvedSuperClass(" << reg_type_cache_->GetFromId(super_type_id).Dump() << ")";
  return result.str();
}

std::string UnresolvedReferenceType::Dump() const {
  std::stringstream result;
  result << "Unresolved Reference" << ": " << PrettyDescriptor(GetDescriptor());
  return result.str();
}

std::string UnresolvedUninitializedRefType::Dump() const {
  std::stringstream result;
  result << "Unresolved And Uninitialized Reference" << ": " << PrettyDescriptor(GetDescriptor());
  result << " Allocation PC: " << GetAllocationPc();
  return result.str();
}

std::string UnresolvedUninitializedThisRefType::Dump() const {
  std::stringstream result;
  result << "Unresolved And Uninitialized This Reference" << PrettyDescriptor(GetDescriptor());
  return result.str();
}

std::string ReferenceType::Dump() const {
  std::stringstream result;
  result << "Reference" << ": " << PrettyDescriptor(GetClass());
  return result.str();
}

std::string PreciseReferenceType::Dump() const {
  std::stringstream result;
  result << "Precise Reference" << ": "<< PrettyDescriptor(GetClass());
  return result.str();
}

std::string UninitializedReferenceType::Dump() const {
  std::stringstream result;
  result << "Uninitialized Reference" << ": " << PrettyDescriptor(GetClass());
  result << " Allocation PC: " << GetAllocationPc();
  return result.str();
}

std::string UninitializedThisReferenceType::Dump() const {
  std::stringstream result;
  result << "Uninitialized This Reference" << ": " << PrettyDescriptor(GetClass());
  result << "Allocation PC: " << GetAllocationPc();
  return result.str();
}

std::string ImpreciseConstType::Dump() const {
  std::stringstream result;
  uint32_t val = ConstantValue();
  if (val == 0) {
    CHECK(IsPreciseConstant());
    result << "Zero/null";
  } else {
    result << "Imprecise ";
    if (IsConstantShort()) {
      result << StringPrintf("Constant: %d", val);
    } else {
      result << StringPrintf("Constant: 0x%x", val);
    }
  }
  return result.str();
}
std::string PreciseConstLoType::Dump() const {
  std::stringstream result;

  int32_t val = ConstantValueLo();
  result << "Precise ";
  if (val >= std::numeric_limits<jshort>::min() &&
      val <= std::numeric_limits<jshort>::max()) {
    result << StringPrintf("Low-half Constant: %d", val);
  } else {
    result << StringPrintf("Low-half Constant: 0x%x", val);
  }
  return result.str();
}

std::string ImpreciseConstLoType::Dump() const {
  std::stringstream result;

  int32_t val = ConstantValueLo();
  result << "Imprecise ";
  if (val >= std::numeric_limits<jshort>::min() &&
      val <= std::numeric_limits<jshort>::max()) {
    result << StringPrintf("Low-half Constant: %d", val);
  } else {
    result << StringPrintf("Low-half Constant: 0x%x", val);
  }
  return result.str();
}

std::string PreciseConstHiType::Dump() const {
  std::stringstream result;
  int32_t val = ConstantValueHi();
  result << "Precise ";
  if (val >= std::numeric_limits<jshort>::min() &&
      val <= std::numeric_limits<jshort>::max()) {
    result << StringPrintf("High-half Constant: %d", val);
  } else {
    result << StringPrintf("High-half Constant: 0x%x", val);
  }
  return result.str();
}

std::string ImpreciseConstHiType::Dump() const {
  std::stringstream result;
  int32_t val = ConstantValueHi();
  result << "Imprecise ";
  if (val >= std::numeric_limits<jshort>::min() &&
      val <= std::numeric_limits<jshort>::max()) {
    result << StringPrintf("High-half Constant: %d", val);
  } else {
    result << StringPrintf("High-half Constant: 0x%x", val);
  }
  return result.str();
}

ConstantType::ConstantType(uint32_t constant, uint16_t cache_id)
    : RegType(NULL, "", cache_id), constant_(constant) {
}

const RegType& UndefinedType::Merge(const RegType& incoming_type, RegTypeCache* reg_types) const
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (incoming_type.IsUndefined()) {
    return *this;  // Undefined MERGE Undefined => Undefined
  }
  return reg_types->Conflict();
}

const RegType& RegType::HighHalf(RegTypeCache* cache) const {
  DCHECK(IsLowHalf());
  if (IsLongLo()) {
    return cache->LongHi();
  } else if (IsDoubleLo()) {
    return cache->DoubleHi();
  } else {
    DCHECK(IsImpreciseConstantLo());
    return cache->FromCat2ConstHi(ConstantValue(), false);
  }
}

Primitive::Type RegType::GetPrimitiveType() const {
  if (IsNonZeroReferenceTypes()) {
    return Primitive::kPrimNot;
  } else if (IsBooleanTypes()) {
    return Primitive::kPrimBoolean;
  } else if (IsByteTypes()) {
    return Primitive::kPrimByte;
  } else if (IsShortTypes()) {
    return Primitive::kPrimShort;
  } else if (IsCharTypes()) {
    return Primitive::kPrimChar;
  } else if (IsFloat()) {
    return Primitive::kPrimFloat;
  } else if (IsIntegralTypes()) {
    return Primitive::kPrimInt;
  } else if (IsDoubleLo()) {
    return Primitive::kPrimDouble;
  } else {
    DCHECK(IsLongTypes());
    return Primitive::kPrimLong;
  }
}

bool UninitializedType::IsUninitializedTypes() const {
  return true;
}

bool UninitializedType::IsNonZeroReferenceTypes() const {
  return true;
}

bool UnresolvedType::IsNonZeroReferenceTypes() const {
  return true;
}
std::set<uint16_t> UnresolvedMergedType::GetMergedTypes() const {
  std::pair<uint16_t, uint16_t> refs = GetTopMergedTypes();
  const RegType& _left(reg_type_cache_->GetFromId(refs.first));
  RegType& __left(const_cast<RegType&>(_left));
  UnresolvedMergedType* left = down_cast<UnresolvedMergedType*>(&__left);

  RegType& _right(
      const_cast<RegType&>(reg_type_cache_->GetFromId(refs.second)));
  UnresolvedMergedType* right = down_cast<UnresolvedMergedType*>(&_right);

  std::set<uint16_t> types;
  if (left->IsUnresolvedMergedReference()) {
    types = left->GetMergedTypes();
  } else {
    types.insert(refs.first);
  }
  if (right->IsUnresolvedMergedReference()) {
    std::set<uint16_t> right_types = right->GetMergedTypes();
    types.insert(right_types.begin(), right_types.end());
  } else {
    types.insert(refs.second);
  }
  if (kIsDebugBuild) {
    for (const auto& type : types) {
      CHECK(!reg_type_cache_->GetFromId(type).IsUnresolvedMergedReference());
    }
  }
  return types;
}

const RegType& RegType::GetSuperClass(RegTypeCache* cache) const {
  if (!IsUnresolvedTypes()) {
    mirror::Class* super_klass = GetClass()->GetSuperClass();
    if (super_klass != NULL) {
      // A super class of a precise type isn't precise as a precise type indicates the register
      // holds exactly that type.
      return cache->FromClass(ClassHelper(super_klass).GetDescriptor(), super_klass, false);
    } else {
      return cache->Zero();
    }
  } else {
    if (!IsUnresolvedMergedReference() && !IsUnresolvedSuperClass() &&
        GetDescriptor()[0] == '[') {
      // Super class of all arrays is Object.
      return cache->JavaLangObject(true);
    } else {
      return cache->FromUnresolvedSuperClass(*this);
    }
  }
}

bool RegType::CanAccess(const RegType& other) const {
  if (Equals(other)) {
    return true;  // Trivial accessibility.
  } else {
    bool this_unresolved = IsUnresolvedTypes();
    bool other_unresolved = other.IsUnresolvedTypes();
    if (!this_unresolved && !other_unresolved) {
      return GetClass()->CanAccess(other.GetClass());
    } else if (!other_unresolved) {
      return other.GetClass()->IsPublic();  // Be conservative, only allow if other is public.
    } else {
      return false;  // More complicated test not possible on unresolved types, be conservative.
    }
  }
}

bool RegType::CanAccessMember(mirror::Class* klass, uint32_t access_flags) const {
  if ((access_flags & kAccPublic) != 0) {
    return true;
  }
  if (!IsUnresolvedTypes()) {
    return GetClass()->CanAccessMember(klass, access_flags);
  } else {
    return false;  // More complicated test not possible on unresolved types, be conservative.
  }
}

bool RegType::IsObjectArrayTypes() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (IsUnresolvedTypes() && !IsUnresolvedMergedReference() && !IsUnresolvedSuperClass()) {
    // Primitive arrays will always resolve
    DCHECK(descriptor_[1] == 'L' || descriptor_[1] == '[');
    return descriptor_[0] == '[';
  } else if (HasClass()) {
    mirror::Class* type = GetClass();
    return type->IsArrayClass() && !type->GetComponentType()->IsPrimitive();
  } else {
    return false;
  }
}

bool RegType::IsJavaLangObject() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return IsReference() && GetClass()->IsObjectClass();
}

bool RegType::IsArrayTypes() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (IsUnresolvedTypes() && !IsUnresolvedMergedReference() && !IsUnresolvedSuperClass()) {
    return descriptor_[0] == '[';
  } else if (HasClass()) {
    return GetClass()->IsArrayClass();
  } else {
    return false;
  }
}

bool RegType::IsJavaLangObjectArray() const {
  if (HasClass()) {
    mirror::Class* type = GetClass();
    return type->IsArrayClass() && type->GetComponentType()->IsObjectClass();
  }
  return false;
}

bool RegType::IsInstantiableTypes() const {
  return IsUnresolvedTypes() || (IsNonZeroReferenceTypes() && GetClass()->IsInstantiable());
}

ImpreciseConstType::ImpreciseConstType(uint32_t constat, uint16_t cache_id)
  : ConstantType(constat, cache_id) {
}

static bool AssignableFrom(const RegType& lhs, const RegType& rhs, bool strict)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (lhs.Equals(rhs)) {
    return true;
  } else {
    if (lhs.IsBoolean()) {
      return rhs.IsBooleanTypes();
    } else if (lhs.IsByte()) {
      return rhs.IsByteTypes();
    } else if (lhs.IsShort()) {
      return rhs.IsShortTypes();
    } else if (lhs.IsChar()) {
      return rhs.IsCharTypes();
    } else if (lhs.IsInteger()) {
      return rhs.IsIntegralTypes();
    } else if (lhs.IsFloat()) {
      return rhs.IsFloatTypes();
    } else if (lhs.IsLongLo()) {
      return rhs.IsLongTypes();
    } else if (lhs.IsDoubleLo()) {
      return rhs.IsDoubleTypes();
    } else {
      CHECK(lhs.IsReferenceTypes())
          << "Unexpected register type in IsAssignableFrom: '"
          << lhs << "' := '" << rhs << "'";
      if (rhs.IsZero()) {
        return true;  // All reference types can be assigned null.
      } else if (!rhs.IsReferenceTypes()) {
        return false;  // Expect rhs to be a reference type.
      } else if (lhs.IsJavaLangObject()) {
        return true;  // All reference types can be assigned to Object.
      } else if (!strict && !lhs.IsUnresolvedTypes() && lhs.GetClass()->IsInterface()) {
        // If we're not strict allow assignment to any interface, see comment in ClassJoin.
        return true;
      } else if (lhs.IsJavaLangObjectArray()) {
        return rhs.IsObjectArrayTypes();  // All reference arrays may be assigned to Object[]
      } else if (lhs.HasClass() && rhs.HasClass() &&
                 lhs.GetClass()->IsAssignableFrom(rhs.GetClass())) {
        // We're assignable from the Class point-of-view.
        return true;
      } else {
        // Unresolved types are only assignable for null and equality.
        return false;
      }
    }
  }
}

bool RegType::IsAssignableFrom(const RegType& src) const {
  return AssignableFrom(*this, src, false);
}

bool RegType::IsStrictlyAssignableFrom(const RegType& src) const {
  return AssignableFrom(*this, src, true);
}

int32_t ConstantType::ConstantValue() const {
  DCHECK(IsConstantTypes());
  return constant_;
}

int32_t ConstantType::ConstantValueLo() const {
  DCHECK(IsConstantLo());
  return constant_;
}

int32_t ConstantType::ConstantValueHi() const {
  if (IsConstantHi() || IsPreciseConstantHi() || IsImpreciseConstantHi()) {
    return constant_;
  } else {
    DCHECK(false);
    return 0;
  }
}

static const RegType& SelectNonConstant(const RegType& a, const RegType& b) {
  return a.IsConstant() ? b : a;
}

const RegType& RegType::Merge(const RegType& incoming_type, RegTypeCache* reg_types) const {
  DCHECK(!Equals(incoming_type));  // Trivial equality handled by caller
  if (IsConflict()) {
    return *this;  // Conflict MERGE * => Conflict
  } else if (incoming_type.IsConflict()) {
    return incoming_type;  // * MERGE Conflict => Conflict
  } else if (IsUndefined() || incoming_type.IsUndefined()) {
    return reg_types->Conflict();  // Unknown MERGE * => Conflict
  } else if (IsConstant() && incoming_type.IsConstant()) {
    int32_t val1 = ConstantValue();
    int32_t val2 = incoming_type.ConstantValue();
    if (val1 >= 0 && val2 >= 0) {
      // +ve1 MERGE +ve2 => MAX(+ve1, +ve2)
      if (val1 >= val2) {
        if (!IsPreciseConstant()) {
          return *this;
        } else {
          return reg_types->FromCat1Const(val1, false);
        }
      } else {
        if (!incoming_type.IsPreciseConstant()) {
          return incoming_type;
        } else {
          return reg_types->FromCat1Const(val2, false);
        }
      }
    } else if (val1 < 0 && val2 < 0) {
      // -ve1 MERGE -ve2 => MIN(-ve1, -ve2)
      if (val1 <= val2) {
        if (!IsPreciseConstant()) {
          return *this;
        } else {
          return reg_types->FromCat1Const(val1, false);
        }
      } else {
        if (!incoming_type.IsPreciseConstant()) {
          return incoming_type;
        } else {
          return reg_types->FromCat1Const(val2, false);
        }
      }
    } else {
      // Values are +ve and -ve, choose smallest signed type in which they both fit
      if (IsConstantByte()) {
        if (incoming_type.IsConstantByte()) {
          return reg_types->ByteConstant();
        } else if (incoming_type.IsConstantShort()) {
          return reg_types->ShortConstant();
        } else {
          return reg_types->IntConstant();
        }
      } else if (IsConstantShort()) {
        if (incoming_type.IsConstantShort()) {
          return reg_types->ShortConstant();
        } else {
          return reg_types->IntConstant();
        }
      } else {
        return reg_types->IntConstant();
      }
    }
  } else if (IsConstantLo() && incoming_type.IsConstantLo()) {
    int32_t val1 = ConstantValueLo();
    int32_t val2 = incoming_type.ConstantValueLo();
    return reg_types->FromCat2ConstLo(val1 | val2, false);
  } else if (IsConstantHi() && incoming_type.IsConstantHi()) {
    int32_t val1 = ConstantValueHi();
    int32_t val2 = incoming_type.ConstantValueHi();
    return reg_types->FromCat2ConstHi(val1 | val2, false);
  } else if (IsIntegralTypes() && incoming_type.IsIntegralTypes()) {
    if (IsBooleanTypes() && incoming_type.IsBooleanTypes()) {
      return reg_types->Boolean();  // boolean MERGE boolean => boolean
    }
    if (IsByteTypes() && incoming_type.IsByteTypes()) {
      return reg_types->Byte();  // byte MERGE byte => byte
    }
    if (IsShortTypes() && incoming_type.IsShortTypes()) {
      return reg_types->Short();  // short MERGE short => short
    }
    if (IsCharTypes() && incoming_type.IsCharTypes()) {
      return reg_types->Char();  // char MERGE char => char
    }
    return reg_types->Integer();  // int MERGE * => int
  } else if ((IsFloatTypes() && incoming_type.IsFloatTypes()) ||
             (IsLongTypes() && incoming_type.IsLongTypes()) ||
             (IsLongHighTypes() && incoming_type.IsLongHighTypes()) ||
             (IsDoubleTypes() && incoming_type.IsDoubleTypes()) ||
             (IsDoubleHighTypes() && incoming_type.IsDoubleHighTypes())) {
    // check constant case was handled prior to entry
    DCHECK(!IsConstant() || !incoming_type.IsConstant());
    // float/long/double MERGE float/long/double_constant => float/long/double
    return SelectNonConstant(*this, incoming_type);
  } else if (IsReferenceTypes() && incoming_type.IsReferenceTypes()) {
    if (IsZero() || incoming_type.IsZero()) {
      return SelectNonConstant(*this, incoming_type);  // 0 MERGE ref => ref
    } else if (IsJavaLangObject() || incoming_type.IsJavaLangObject()) {
      return reg_types->JavaLangObject(false);  // Object MERGE ref => Object
    } else if (IsUnresolvedTypes() || incoming_type.IsUnresolvedTypes()) {
      // We know how to merge an unresolved type with itself, 0 or Object. In this case we
      // have two sub-classes and don't know how to merge. Create a new string-based unresolved
      // type that reflects our lack of knowledge and that allows the rest of the unresolved
      // mechanics to continue.
      return reg_types->FromUnresolvedMerge(*this, incoming_type);
    } else if (IsUninitializedTypes() || incoming_type.IsUninitializedTypes()) {
      // Something that is uninitialized hasn't had its constructor called. Mark any merge
      // of this type with something that is initialized as conflicting. The cases of a merge
      // with itself, 0 or Object are handled above.
      return reg_types->Conflict();
    } else {  // Two reference types, compute Join
      mirror::Class* c1 = GetClass();
      mirror::Class* c2 = incoming_type.GetClass();
      DCHECK(c1 != NULL && !c1->IsPrimitive());
      DCHECK(c2 != NULL && !c2->IsPrimitive());
      mirror::Class* join_class = ClassJoin(c1, c2);
      if (c1 == join_class && !IsPreciseReference()) {
        return *this;
      } else if (c2 == join_class && !incoming_type.IsPreciseReference()) {
        return incoming_type;
      } else {
        return reg_types->FromClass(ClassHelper(join_class).GetDescriptor(), join_class, false);
      }
    }
  } else {
    return reg_types->Conflict();  // Unexpected types => Conflict
  }
}

// See comment in reg_type.h
mirror::Class* RegType::ClassJoin(mirror::Class* s, mirror::Class* t) {
  DCHECK(!s->IsPrimitive()) << PrettyClass(s);
  DCHECK(!t->IsPrimitive()) << PrettyClass(t);
  if (s == t) {
    return s;
  } else if (s->IsAssignableFrom(t)) {
    return s;
  } else if (t->IsAssignableFrom(s)) {
    return t;
  } else if (s->IsArrayClass() && t->IsArrayClass()) {
    mirror::Class* s_ct = s->GetComponentType();
    mirror::Class* t_ct = t->GetComponentType();
    if (s_ct->IsPrimitive() || t_ct->IsPrimitive()) {
      // Given the types aren't the same, if either array is of primitive types then the only
      // common parent is java.lang.Object
      mirror::Class* result = s->GetSuperClass();  // short-cut to java.lang.Object
      DCHECK(result->IsObjectClass());
      return result;
    }
    mirror::Class* common_elem = ClassJoin(s_ct, t_ct);
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    mirror::ClassLoader* class_loader = s->GetClassLoader();
    std::string descriptor("[");
    descriptor += ClassHelper(common_elem).GetDescriptor();
    mirror::Class* array_class = class_linker->FindClass(descriptor.c_str(), class_loader);
    DCHECK(array_class != NULL);
    return array_class;
  } else {
    size_t s_depth = s->Depth();
    size_t t_depth = t->Depth();
    // Get s and t to the same depth in the hierarchy
    if (s_depth > t_depth) {
      while (s_depth > t_depth) {
        s = s->GetSuperClass();
        s_depth--;
      }
    } else {
      while (t_depth > s_depth) {
        t = t->GetSuperClass();
        t_depth--;
      }
    }
    // Go up the hierarchy until we get to the common parent
    while (s != t) {
      s = s->GetSuperClass();
      t = t->GetSuperClass();
    }
    return s;
  }
}

void RegType::CheckInvariants() const {
  if (IsConstant() || IsConstantLo() || IsConstantHi()) {
    CHECK(descriptor_.empty()) << *this;
    CHECK(klass_ == NULL) << *this;
  }
  if (klass_ != NULL) {
    CHECK(!descriptor_.empty()) << *this;
  }
}

void UninitializedThisReferenceType::CheckInvariants() const {
  CHECK_EQ(GetAllocationPc(), 0U) << *this;
}

void UnresolvedUninitializedThisRefType::CheckInvariants() const {
  CHECK_EQ(GetAllocationPc(), 0U) << *this;
  CHECK(!descriptor_.empty()) << *this;
  CHECK(klass_ == NULL) << *this;
}

void UnresolvedUninitializedRefType::CheckInvariants() const {
  CHECK(!descriptor_.empty()) << *this;
  CHECK(klass_ == NULL) << *this;
}

void UnresolvedMergedType::CheckInvariants() const {
  // Unresolved merged types: merged types should be defined.
  CHECK(descriptor_.empty()) << *this;
  CHECK(klass_ == NULL) << *this;
  CHECK_NE(merged_types_.first, 0U) << *this;
  CHECK_NE(merged_types_.second, 0U) << *this;
}

void UnresolvedReferenceType::CheckInvariants() const {
  CHECK(!descriptor_.empty()) << *this;
  CHECK(klass_ == NULL) << *this;
}

void UnresolvedSuperClass::CheckInvariants() const {
  // Unresolved merged types: merged types should be defined.
  CHECK(descriptor_.empty()) << *this;
  CHECK(klass_ == NULL) << *this;
  CHECK_NE(unresolved_child_id_, 0U) << *this;
}

std::ostream& operator<<(std::ostream& os, const RegType& rhs) {
  os << rhs.Dump();
  return os;
}

}  // namespace verifier
}  // namespace art
