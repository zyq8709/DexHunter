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

#include "art_field.h"

#include "art_field-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "object-inl.h"
#include "object_utils.h"
#include "runtime.h"
#include "utils.h"

namespace art {
namespace mirror {

// TODO: get global references for these
Class* ArtField::java_lang_reflect_ArtField_ = NULL;

void ArtField::SetClass(Class* java_lang_reflect_ArtField) {
  CHECK(java_lang_reflect_ArtField_ == NULL);
  CHECK(java_lang_reflect_ArtField != NULL);
  java_lang_reflect_ArtField_ = java_lang_reflect_ArtField;
}

void ArtField::ResetClass() {
  CHECK(java_lang_reflect_ArtField_ != NULL);
  java_lang_reflect_ArtField_ = NULL;
}

void ArtField::SetOffset(MemberOffset num_bytes) {
  DCHECK(GetDeclaringClass()->IsLoaded() || GetDeclaringClass()->IsErroneous());
#if 0  // TODO enable later in boot and under !NDEBUG
  FieldHelper fh(this);
  Primitive::Type type = fh.GetTypeAsPrimitiveType();
  if (type == Primitive::kPrimDouble || type == Primitive::kPrimLong) {
    DCHECK_ALIGNED(num_bytes.Uint32Value(), 8);
  }
#endif
  SetField32(OFFSET_OF_OBJECT_MEMBER(ArtField, offset_), num_bytes.Uint32Value(), false);
}

}  // namespace mirror
}  // namespace art
