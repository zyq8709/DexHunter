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
#include "thread.h"
#include "thread_list.h"

namespace art {

void CheckSuspendFromCode(Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Called when thread->suspend_count_ != 0 on JNI return. JNI method acts as callee-save frame.
  thread->VerifyStack();
  CheckSuspend(thread);
}

extern "C" void artTestSuspendFromCode(Thread* thread, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Called when suspend count check value is 0 and thread->suspend_count_ != 0
  FinishCalleeSaveFrameSetup(thread, sp, Runtime::kRefsOnly);
  CheckSuspend(thread);
}

}  // namespace art
