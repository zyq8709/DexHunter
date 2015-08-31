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

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_

#include "base/mutex.h"
#include "thread-inl.h"

namespace art {
namespace mirror {
class ArtMethod;
}  // namespace mirror

// Place a special frame at the TOS that will save the callee saves for the given type.
static void FinishCalleeSaveFrameSetup(Thread* self, mirror::ArtMethod** sp,
                                       Runtime::CalleeSaveType type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Be aware the store below may well stomp on an incoming argument.
  Locks::mutator_lock_->AssertSharedHeld(self);
  *sp = Runtime::Current()->GetCalleeSaveMethod(type);
  self->SetTopOfStack(sp, 0);
  self->VerifyStack();
}

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_
