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

#include "string.h"

#include "array.h"
#include "gc/accounting/card_table-inl.h"
#include "intern_table.h"
#include "object-inl.h"
#include "runtime.h"
#include "sirt_ref.h"
#include "thread.h"
#include "utf.h"

namespace art {
namespace mirror {

const CharArray* String::GetCharArray() const {
  return GetFieldObject<const CharArray*>(ValueOffset(), false);
}

void String::ComputeHashCode() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  SetHashCode(ComputeUtf16Hash(GetCharArray(), GetOffset(), GetLength()));
}

int32_t String::GetUtfLength() const {
  return CountUtf8Bytes(GetCharArray()->GetData() + GetOffset(), GetLength());
}

int32_t String::FastIndexOf(int32_t ch, int32_t start) const {
  int32_t count = GetLength();
  if (start < 0) {
    start = 0;
  } else if (start > count) {
    start = count;
  }
  const uint16_t* chars = GetCharArray()->GetData() + GetOffset();
  const uint16_t* p = chars + start;
  const uint16_t* end = chars + count;
  while (p < end) {
    if (*p++ == ch) {
      return (p - 1) - chars;
    }
  }
  return -1;
}

void String::SetArray(CharArray* new_array) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(new_array != NULL);
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(String, array_), new_array, false);
}

// TODO: get global references for these
Class* String::java_lang_String_ = NULL;

void String::SetClass(Class* java_lang_String) {
  CHECK(java_lang_String_ == NULL);
  CHECK(java_lang_String != NULL);
  java_lang_String_ = java_lang_String;
}

void String::ResetClass() {
  CHECK(java_lang_String_ != NULL);
  java_lang_String_ = NULL;
}

String* String::Intern() {
  return Runtime::Current()->GetInternTable()->InternWeak(this);
}

int32_t String::GetHashCode() {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_), false);
  if (result == 0) {
    ComputeHashCode();
  }
  result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_), false);
  DCHECK(result != 0 || ComputeUtf16Hash(GetCharArray(), GetOffset(), GetLength()) == 0)
          << ToModifiedUtf8() << " " << result;
  return result;
}

int32_t String::GetLength() const {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, count_), false);
  DCHECK(result >= 0 && result <= GetCharArray()->GetLength());
  return result;
}

uint16_t String::CharAt(int32_t index) const {
  // TODO: do we need this? Equals is the only caller, and could
  // bounds check itself.
  DCHECK_GE(count_, 0);  // ensures the unsigned comparison is safe.
  if (UNLIKELY(static_cast<uint32_t>(index) >= static_cast<uint32_t>(count_))) {
    Thread* self = Thread::Current();
    ThrowLocation throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewExceptionF(throw_location, "Ljava/lang/StringIndexOutOfBoundsException;",
                             "length=%i; index=%i", count_, index);
    return 0;
  }
  return GetCharArray()->Get(index + GetOffset());
}

String* String::AllocFromUtf16(Thread* self,
                               int32_t utf16_length,
                               const uint16_t* utf16_data_in,
                               int32_t hash_code) {
  CHECK(utf16_data_in != NULL || utf16_length == 0);
  String* string = Alloc(self, GetJavaLangString(), utf16_length);
  if (string == NULL) {
    return NULL;
  }
  // TODO: use 16-bit wide memset variant
  CharArray* array = const_cast<CharArray*>(string->GetCharArray());
  if (array == NULL) {
    return NULL;
  }
  for (int i = 0; i < utf16_length; i++) {
    array->Set(i, utf16_data_in[i]);
  }
  if (hash_code != 0) {
    string->SetHashCode(hash_code);
  } else {
    string->ComputeHashCode();
  }
  return string;
}

String* String::AllocFromModifiedUtf8(Thread* self, const char* utf) {
  if (utf == NULL) {
    return NULL;
  }
  size_t char_count = CountModifiedUtf8Chars(utf);
  return AllocFromModifiedUtf8(self, char_count, utf);
}

String* String::AllocFromModifiedUtf8(Thread* self, int32_t utf16_length,
                                      const char* utf8_data_in) {
  String* string = Alloc(self, GetJavaLangString(), utf16_length);
  if (string == NULL) {
    return NULL;
  }
  uint16_t* utf16_data_out =
      const_cast<uint16_t*>(string->GetCharArray()->GetData());
  ConvertModifiedUtf8ToUtf16(utf16_data_out, utf8_data_in);
  string->ComputeHashCode();
  return string;
}

String* String::Alloc(Thread* self, Class* java_lang_String, int32_t utf16_length) {
  SirtRef<CharArray> array(self, CharArray::Alloc(self, utf16_length));
  if (array.get() == NULL) {
    return NULL;
  }
  return Alloc(self, java_lang_String, array.get());
}

String* String::Alloc(Thread* self, Class* java_lang_String, CharArray* array) {
  // Hold reference in case AllocObject causes GC.
  SirtRef<CharArray> array_ref(self, array);
  String* string = down_cast<String*>(java_lang_String->AllocObject(self));
  if (string == NULL) {
    return NULL;
  }
  string->SetArray(array);
  string->SetCount(array->GetLength());
  return string;
}

bool String::Equals(const String* that) const {
  if (this == that) {
    // Quick reference equality test
    return true;
  } else if (that == NULL) {
    // Null isn't an instanceof anything
    return false;
  } else if (this->GetLength() != that->GetLength()) {
    // Quick length inequality test
    return false;
  } else {
    // Note: don't short circuit on hash code as we're presumably here as the
    // hash code was already equal
    for (int32_t i = 0; i < that->GetLength(); ++i) {
      if (this->CharAt(i) != that->CharAt(i)) {
        return false;
      }
    }
    return true;
  }
}

bool String::Equals(const uint16_t* that_chars, int32_t that_offset, int32_t that_length) const {
  if (this->GetLength() != that_length) {
    return false;
  } else {
    for (int32_t i = 0; i < that_length; ++i) {
      if (this->CharAt(i) != that_chars[that_offset + i]) {
        return false;
      }
    }
    return true;
  }
}

bool String::Equals(const char* modified_utf8) const {
  for (int32_t i = 0; i < GetLength(); ++i) {
    uint16_t ch = GetUtf16FromUtf8(&modified_utf8);
    if (ch == '\0' || ch != CharAt(i)) {
      return false;
    }
  }
  return *modified_utf8 == '\0';
}

bool String::Equals(const StringPiece& modified_utf8) const {
  const char* p = modified_utf8.data();
  for (int32_t i = 0; i < GetLength(); ++i) {
    uint16_t ch = GetUtf16FromUtf8(&p);
    if (ch != CharAt(i)) {
      return false;
    }
  }
  return true;
}

// Create a modified UTF-8 encoded std::string from a java/lang/String object.
std::string String::ToModifiedUtf8() const {
  const uint16_t* chars = GetCharArray()->GetData() + GetOffset();
  size_t byte_count = GetUtfLength();
  std::string result(byte_count, static_cast<char>(0));
  ConvertUtf16ToModifiedUtf8(&result[0], chars, GetLength());
  return result;
}

#ifdef HAVE__MEMCMP16
// "count" is in 16-bit units.
extern "C" uint32_t __memcmp16(const uint16_t* s0, const uint16_t* s1, size_t count);
#define MemCmp16 __memcmp16
#else
static uint32_t MemCmp16(const uint16_t* s0, const uint16_t* s1, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (s0[i] != s1[i]) {
      return static_cast<int32_t>(s0[i]) - static_cast<int32_t>(s1[i]);
    }
  }
  return 0;
}
#endif

int32_t String::CompareTo(String* rhs) const {
  // Quick test for comparison of a string with itself.
  const String* lhs = this;
  if (lhs == rhs) {
    return 0;
  }
  // TODO: is this still true?
  // The annoying part here is that 0x00e9 - 0xffff != 0x00ea,
  // because the interpreter converts the characters to 32-bit integers
  // *without* sign extension before it subtracts them (which makes some
  // sense since "char" is unsigned).  So what we get is the result of
  // 0x000000e9 - 0x0000ffff, which is 0xffff00ea.
  int lhsCount = lhs->GetLength();
  int rhsCount = rhs->GetLength();
  int countDiff = lhsCount - rhsCount;
  int minCount = (countDiff < 0) ? lhsCount : rhsCount;
  const uint16_t* lhsChars = lhs->GetCharArray()->GetData() + lhs->GetOffset();
  const uint16_t* rhsChars = rhs->GetCharArray()->GetData() + rhs->GetOffset();
  int otherRes = MemCmp16(lhsChars, rhsChars, minCount);
  if (otherRes != 0) {
    return otherRes;
  }
  return countDiff;
}

}  // namespace mirror
}  // namespace art

