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

#include "common_throws.h"
#include "entrypoints/entrypoint_utils.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" int32_t art_portable_is_assignable_from_code(const mirror::Class* dest_type,
                                                        const mirror::Class* src_type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(dest_type != NULL);
  DCHECK(src_type != NULL);
  return dest_type->IsAssignableFrom(src_type) ? 1 : 0;
}

extern "C" void art_portable_check_cast_from_code(const mirror::Class* dest_type,
                                                  const mirror::Class* src_type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(dest_type->IsClass()) << PrettyClass(dest_type);
  DCHECK(src_type->IsClass()) << PrettyClass(src_type);
  if (UNLIKELY(!dest_type->IsAssignableFrom(src_type))) {
    ThrowClassCastException(dest_type, src_type);
  }
}

extern "C" void art_portable_check_put_array_element_from_code(const mirror::Object* element,
                                                               const mirror::Object* array)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (element == NULL) {
    return;
  }
  DCHECK(array != NULL);
  mirror::Class* array_class = array->GetClass();
  DCHECK(array_class != NULL);
  mirror::Class* component_type = array_class->GetComponentType();
  mirror::Class* element_class = element->GetClass();
  if (UNLIKELY(!component_type->IsAssignableFrom(element_class))) {
    ThrowArrayStoreException(element_class, array_class);
  }
}

}  // namespace art
