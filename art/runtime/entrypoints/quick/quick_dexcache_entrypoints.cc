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
#include "class_linker-inl.h"
#include "dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" mirror::Class* artInitializeStaticStorageFromCode(uint32_t type_idx,
                                                             const mirror::ArtMethod* referrer,
                                                             Thread* self,
                                                             mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Called to ensure static storage base is initialized for direct static field reads and writes.
  // A class may be accessing another class' fields when it doesn't have access, as access has been
  // given by inheritance.
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return ResolveVerifyAndClinit(type_idx, referrer, self, true, false);
}

extern "C" mirror::Class* artInitializeTypeFromCode(uint32_t type_idx,
                                                    const mirror::ArtMethod* referrer,
                                                    Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Called when method->dex_cache_resolved_types_[] misses.
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return ResolveVerifyAndClinit(type_idx, referrer, self, false, false);
}

extern "C" mirror::Class* artInitializeTypeAndVerifyAccessFromCode(uint32_t type_idx,
                                                                   const mirror::ArtMethod* referrer,
                                                                   Thread* self,
                                                                   mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Called when caller isn't guaranteed to have access to a type and the dex cache may be
  // unpopulated.
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return ResolveVerifyAndClinit(type_idx, referrer, self, false, true);
}

extern "C" mirror::String* artResolveStringFromCode(mirror::ArtMethod* referrer,
                                                    int32_t string_idx,
                                                    Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return ResolveStringFromCode(referrer, string_idx);
}

}  // namespace art
