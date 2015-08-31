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

#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" mirror::Object* art_portable_alloc_object_from_code(uint32_t type_idx,
                                                               mirror::ArtMethod* referrer,
                                                               Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return AllocObjectFromCode(type_idx, referrer, thread, false);
}

extern "C" mirror::Object* art_portable_alloc_object_from_code_with_access_check(uint32_t type_idx,
                                                                                 mirror::ArtMethod* referrer,
                                                                                 Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return AllocObjectFromCode(type_idx, referrer, thread, true);
}

extern "C" mirror::Object* art_portable_alloc_array_from_code(uint32_t type_idx,
                                                              mirror::ArtMethod* referrer,
                                                              uint32_t length,
                                                              Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return AllocArrayFromCode(type_idx, referrer, length, self, false);
}

extern "C" mirror::Object* art_portable_alloc_array_from_code_with_access_check(uint32_t type_idx,
                                                                                mirror::ArtMethod* referrer,
                                                                                uint32_t length,
                                                                                Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return AllocArrayFromCode(type_idx, referrer, length, self, true);
}

extern "C" mirror::Object* art_portable_check_and_alloc_array_from_code(uint32_t type_idx,
                                                                        mirror::ArtMethod* referrer,
                                                                        uint32_t length,
                                                                        Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return CheckAndAllocArrayFromCode(type_idx, referrer, length, thread, false);
}

extern "C" mirror::Object* art_portable_check_and_alloc_array_from_code_with_access_check(uint32_t type_idx,
                                                                                          mirror::ArtMethod* referrer,
                                                                                          uint32_t length,
                                                                                          Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return CheckAndAllocArrayFromCode(type_idx, referrer, length, thread, true);
}

}  // namespace art
