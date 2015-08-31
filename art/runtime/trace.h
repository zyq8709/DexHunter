/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_TRACE_H_
#define ART_RUNTIME_TRACE_H_

#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "globals.h"
#include "instrumentation.h"
#include "os.h"
#include "safe_map.h"
#include "UniquePtr.h"

namespace art {

namespace mirror {
  class ArtMethod;
}  // namespace mirror
class Thread;

enum ProfilerClockSource {
  kProfilerClockSourceThreadCpu,
  kProfilerClockSourceWall,
  kProfilerClockSourceDual,  // Both wall and thread CPU clocks.
};

enum TracingMode {
  kTracingInactive,
  kMethodTracingActive,
  kSampleProfilingActive,
};

class Trace : public instrumentation::InstrumentationListener {
 public:
  enum TraceFlag {
    kTraceCountAllocs = 1,
  };

  static void SetDefaultClockSource(ProfilerClockSource clock_source);

  static void Start(const char* trace_filename, int trace_fd, int buffer_size, int flags,
                    bool direct_to_ddms, bool sampling_enabled, int interval_us)
  LOCKS_EXCLUDED(Locks::mutator_lock_,
                 Locks::thread_list_lock_,
                 Locks::thread_suspend_count_lock_,
                 Locks::trace_lock_);
  static void Stop() LOCKS_EXCLUDED(Locks::trace_lock_);
  static void Shutdown() LOCKS_EXCLUDED(Locks::trace_lock_);
  static TracingMode GetMethodTracingMode() LOCKS_EXCLUDED(Locks::trace_lock_);

  bool UseWallClock();
  bool UseThreadCpuClock();

  void CompareAndUpdateStackTrace(Thread* thread, std::vector<mirror::ArtMethod*>* stack_trace)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  virtual void MethodEntered(Thread* thread, mirror::Object* this_object,
                             const mirror::ArtMethod* method, uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  virtual void MethodExited(Thread* thread, mirror::Object* this_object,
                            const mirror::ArtMethod* method, uint32_t dex_pc,
                            const JValue& return_value)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  virtual void MethodUnwind(Thread* thread, const mirror::ArtMethod* method, uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  virtual void DexPcMoved(Thread* thread, mirror::Object* this_object,
                          const mirror::ArtMethod* method, uint32_t new_dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  virtual void ExceptionCaught(Thread* thread, const ThrowLocation& throw_location,
                               mirror::ArtMethod* catch_method, uint32_t catch_dex_pc,
                               mirror::Throwable* exception_object)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Reuse an old stack trace if it exists, otherwise allocate a new one.
  static std::vector<mirror::ArtMethod*>* AllocStackTrace();
  // Clear and store an old stack trace for later use.
  static void FreeStackTrace(std::vector<mirror::ArtMethod*>* stack_trace);

 private:
  explicit Trace(File* trace_file, int buffer_size, int flags, bool sampling_enabled);

  // The sampling interval in microseconds is passed as an argument.
  static void* RunSamplingThread(void* arg) LOCKS_EXCLUDED(Locks::trace_lock_);

  void FinishTracing() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void ReadClocks(Thread* thread, uint32_t* thread_clock_diff, uint32_t* wall_clock_diff);

  void LogMethodTraceEvent(Thread* thread, const mirror::ArtMethod* method,
                           instrumentation::Instrumentation::InstrumentationEvent event,
                           uint32_t thread_clock_diff, uint32_t wall_clock_diff);

  // Methods to output traced methods and threads.
  void GetVisitedMethods(size_t end_offset, std::set<mirror::ArtMethod*>* visited_methods);
  void DumpMethodList(std::ostream& os, const std::set<mirror::ArtMethod*>& visited_methods)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void DumpThreadList(std::ostream& os) LOCKS_EXCLUDED(Locks::thread_list_lock_);

  // Singleton instance of the Trace or NULL when no method tracing is active.
  static Trace* volatile the_trace_ GUARDED_BY(Locks::trace_lock_);

  // The default profiler clock source.
  static ProfilerClockSource default_clock_source_;

  // Sampling thread, non-zero when sampling.
  static pthread_t sampling_pthread_;

  // Used to remember an unused stack trace to avoid re-allocation during sampling.
  static UniquePtr<std::vector<mirror::ArtMethod*> > temp_stack_trace_;

  // File to write trace data out to, NULL if direct to ddms.
  UniquePtr<File> trace_file_;

  // Buffer to store trace data.
  UniquePtr<uint8_t> buf_;

  // Flags enabling extra tracing of things such as alloc counts.
  const int flags_;

  // True if traceview should sample instead of instrumenting method entry/exit.
  const bool sampling_enabled_;

  const ProfilerClockSource clock_source_;

  // Size of buf_.
  const int buffer_size_;

  // Time trace was created.
  const uint64_t start_time_;

  // Offset into buf_.
  volatile int32_t cur_offset_;

  // Did we overflow the buffer recording traces?
  bool overflow_;

  DISALLOW_COPY_AND_ASSIGN(Trace);
};

}  // namespace art

#endif  // ART_RUNTIME_TRACE_H_
