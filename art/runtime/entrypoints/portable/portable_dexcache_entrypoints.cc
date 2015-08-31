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
#include "gc/accounting/card_table-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" mirror::Object* art_portable_initialize_static_storage_from_code(uint32_t type_idx,
                                                                            mirror::ArtMethod* referrer,
                                                                            Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return ResolveVerifyAndClinit(type_idx, referrer, thread, true, false);
}

extern "C" mirror::Object* art_portable_initialize_type_from_code(uint32_t type_idx,
                                                                  mirror::ArtMethod* referrer,
                                                                  Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return ResolveVerifyAndClinit(type_idx, referrer, thread, false, false);
}

extern "C" mirror::Object* art_portable_initialize_type_and_verify_access_from_code(uint32_t type_idx,
                                                                                    mirror::ArtMethod* referrer,
                                                                                    Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Called when caller isn't guaranteed to have access to a type and the dex cache may be
  // unpopulated
  return ResolveVerifyAndClinit(type_idx, referrer, thread, false, true);
}

extern "C" mirror::Object* art_portable_resolve_string_from_code(mirror::ArtMethod* referrer,
                                                                 uint32_t string_idx)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return ResolveStringFromCode(referrer, string_idx);
}

}  // namespace art
