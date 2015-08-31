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
#include "entrypoints/entrypoint_utils.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"

namespace art {

// Assignable test for code, won't throw.  Null and equality tests already performed
extern "C" uint32_t artIsAssignableFromCode(const mirror::Class* klass,
                                            const mirror::Class* ref_class)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(klass != NULL);
  DCHECK(ref_class != NULL);
  return klass->IsAssignableFrom(ref_class) ? 1 : 0;
}

// Check whether it is safe to cast one class to the other, throw exception and return -1 on failure
extern "C" int artCheckCastFromCode(mirror::Class* src_type, mirror::Class* dest_type,
                                    Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(src_type->IsClass()) << PrettyClass(src_type);
  DCHECK(dest_type->IsClass()) << PrettyClass(dest_type);
  if (LIKELY(dest_type->IsAssignableFrom(src_type))) {
    return 0;  // Success
  } else {
    FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
    ThrowClassCastException(dest_type, src_type);
    return -1;  // Failure
  }
}

// Tests whether 'element' can be assigned into an array of type 'array_class'.
// Returns 0 on success and -1 if an exception is pending.
extern "C" int artCanPutArrayElementFromCode(const mirror::Object* element,
                                             const mirror::Class* array_class,
                                             Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(array_class != NULL);
  // element can't be NULL as we catch this is screened in runtime_support
  mirror::Class* element_class = element->GetClass();
  mirror::Class* component_type = array_class->GetComponentType();
  if (LIKELY(component_type->IsAssignableFrom(element_class))) {
    return 0;  // Success
  } else {
    FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
    ThrowArrayStoreException(element_class, array_class);
    return -1;  // Failure
  }
}

}  // namespace art
