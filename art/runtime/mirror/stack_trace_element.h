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

#ifndef ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_H_
#define ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_H_

#include "object.h"

namespace art {

struct StackTraceElementOffsets;

namespace mirror {

// C++ mirror of java.lang.StackTraceElement
class MANAGED StackTraceElement : public Object {
 public:
  const String* GetDeclaringClass() const {
    return GetFieldObject<const String*>(
        OFFSET_OF_OBJECT_MEMBER(StackTraceElement, declaring_class_), false);
  }

  const String* GetMethodName() const {
    return GetFieldObject<const String*>(
        OFFSET_OF_OBJECT_MEMBER(StackTraceElement, method_name_), false);
  }

  const String* GetFileName() const {
    return GetFieldObject<const String*>(
        OFFSET_OF_OBJECT_MEMBER(StackTraceElement, file_name_), false);
  }

  int32_t GetLineNumber() const {
    return GetField32(
        OFFSET_OF_OBJECT_MEMBER(StackTraceElement, line_number_), false);
  }

  static StackTraceElement* Alloc(Thread* self,
                                  String* declaring_class,
                                  String* method_name,
                                  String* file_name,
                                  int32_t line_number)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void SetClass(Class* java_lang_StackTraceElement);

  static void ResetClass();

 private:
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  String* declaring_class_;
  String* file_name_;
  String* method_name_;
  int32_t line_number_;

  static Class* GetStackTraceElement() {
    DCHECK(java_lang_StackTraceElement_ != NULL);
    return java_lang_StackTraceElement_;
  }

  static Class* java_lang_StackTraceElement_;

  friend struct art::StackTraceElementOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(StackTraceElement);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_H_
