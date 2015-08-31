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

#ifndef ART_RUNTIME_MIRROR_ART_FIELD_H_
#define ART_RUNTIME_MIRROR_ART_FIELD_H_

#include "class.h"
#include "modifiers.h"
#include "object.h"

namespace art {

struct ArtFieldOffsets;

namespace mirror {

// C++ mirror of java.lang.reflect.ArtField
class MANAGED ArtField : public Object {
 public:
  Class* GetDeclaringClass() const;

  void SetDeclaringClass(Class *new_declaring_class) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uint32_t GetAccessFlags() const;

  void SetAccessFlags(uint32_t new_access_flags) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtField, access_flags_), new_access_flags, false);
  }

  bool IsPublic() const {
    return (GetAccessFlags() & kAccPublic) != 0;
  }

  bool IsStatic() const {
    return (GetAccessFlags() & kAccStatic) != 0;
  }

  bool IsFinal() const {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  uint32_t GetDexFieldIndex() const {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(ArtField, field_dex_idx_), false);
  }

  void SetDexFieldIndex(uint32_t new_idx) {
    SetField32(OFFSET_OF_OBJECT_MEMBER(ArtField, field_dex_idx_), new_idx, false);
  }

  // Offset to field within an Object
  MemberOffset GetOffset() const;

  static MemberOffset OffsetOffset() {
    return MemberOffset(OFFSETOF_MEMBER(ArtField, offset_));
  }

  MemberOffset GetOffsetDuringLinking() const;

  void SetOffset(MemberOffset num_bytes);

  // field access, null object for static fields
  bool GetBoolean(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetBoolean(Object* object, bool z) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  int8_t GetByte(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetByte(Object* object, int8_t b) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  uint16_t GetChar(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetChar(Object* object, uint16_t c) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  int16_t GetShort(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetShort(Object* object, int16_t s) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  int32_t GetInt(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetInt(Object* object, int32_t i) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  int64_t GetLong(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetLong(Object* object, int64_t j) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  float GetFloat(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetFloat(Object* object, float f) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  double GetDouble(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetDouble(Object* object, double d) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  Object* GetObject(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetObject(Object* object, const Object* l) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // raw field accesses
  uint32_t Get32(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Set32(Object* object, uint32_t new_value) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  uint64_t Get64(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Set64(Object* object, uint64_t new_value) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  Object* GetObj(const Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetObj(Object* object, const Object* new_value) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static Class* GetJavaLangReflectArtField() {
    DCHECK(java_lang_reflect_ArtField_ != NULL);
    return java_lang_reflect_ArtField_;
  }

  static void SetClass(Class* java_lang_reflect_ArtField);
  static void ResetClass();

  bool IsVolatile() const {
    return (GetAccessFlags() & kAccVolatile) != 0;
  }

 private:
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  // The class we are a part of
  Class* declaring_class_;

  uint32_t access_flags_;

  // Dex cache index of field id
  uint32_t field_dex_idx_;

  // Offset of field within an instance or in the Class' static fields
  uint32_t offset_;

  static Class* java_lang_reflect_ArtField_;

  friend struct art::ArtFieldOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArtField);
};

class MANAGED ArtFieldClass : public Class {
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArtFieldClass);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ART_FIELD_H_
