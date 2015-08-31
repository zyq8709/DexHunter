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

#include "dex_instruction.h"
#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" void art_portable_fill_array_data_from_code(mirror::ArtMethod* method,
                                                       uint32_t dex_pc,
                                                       mirror::Array* array,
                                                       uint32_t payload_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  const DexFile::CodeItem* code_item = MethodHelper(method).GetCodeItem();
  const Instruction::ArrayDataPayload* payload =
      reinterpret_cast<const Instruction::ArrayDataPayload*>(code_item->insns_ + payload_offset);
  DCHECK_EQ(payload->ident, static_cast<uint16_t>(Instruction::kArrayDataSignature));
  if (UNLIKELY(array == NULL)) {
    ThrowNullPointerException(NULL, "null array in FILL_ARRAY_DATA");
    return;  // Error
  }
  DCHECK(array->IsArrayInstance() && !array->IsObjectArray());
  if (UNLIKELY(static_cast<int32_t>(payload->element_count) > array->GetLength())) {
    Thread* self = Thread::Current();
    ThrowLocation throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewExceptionF(throw_location, "Ljava/lang/ArrayIndexOutOfBoundsException;",
                             "failed FILL_ARRAY_DATA; length=%d, index=%d",
                             array->GetLength(), payload->element_count - 1);
    return;  // Error
  }
  uint32_t size_in_bytes = payload->element_count * payload->element_width;
  memcpy(array->GetRawData(payload->element_width), payload->data, size_in_bytes);
}

}  // namespace art
