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

#define ATRACE_TAG ATRACE_TAG_DALVIK

#include <stdio.h>
#include <cutils/trace.h>

#include "garbage_collector.h"

#include "base/logging.h"
#include "base/mutex-inl.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "thread.h"
#include "thread_list.h"

namespace art {
namespace gc {
namespace collector {

GarbageCollector::GarbageCollector(Heap* heap, const std::string& name)
    : heap_(heap),
      name_(name),
      verbose_(VLOG_IS_ON(heap)),
      duration_ns_(0),
      timings_(name_.c_str(), true, verbose_),
      cumulative_timings_(name) {
  ResetCumulativeStatistics();
}

bool GarbageCollector::HandleDirtyObjectsPhase() {
  DCHECK(IsConcurrent());
  return true;
}

void GarbageCollector::RegisterPause(uint64_t nano_length) {
  pause_times_.push_back(nano_length);
}

void GarbageCollector::ResetCumulativeStatistics() {
  cumulative_timings_.Reset();
  total_time_ns_ = 0;
  total_paused_time_ns_ = 0;
  total_freed_objects_ = 0;
  total_freed_bytes_ = 0;
}

void GarbageCollector::Run() {
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  uint64_t start_time = NanoTime();
  pause_times_.clear();
  duration_ns_ = 0;

  InitializePhase();

  if (!IsConcurrent()) {
    // Pause is the entire length of the GC.
    uint64_t pause_start = NanoTime();
    ATRACE_BEGIN("Application threads suspended");
    thread_list->SuspendAll();
    MarkingPhase();
    ReclaimPhase();
    thread_list->ResumeAll();
    ATRACE_END();
    uint64_t pause_end = NanoTime();
    pause_times_.push_back(pause_end - pause_start);
  } else {
    Thread* self = Thread::Current();
    {
      ReaderMutexLock mu(self, *Locks::mutator_lock_);
      MarkingPhase();
    }
    bool done = false;
    while (!done) {
      uint64_t pause_start = NanoTime();
      ATRACE_BEGIN("Suspending mutator threads");
      thread_list->SuspendAll();
      ATRACE_END();
      ATRACE_BEGIN("All mutator threads suspended");
      done = HandleDirtyObjectsPhase();
      ATRACE_END();
      uint64_t pause_end = NanoTime();
      ATRACE_BEGIN("Resuming mutator threads");
      thread_list->ResumeAll();
      ATRACE_END();
      pause_times_.push_back(pause_end - pause_start);
    }
    {
      ReaderMutexLock mu(self, *Locks::mutator_lock_);
      ReclaimPhase();
    }
  }

  uint64_t end_time = NanoTime();
  duration_ns_ = end_time - start_time;

  FinishPhase();
}

void GarbageCollector::SwapBitmaps() {
  // Swap the live and mark bitmaps for each alloc space. This is needed since sweep re-swaps
  // these bitmaps. The bitmap swapping is an optimization so that we do not need to clear the live
  // bits of dead objects in the live bitmap.
  const GcType gc_type = GetGcType();
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    // We never allocate into zygote spaces.
    if (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyAlwaysCollect ||
        (gc_type == kGcTypeFull &&
         space->GetGcRetentionPolicy() == space::kGcRetentionPolicyFullCollect)) {
      accounting::SpaceBitmap* live_bitmap = space->GetLiveBitmap();
      accounting::SpaceBitmap* mark_bitmap = space->GetMarkBitmap();
      if (live_bitmap != mark_bitmap) {
        heap_->GetLiveBitmap()->ReplaceBitmap(live_bitmap, mark_bitmap);
        heap_->GetMarkBitmap()->ReplaceBitmap(mark_bitmap, live_bitmap);
        space->AsDlMallocSpace()->SwapBitmaps();
      }
    }
  }
  for (const auto& disc_space : GetHeap()->GetDiscontinuousSpaces()) {
    space::LargeObjectSpace* space = down_cast<space::LargeObjectSpace*>(disc_space);
    accounting::SpaceSetMap* live_set = space->GetLiveObjects();
    accounting::SpaceSetMap* mark_set = space->GetMarkObjects();
    heap_->GetLiveBitmap()->ReplaceObjectSet(live_set, mark_set);
    heap_->GetMarkBitmap()->ReplaceObjectSet(mark_set, live_set);
    down_cast<space::LargeObjectSpace*>(space)->SwapBitmaps();
  }
}

}  // namespace collector
}  // namespace gc
}  // namespace art
