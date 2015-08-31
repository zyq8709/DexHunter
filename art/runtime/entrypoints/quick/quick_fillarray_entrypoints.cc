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

#include "callee_save_frame.h"
#include "common_throws.h"
#include "dex_instruction.h"
#include "mirror/array.h"
#include "mirror/object-inl.h"

namespace art {

/*
 * Fill the array with predefined constant values, throwing exceptions if the array is null or
 * not of sufficient length.
 *
 * NOTE: When dealing with a raw dex file, the data to be copied uses
 * little-endian ordering.  Require that oat2dex do any required swapping
 * so this routine can get by with a memcpy().
 *
 * Format of the data:
 *  ushort ident = 0x0300   magic value
 *  ushort width            width of each element in the table
 *  uint   size             number of elements in the table
 *  ubyte  data[size*width] table of data values (may contain a single-byte
 *                          padding at the end)
 */
extern "C" int artHandleFillArrayDataFromCode(mirror::Array* array,
                                              const Instruction::ArrayDataPayload* payload,
                                              Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  DCHECK_EQ(payload->ident, static_cast<uint16_t>(Instruction::kArrayDataSignature));
  if (UNLIKELY(array == NULL)) {
    ThrowNullPointerException(NULL, "null array in FILL_ARRAY_DATA");
    return -1;  // Error
  }
  DCHECK(array->IsArrayInstance() && !array->IsObjectArray());
  if (UNLIKELY(static_cast<int32_t>(payload->element_count) > array->GetLength())) {
    ThrowLocation throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewExceptionF(throw_location, "Ljava/lang/ArrayIndexOutOfBoundsException;",
                             "failed FILL_ARRAY_DATA; length=%d, index=%d",
                             array->GetLength(), payload->element_count - 1);
    return -1;  // Error
  }
  uint32_t size_in_bytes = payload->element_count * payload->element_width;
  memcpy(array->GetRawData(payload->element_width), payload->data, size_in_bytes);
  return 0;  // Success
}

}  // namespace art
