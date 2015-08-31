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

#ifndef ART_RUNTIME_MIRROR_THROWABLE_H_
#define ART_RUNTIME_MIRROR_THROWABLE_H_

#include "object.h"
#include "string.h"

namespace art {

struct ThrowableOffsets;

namespace mirror {

// C++ mirror of java.lang.Throwable
class MANAGED Throwable : public Object {
 public:
  void SetDetailMessage(String* new_detail_message) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Throwable, detail_message_), new_detail_message, false);
  }
  String* GetDetailMessage() const {
    return GetFieldObject<String*>(OFFSET_OF_OBJECT_MEMBER(Throwable, detail_message_), false);
  }
  std::string Dump() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // This is a runtime version of initCause, you shouldn't use it if initCause may have been
  // overridden. Also it asserts rather than throwing exceptions. Currently this is only used
  // in cases like the verifier where the checks cannot fail and initCause isn't overridden.
  void SetCause(Throwable* cause) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsCheckedException() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static Class* GetJavaLangThrowable() {
    DCHECK(java_lang_Throwable_ != NULL);
    return java_lang_Throwable_;
  }

  static void SetClass(Class* java_lang_Throwable);
  static void ResetClass();

 private:
  Object* GetStackState() const {
    return GetFieldObject<Object*>(OFFSET_OF_OBJECT_MEMBER(Throwable, stack_state_), true);
  }

  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  Throwable* cause_;
  String* detail_message_;
  Object* stack_state_;  // Note this is Java volatile:
  Object* stack_trace_;
  Object* suppressed_exceptions_;

  static Class* java_lang_Throwable_;

  friend struct art::ThrowableOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Throwable);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_THROWABLE_H_
