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
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" mirror::Object* artAllocObjectFromCode(uint32_t type_idx, mirror::ArtMethod* method,
                                                  Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return AllocObjectFromCode(type_idx, method, self, false);
}

extern "C" mirror::Object* artAllocObjectFromCodeWithAccessCheck(uint32_t type_idx,
                                                                 mirror::ArtMethod* method,
                                                                 Thread* self,
                                                                 mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return AllocObjectFromCode(type_idx, method, self, true);
}

extern "C" mirror::Array* artAllocArrayFromCode(uint32_t type_idx, mirror::ArtMethod* method,
                                                int32_t component_count, Thread* self,
                                                mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return AllocArrayFromCode(type_idx, method, component_count, self, false);
}

extern "C" mirror::Array* artAllocArrayFromCodeWithAccessCheck(uint32_t type_idx,
                                                               mirror::ArtMethod* method,
                                                               int32_t component_count,
                                                               Thread* self,
                                                               mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return AllocArrayFromCode(type_idx, method, component_count, self, true);
}

extern "C" mirror::Array* artCheckAndAllocArrayFromCode(uint32_t type_idx,
                                                        mirror::ArtMethod* method,
                                                        int32_t component_count, Thread* self,
                                                        mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return CheckAndAllocArrayFromCode(type_idx, method, component_count, self, false);
}

extern "C" mirror::Array* artCheckAndAllocArrayFromCodeWithAccessCheck(uint32_t type_idx,
                                                                       mirror::ArtMethod* method,
                                                                       int32_t component_count,
                                                                       Thread* self,
                                                                       mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly);
  return CheckAndAllocArrayFromCode(type_idx, method, component_count, self, true);
}

}  // namespace art
