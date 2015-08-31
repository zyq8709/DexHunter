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
#include "instrumentation.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"
#include "runtime.h"
#include "thread-inl.h"

namespace art {

extern "C" const void* artInstrumentationMethodEntryFromCode(mirror::ArtMethod* method,
                                                             mirror::Object* this_object,
                                                             Thread* self,
                                                             mirror::ArtMethod** sp,
                                                             uintptr_t lr)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsAndArgs);
  instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
  const void* result = instrumentation->GetQuickCodeFor(method);
  bool interpreter_entry = (result == GetQuickToInterpreterBridge());
  instrumentation->PushInstrumentationStackFrame(self, method->IsStatic() ? NULL : this_object,
                                                 method, lr, interpreter_entry);
  CHECK(result != NULL) << PrettyMethod(method);
  return result;
}

extern "C" uint64_t artInstrumentationMethodExitFromCode(Thread* self, mirror::ArtMethod** sp,
                                                         uint64_t gpr_result, uint64_t fpr_result)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // TODO: use FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly) not the hand inlined below.
  //       We use the hand inline version to ensure the return_pc is assigned before verifying the
  //       stack.
  // Be aware the store below may well stomp on an incoming argument.
  Locks::mutator_lock_->AssertSharedHeld(self);
  mirror::ArtMethod* callee_save = Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsOnly);
  *sp = callee_save;
  uintptr_t* return_pc = reinterpret_cast<uintptr_t*>(reinterpret_cast<byte*>(sp) +
                                                      callee_save->GetReturnPcOffsetInBytes());
  CHECK_EQ(*return_pc, 0U);
  self->SetTopOfStack(sp, 0);
  self->VerifyStack();
  instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
  uint64_t return_or_deoptimize_pc = instrumentation->PopInstrumentationStackFrame(self, return_pc,
                                                                                   gpr_result,
                                                                                   fpr_result);
  self->VerifyStack();
  return return_or_deoptimize_pc;
}

}  // namespace art
