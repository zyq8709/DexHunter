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
#include "dex_file-inl.h"
#include "interpreter/interpreter.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "stack.h"
#include "thread.h"
#include "verifier/method_verifier.h"

namespace art {

extern "C" void artDeoptimize(Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kSaveAll);
  self->SetException(ThrowLocation(), reinterpret_cast<mirror::Throwable*>(-1));
  self->QuickDeliverException();
}

}  // namespace art
