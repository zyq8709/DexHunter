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

#include "throwable.h"

#include "art_method-inl.h"
#include "class-inl.h"
#include "dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "object-inl.h"
#include "object_array.h"
#include "object_array-inl.h"
#include "object_utils.h"
#include "utils.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

Class* Throwable::java_lang_Throwable_ = NULL;

void Throwable::SetCause(Throwable* cause) {
  CHECK(cause != NULL);
  CHECK(cause != this);
  Throwable* current_cause = GetFieldObject<Throwable*>(OFFSET_OF_OBJECT_MEMBER(Throwable, cause_),
                                                        false);
  CHECK(current_cause == NULL || current_cause == this);
  SetFieldObject(OFFSET_OF_OBJECT_MEMBER(Throwable, cause_), cause, false);
}

bool Throwable::IsCheckedException() const {
  if (InstanceOf(WellKnownClasses::ToClass(WellKnownClasses::java_lang_Error))) {
    return false;
  }
  return !InstanceOf(WellKnownClasses::ToClass(WellKnownClasses::java_lang_RuntimeException));
}

std::string Throwable::Dump() const {
  std::string result(PrettyTypeOf(this));
  result += ": ";
  String* msg = GetDetailMessage();
  if (msg != NULL) {
    result += msg->ToModifiedUtf8();
  }
  result += "\n";
  Object* stack_state = GetStackState();
  // check stack state isn't missing or corrupt
  if (stack_state != NULL && stack_state->IsObjectArray()) {
    // Decode the internal stack trace into the depth and method trace
    ObjectArray<Object>* method_trace = down_cast<ObjectArray<Object>*>(stack_state);
    int32_t depth = method_trace->GetLength() - 1;
    IntArray* pc_trace = down_cast<IntArray*>(method_trace->Get(depth));
    MethodHelper mh;
    for (int32_t i = 0; i < depth; ++i) {
      ArtMethod* method = down_cast<ArtMethod*>(method_trace->Get(i));
      mh.ChangeMethod(method);
      uint32_t dex_pc = pc_trace->Get(i);
      int32_t line_number = mh.GetLineNumFromDexPC(dex_pc);
      const char* source_file = mh.GetDeclaringClassSourceFile();
      result += StringPrintf("  at %s (%s:%d)\n", PrettyMethod(method, true).c_str(),
                             source_file, line_number);
    }
  }
  Throwable* cause = GetFieldObject<Throwable*>(OFFSET_OF_OBJECT_MEMBER(Throwable, cause_), false);
  if (cause != NULL && cause != this) {  // Constructor makes cause == this by default.
    result += "Caused by: ";
    result += cause->Dump();
  }
  return result;
}

void Throwable::SetClass(Class* java_lang_Throwable) {
  CHECK(java_lang_Throwable_ == NULL);
  CHECK(java_lang_Throwable != NULL);
  java_lang_Throwable_ = java_lang_Throwable;
}

void Throwable::ResetClass() {
  CHECK(java_lang_Throwable_ != NULL);
  java_lang_Throwable_ = NULL;
}

}  // namespace mirror
}  // namespace art
