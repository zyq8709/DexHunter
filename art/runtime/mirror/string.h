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

#ifndef ART_RUNTIME_MIRROR_STRING_H_
#define ART_RUNTIME_MIRROR_STRING_H_

#include "class.h"
#include "gtest/gtest.h"

namespace art {

struct StringClassOffsets;
struct StringOffsets;
class StringPiece;

namespace mirror {

// C++ mirror of java.lang.String
class MANAGED String : public Object {
 public:
  static MemberOffset CountOffset() {
    return OFFSET_OF_OBJECT_MEMBER(String, count_);
  }

  static MemberOffset ValueOffset() {
    return OFFSET_OF_OBJECT_MEMBER(String, array_);
  }

  static MemberOffset OffsetOffset() {
    return OFFSET_OF_OBJECT_MEMBER(String, offset_);
  }

  const CharArray* GetCharArray() const;

  int32_t GetOffset() const {
    int32_t result = GetField32(OffsetOffset(), false);
    DCHECK_LE(0, result);
    return result;
  }

  int32_t GetLength() const;

  int32_t GetHashCode() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void ComputeHashCode() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t GetUtfLength() const;

  uint16_t CharAt(int32_t index) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  String* Intern() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromUtf16(Thread* self,
                                int32_t utf16_length,
                                const uint16_t* utf16_data_in,
                                int32_t hash_code = 0)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromModifiedUtf8(Thread* self, const char* utf)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromModifiedUtf8(Thread* self, int32_t utf16_length,
                                       const char* utf8_data_in)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* Alloc(Thread* self, Class* java_lang_String, int32_t utf16_length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* Alloc(Thread* self, Class* java_lang_String, CharArray* array)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool Equals(const char* modified_utf8) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // TODO: do we need this overload? give it a more intention-revealing name.
  bool Equals(const StringPiece& modified_utf8) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool Equals(const String* that) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Compare UTF-16 code point values not in a locale-sensitive manner
  int Compare(int32_t utf16_length, const char* utf8_data_in);

  // TODO: do we need this overload? give it a more intention-revealing name.
  bool Equals(const uint16_t* that_chars, int32_t that_offset,
              int32_t that_length) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Create a modified UTF-8 encoded std::string from a java/lang/String object.
  std::string ToModifiedUtf8() const;

  int32_t FastIndexOf(int32_t ch, int32_t start) const;

  int32_t CompareTo(String* other) const;

  static Class* GetJavaLangString() {
    DCHECK(java_lang_String_ != NULL);
    return java_lang_String_;
  }

  static void SetClass(Class* java_lang_String);
  static void ResetClass();

 private:
  void SetHashCode(int32_t new_hash_code) {
    DCHECK_EQ(0u,
              GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_), false));
    SetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_),
               new_hash_code, false);
  }

  void SetCount(int32_t new_count) {
    DCHECK_LE(0, new_count);
    SetField32(OFFSET_OF_OBJECT_MEMBER(String, count_), new_count, false);
  }

  void SetOffset(int32_t new_offset) {
    DCHECK_LE(0, new_offset);
    DCHECK_GE(GetLength(), new_offset);
    SetField32(OFFSET_OF_OBJECT_MEMBER(String, offset_), new_offset, false);
  }

  void SetArray(CharArray* new_array) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  CharArray* array_;

  int32_t count_;

  uint32_t hash_code_;

  int32_t offset_;

  static Class* java_lang_String_;

  friend struct art::StringOffsets;  // for verifying offset information
  FRIEND_TEST(ObjectTest, StringLength);  // for SetOffset and SetCount
  DISALLOW_IMPLICIT_CONSTRUCTORS(String);
};

class MANAGED StringClass : public Class {
 private:
  CharArray* ASCII_;
  Object* CASE_INSENSITIVE_ORDER_;
  int64_t serialVersionUID_;
  uint32_t REPLACEMENT_CHAR_;
  friend struct art::StringClassOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringClass);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_H_
