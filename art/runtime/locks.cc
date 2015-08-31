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

#include "locks.h"

#include "base/mutex.h"

namespace art {

Mutex* Locks::abort_lock_ = NULL;
Mutex* Locks::breakpoint_lock_ = NULL;
ReaderWriterMutex* Locks::classlinker_classes_lock_ = NULL;
ReaderWriterMutex* Locks::heap_bitmap_lock_ = NULL;
Mutex* Locks::logging_lock_ = NULL;
ReaderWriterMutex* Locks::mutator_lock_ = NULL;
Mutex* Locks::runtime_shutdown_lock_ = NULL;
Mutex* Locks::thread_list_lock_ = NULL;
Mutex* Locks::thread_suspend_count_lock_ = NULL;
Mutex* Locks::trace_lock_ = NULL;
Mutex* Locks::unexpected_signal_lock_ = NULL;

void Locks::Init() {
  if (logging_lock_ != NULL) {
    // Already initialized.
    DCHECK(abort_lock_ != NULL);
    DCHECK(breakpoint_lock_ != NULL);
    DCHECK(classlinker_classes_lock_ != NULL);
    DCHECK(heap_bitmap_lock_ != NULL);
    DCHECK(logging_lock_ != NULL);
    DCHECK(mutator_lock_ != NULL);
    DCHECK(thread_list_lock_ != NULL);
    DCHECK(thread_suspend_count_lock_ != NULL);
    DCHECK(trace_lock_ != NULL);
    DCHECK(unexpected_signal_lock_ != NULL);
  } else {
    logging_lock_ = new Mutex("logging lock", kLoggingLock, true);
    abort_lock_ = new Mutex("abort lock", kAbortLock, true);

    DCHECK(breakpoint_lock_ == NULL);
    breakpoint_lock_ = new Mutex("breakpoint lock", kBreakpointLock);
    DCHECK(classlinker_classes_lock_ == NULL);
    classlinker_classes_lock_ = new ReaderWriterMutex("ClassLinker classes lock",
                                                      kClassLinkerClassesLock);
    DCHECK(heap_bitmap_lock_ == NULL);
    heap_bitmap_lock_ = new ReaderWriterMutex("heap bitmap lock", kHeapBitmapLock);
    DCHECK(mutator_lock_ == NULL);
    mutator_lock_ = new ReaderWriterMutex("mutator lock", kMutatorLock);
    DCHECK(runtime_shutdown_lock_ == NULL);
    runtime_shutdown_lock_ = new Mutex("runtime shutdown lock", kRuntimeShutdownLock);
    DCHECK(thread_list_lock_ == NULL);
    thread_list_lock_ = new Mutex("thread list lock", kThreadListLock);
    DCHECK(thread_suspend_count_lock_ == NULL);
    thread_suspend_count_lock_ = new Mutex("thread suspend count lock", kThreadSuspendCountLock);
    DCHECK(trace_lock_ == NULL);
    trace_lock_ = new Mutex("trace lock", kTraceLock);
    DCHECK(unexpected_signal_lock_ == NULL);
    unexpected_signal_lock_ = new Mutex("unexpected signal lock", kUnexpectedSignalLock, true);
  }
}

}  // namespace art
