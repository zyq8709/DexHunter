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

#ifndef ART_RUNTIME_GC_COLLECTOR_GARBAGE_COLLECTOR_H_
#define ART_RUNTIME_GC_COLLECTOR_GARBAGE_COLLECTOR_H_

#include "gc_type.h"
#include "locks.h"
#include "base/timing_logger.h"

#include <stdint.h>
#include <vector>

namespace art {
namespace gc {

class Heap;

namespace collector {

class GarbageCollector {
 public:
  // Returns true iff the garbage collector is concurrent.
  virtual bool IsConcurrent() const = 0;

  GarbageCollector(Heap* heap, const std::string& name);
  virtual ~GarbageCollector() { }

  const char* GetName() const {
    return name_.c_str();
  }

  virtual GcType GetGcType() const = 0;

  // Run the garbage collector.
  void Run();

  Heap* GetHeap() const {
    return heap_;
  }

  // Returns how long the mutators were paused in nanoseconds.
  const std::vector<uint64_t>& GetPauseTimes() const {
    return pause_times_;
  }

  // Returns how long the GC took to complete in nanoseconds.
  uint64_t GetDurationNs() const {
    return duration_ns_;
  }

  void RegisterPause(uint64_t nano_length);

  base::TimingLogger& GetTimings() {
    return timings_;
  }

  CumulativeLogger& GetCumulativeTimings() {
    return cumulative_timings_;
  }

  void ResetCumulativeStatistics();

  // Swap the live and mark bitmaps of spaces that are active for the collector. For partial GC,
  // this is the allocation space, for full GC then we swap the zygote bitmaps too.
  void SwapBitmaps() EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

 protected:
  // The initial phase. Done without mutators paused.
  virtual void InitializePhase() = 0;

  // Mark all reachable objects, done concurrently.
  virtual void MarkingPhase() = 0;

  // Only called for concurrent GCs. Gets called repeatedly until it succeeds.
  virtual bool HandleDirtyObjectsPhase() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Called with mutators running.
  virtual void ReclaimPhase() = 0;

  // Called after the GC is finished. Done without mutators paused.
  virtual void FinishPhase() = 0;

  Heap* const heap_;

  std::string name_;

  const bool verbose_;

  uint64_t duration_ns_;
  base::TimingLogger timings_;

  // Cumulative statistics.
  uint64_t total_time_ns_;
  uint64_t total_paused_time_ns_;
  uint64_t total_freed_objects_;
  uint64_t total_freed_bytes_;

  CumulativeLogger cumulative_timings_;

  std::vector<uint64_t> pause_times_;
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_GARBAGE_COLLECTOR_H_
