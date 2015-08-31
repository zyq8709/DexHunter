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
#include "mirror/object.h"
#include "object_utils.h"
#include "thread.h"
#include "well_known_classes.h"

namespace art {

// Deliver an exception that's pending on thread helping set up a callee save frame on the way.
extern "C" void artDeliverPendingExceptionFromCode(Thread* thread, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(thread, sp, Runtime::kSaveAll);
  thread->QuickDeliverException();
}

// Called by generated call to throw an exception.
extern "C" void artDeliverExceptionFromCode(mirror::Throwable* exception, Thread* self,
                                            mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  /*
   * exception may be NULL, in which case this routine should
   * throw NPE.  NOTE: this is a convenience for generated code,
   * which previously did the null check inline and constructed
   * and threw a NPE if NULL.  This routine responsible for setting
   * exception_ in thread and delivering the exception.
   */
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kSaveAll);
  ThrowLocation throw_location = self->GetCurrentLocationForThrow();
  if (exception == NULL) {
    self->ThrowNewException(throw_location, "Ljava/lang/NullPointerException;",
                            "throw with null exception");
  } else {
    self->SetException(throw_location, exception);
  }
  self->QuickDeliverException();
}

// Called by generated call to throw a NPE exception.
extern "C" void artThrowNullPointerExceptionFromCode(Thread* self,
                                                     mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kSaveAll);
  ThrowLocation throw_location = self->GetCurrentLocationForThrow();
  ThrowNullPointerExceptionFromDexPC(throw_location);
  self->QuickDeliverException();
}

// Called by generated call to throw an arithmetic divide by zero exception.
extern "C" void artThrowDivZeroFromCode(Thread* self,
                                        mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kSaveAll);
  ThrowArithmeticExceptionDivideByZero();
  self->QuickDeliverException();
}

// Called by generated call to throw an array index out of bounds exception.
extern "C" void artThrowArrayBoundsFromCode(int index, int length, Thread* self,
                                            mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kSaveAll);
  ThrowArrayIndexOutOfBoundsException(index, length);
  self->QuickDeliverException();
}

extern "C" void artThrowStackOverflowFromCode(Thread* self, mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kSaveAll);
  ThrowStackOverflowError(self);
  self->QuickDeliverException();
}

extern "C" void artThrowNoSuchMethodFromCode(int32_t method_idx, Thread* self,
                                             mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kSaveAll);
  ThrowNoSuchMethodError(method_idx);
  self->QuickDeliverException();
}

}  // namespace art
