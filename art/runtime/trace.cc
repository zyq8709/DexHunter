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

#include "trace.h"

#include <sys/uio.h>

#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "common_throws.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "instrumentation.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "os.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "thread.h"
#include "thread_list.h"
#if !defined(ART_USE_PORTABLE_COMPILER)
#include "entrypoints/quick/quick_entrypoints.h"
#endif

namespace art {

// File format:
//     header
//     record 0
//     record 1
//     ...
//
// Header format:
//     u4  magic ('SLOW')
//     u2  version
//     u2  offset to data
//     u8  start date/time in usec
//     u2  record size in bytes (version >= 2 only)
//     ... padding to 32 bytes
//
// Record format v1:
//     u1  thread ID
//     u4  method ID | method action
//     u4  time delta since start, in usec
//
// Record format v2:
//     u2  thread ID
//     u4  method ID | method action
//     u4  time delta since start, in usec
//
// Record format v3:
//     u2  thread ID
//     u4  method ID | method action
//     u4  time delta since start, in usec
//     u4  wall time since start, in usec (when clock == "dual" only)
//
// 32 bits of microseconds is 70 minutes.
//
// All values are stored in little-endian order.

enum TraceAction {
    kTraceMethodEnter = 0x00,       // method entry
    kTraceMethodExit = 0x01,        // method exit
    kTraceUnroll = 0x02,            // method exited by exception unrolling
    // 0x03 currently unused
    kTraceMethodActionMask = 0x03,  // two bits
};

class BuildStackTraceVisitor : public StackVisitor {
 public:
  explicit BuildStackTraceVisitor(Thread* thread) : StackVisitor(thread, NULL),
      method_trace_(Trace::AllocStackTrace()) {}

  bool VisitFrame() {
    mirror::ArtMethod* m = GetMethod();
    // Ignore runtime frames (in particular callee save).
    if (!m->IsRuntimeMethod()) {
      method_trace_->push_back(m);
    }
    return true;
  }

  // Returns a stack trace where the topmost frame corresponds with the first element of the vector.
  std::vector<mirror::ArtMethod*>* GetStackTrace() const {
    return method_trace_;
  }

 private:
  std::vector<mirror::ArtMethod*>* const method_trace_;
};

static const char     kTraceTokenChar             = '*';
static const uint16_t kTraceHeaderLength          = 32;
static const uint32_t kTraceMagicValue            = 0x574f4c53;
static const uint16_t kTraceVersionSingleClock    = 2;
static const uint16_t kTraceVersionDualClock      = 3;
static const uint16_t kTraceRecordSizeSingleClock = 10;  // using v2
static const uint16_t kTraceRecordSizeDualClock   = 14;  // using v3 with two timestamps

#if defined(HAVE_POSIX_CLOCKS)
ProfilerClockSource Trace::default_clock_source_ = kProfilerClockSourceDual;
#else
ProfilerClockSource Trace::default_clock_source_ = kProfilerClockSourceWall;
#endif

Trace* volatile Trace::the_trace_ = NULL;
pthread_t Trace::sampling_pthread_ = 0U;
UniquePtr<std::vector<mirror::ArtMethod*> > Trace::temp_stack_trace_;

static mirror::ArtMethod* DecodeTraceMethodId(uint32_t tmid) {
  return reinterpret_cast<mirror::ArtMethod*>(tmid & ~kTraceMethodActionMask);
}

static TraceAction DecodeTraceAction(uint32_t tmid) {
  return static_cast<TraceAction>(tmid & kTraceMethodActionMask);
}

static uint32_t EncodeTraceMethodAndAction(const mirror::ArtMethod* method,
                                           TraceAction action) {
  uint32_t tmid = reinterpret_cast<uint32_t>(method) | action;
  DCHECK_EQ(method, DecodeTraceMethodId(tmid));
  return tmid;
}

std::vector<mirror::ArtMethod*>* Trace::AllocStackTrace() {
  if (temp_stack_trace_.get() != NULL) {
    return temp_stack_trace_.release();
  } else {
    return new std::vector<mirror::ArtMethod*>();
  }
}

void Trace::FreeStackTrace(std::vector<mirror::ArtMethod*>* stack_trace) {
  stack_trace->clear();
  temp_stack_trace_.reset(stack_trace);
}

void Trace::SetDefaultClockSource(ProfilerClockSource clock_source) {
#if defined(HAVE_POSIX_CLOCKS)
  default_clock_source_ = clock_source;
#else
  if (clock_source != kProfilerClockSourceWall) {
    LOG(WARNING) << "Ignoring tracing request to use CPU time.";
  }
#endif
}

static uint16_t GetTraceVersion(ProfilerClockSource clock_source) {
  return (clock_source == kProfilerClockSourceDual) ? kTraceVersionDualClock
                                                    : kTraceVersionSingleClock;
}

static uint16_t GetRecordSize(ProfilerClockSource clock_source) {
  return (clock_source == kProfilerClockSourceDual) ? kTraceRecordSizeDualClock
                                                    : kTraceRecordSizeSingleClock;
}

bool Trace::UseThreadCpuClock() {
  return (clock_source_ == kProfilerClockSourceThreadCpu) ||
      (clock_source_ == kProfilerClockSourceDual);
}

bool Trace::UseWallClock() {
  return (clock_source_ == kProfilerClockSourceWall) ||
      (clock_source_ == kProfilerClockSourceDual);
}

static void MeasureClockOverhead(Trace* trace) {
  if (trace->UseThreadCpuClock()) {
    Thread::Current()->GetCpuMicroTime();
  }
  if (trace->UseWallClock()) {
    MicroTime();
  }
}

// Compute an average time taken to measure clocks.
static uint32_t GetClockOverheadNanoSeconds(Trace* trace) {
  Thread* self = Thread::Current();
  uint64_t start = self->GetCpuMicroTime();

  for (int i = 4000; i > 0; i--) {
    MeasureClockOverhead(trace);
    MeasureClockOverhead(trace);
    MeasureClockOverhead(trace);
    MeasureClockOverhead(trace);
    MeasureClockOverhead(trace);
    MeasureClockOverhead(trace);
    MeasureClockOverhead(trace);
    MeasureClockOverhead(trace);
  }

  uint64_t elapsed_us = self->GetCpuMicroTime() - start;
  return static_cast<uint32_t>(elapsed_us / 32);
}

// TODO: put this somewhere with the big-endian equivalent used by JDWP.
static void Append2LE(uint8_t* buf, uint16_t val) {
  *buf++ = static_cast<uint8_t>(val);
  *buf++ = static_cast<uint8_t>(val >> 8);
}

// TODO: put this somewhere with the big-endian equivalent used by JDWP.
static void Append4LE(uint8_t* buf, uint32_t val) {
  *buf++ = static_cast<uint8_t>(val);
  *buf++ = static_cast<uint8_t>(val >> 8);
  *buf++ = static_cast<uint8_t>(val >> 16);
  *buf++ = static_cast<uint8_t>(val >> 24);
}

// TODO: put this somewhere with the big-endian equivalent used by JDWP.
static void Append8LE(uint8_t* buf, uint64_t val) {
  *buf++ = static_cast<uint8_t>(val);
  *buf++ = static_cast<uint8_t>(val >> 8);
  *buf++ = static_cast<uint8_t>(val >> 16);
  *buf++ = static_cast<uint8_t>(val >> 24);
  *buf++ = static_cast<uint8_t>(val >> 32);
  *buf++ = static_cast<uint8_t>(val >> 40);
  *buf++ = static_cast<uint8_t>(val >> 48);
  *buf++ = static_cast<uint8_t>(val >> 56);
}

static void GetSample(Thread* thread, void* arg) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  BuildStackTraceVisitor build_trace_visitor(thread);
  build_trace_visitor.WalkStack();
  std::vector<mirror::ArtMethod*>* stack_trace = build_trace_visitor.GetStackTrace();
  Trace* the_trace = reinterpret_cast<Trace*>(arg);
  the_trace->CompareAndUpdateStackTrace(thread, stack_trace);
}

static void ClearThreadStackTraceAndClockBase(Thread* thread, void* arg) {
  thread->SetTraceClockBase(0);
  std::vector<mirror::ArtMethod*>* stack_trace = thread->GetStackTraceSample();
  thread->SetStackTraceSample(NULL);
  delete stack_trace;
}

void Trace::CompareAndUpdateStackTrace(Thread* thread,
                                       std::vector<mirror::ArtMethod*>* stack_trace) {
  CHECK_EQ(pthread_self(), sampling_pthread_);
  std::vector<mirror::ArtMethod*>* old_stack_trace = thread->GetStackTraceSample();
  // Update the thread's stack trace sample.
  thread->SetStackTraceSample(stack_trace);
  // Read timer clocks to use for all events in this trace.
  uint32_t thread_clock_diff = 0;
  uint32_t wall_clock_diff = 0;
  ReadClocks(thread, &thread_clock_diff, &wall_clock_diff);
  if (old_stack_trace == NULL) {
    // If there's no previous stack trace sample for this thread, log an entry event for all
    // methods in the trace.
    for (std::vector<mirror::ArtMethod*>::reverse_iterator rit = stack_trace->rbegin();
         rit != stack_trace->rend(); ++rit) {
      LogMethodTraceEvent(thread, *rit, instrumentation::Instrumentation::kMethodEntered,
                          thread_clock_diff, wall_clock_diff);
    }
  } else {
    // If there's a previous stack trace for this thread, diff the traces and emit entry and exit
    // events accordingly.
    std::vector<mirror::ArtMethod*>::reverse_iterator old_rit = old_stack_trace->rbegin();
    std::vector<mirror::ArtMethod*>::reverse_iterator rit = stack_trace->rbegin();
    // Iterate bottom-up over both traces until there's a difference between them.
    while (old_rit != old_stack_trace->rend() && rit != stack_trace->rend() && *old_rit == *rit) {
      old_rit++;
      rit++;
    }
    // Iterate top-down over the old trace until the point where they differ, emitting exit events.
    for (std::vector<mirror::ArtMethod*>::iterator old_it = old_stack_trace->begin();
         old_it != old_rit.base(); ++old_it) {
      LogMethodTraceEvent(thread, *old_it, instrumentation::Instrumentation::kMethodExited,
                          thread_clock_diff, wall_clock_diff);
    }
    // Iterate bottom-up over the new trace from the point where they differ, emitting entry events.
    for (; rit != stack_trace->rend(); ++rit) {
      LogMethodTraceEvent(thread, *rit, instrumentation::Instrumentation::kMethodEntered,
                          thread_clock_diff, wall_clock_diff);
    }
    FreeStackTrace(old_stack_trace);
  }
}

void* Trace::RunSamplingThread(void* arg) {
  Runtime* runtime = Runtime::Current();
  int interval_us = reinterpret_cast<int>(arg);
  CHECK(runtime->AttachCurrentThread("Sampling Profiler", true, runtime->GetSystemThreadGroup(),
                                     !runtime->IsCompiler()));

  while (true) {
    usleep(interval_us);
    ATRACE_BEGIN("Profile sampling");
    Thread* self = Thread::Current();
    Trace* the_trace;
    {
      MutexLock mu(self, *Locks::trace_lock_);
      the_trace = the_trace_;
      if (the_trace == NULL) {
        break;
      }
    }

    runtime->GetThreadList()->SuspendAll();
    {
      MutexLock mu(self, *Locks::thread_list_lock_);
      runtime->GetThreadList()->ForEach(GetSample, the_trace);
    }
    runtime->GetThreadList()->ResumeAll();
    ATRACE_END();
  }

  runtime->DetachCurrentThread();
  return NULL;
}

void Trace::Start(const char* trace_filename, int trace_fd, int buffer_size, int flags,
                  bool direct_to_ddms, bool sampling_enabled, int interval_us) {
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, *Locks::trace_lock_);
    if (the_trace_ != NULL) {
      LOG(ERROR) << "Trace already in progress, ignoring this request";
      return;
    }
  }
  Runtime* runtime = Runtime::Current();
  runtime->GetThreadList()->SuspendAll();

  // Open trace file if not going directly to ddms.
  UniquePtr<File> trace_file;
  if (!direct_to_ddms) {
    if (trace_fd < 0) {
      trace_file.reset(OS::CreateEmptyFile(trace_filename));
    } else {
      trace_file.reset(new File(trace_fd, "tracefile"));
      trace_file->DisableAutoClose();
    }
    if (trace_file.get() == NULL) {
      PLOG(ERROR) << "Unable to open trace file '" << trace_filename << "'";
      runtime->GetThreadList()->ResumeAll();
      ScopedObjectAccess soa(self);
      ThrowRuntimeException("Unable to open trace file '%s'", trace_filename);
      return;
    }
  }

  // Create Trace object.
  {
    MutexLock mu(self, *Locks::trace_lock_);
    if (the_trace_ != NULL) {
      LOG(ERROR) << "Trace already in progress, ignoring this request";
    } else {
      the_trace_ = new Trace(trace_file.release(), buffer_size, flags, sampling_enabled);

      // Enable count of allocs if specified in the flags.
      if ((flags && kTraceCountAllocs) != 0) {
        runtime->SetStatsEnabled(true);
      }



      if (sampling_enabled) {
        CHECK_PTHREAD_CALL(pthread_create, (&sampling_pthread_, NULL, &RunSamplingThread,
                                            reinterpret_cast<void*>(interval_us)),
                                            "Sampling profiler thread");
      } else {
        runtime->GetInstrumentation()->AddListener(the_trace_,
                                                   instrumentation::Instrumentation::kMethodEntered |
                                                   instrumentation::Instrumentation::kMethodExited |
                                                   instrumentation::Instrumentation::kMethodUnwind);
      }
    }
  }
  runtime->GetThreadList()->ResumeAll();
}

void Trace::Stop() {
  Runtime* runtime = Runtime::Current();
  runtime->GetThreadList()->SuspendAll();
  Trace* the_trace = NULL;
  pthread_t sampling_pthread = 0U;
  {
    MutexLock mu(Thread::Current(), *Locks::trace_lock_);
    if (the_trace_ == NULL) {
      LOG(ERROR) << "Trace stop requested, but no trace currently running";
    } else {
      the_trace = the_trace_;
      the_trace_ = NULL;
      sampling_pthread = sampling_pthread_;
      sampling_pthread_ = 0U;
    }
  }
  if (the_trace != NULL) {
    the_trace->FinishTracing();

    if (the_trace->sampling_enabled_) {
      MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
      runtime->GetThreadList()->ForEach(ClearThreadStackTraceAndClockBase, NULL);
    } else {
      runtime->GetInstrumentation()->RemoveListener(the_trace,
                                                    instrumentation::Instrumentation::kMethodEntered |
                                                    instrumentation::Instrumentation::kMethodExited |
                                                    instrumentation::Instrumentation::kMethodUnwind);
    }
    delete the_trace;
  }
  runtime->GetThreadList()->ResumeAll();

  if (sampling_pthread != 0U) {
    CHECK_PTHREAD_CALL(pthread_join, (sampling_pthread, NULL), "sampling thread shutdown");
  }
}

void Trace::Shutdown() {
  if (GetMethodTracingMode() != kTracingInactive) {
    Stop();
  }
}

TracingMode Trace::GetMethodTracingMode() {
  MutexLock mu(Thread::Current(), *Locks::trace_lock_);
  if (the_trace_ == NULL) {
    return kTracingInactive;
  } else if (the_trace_->sampling_enabled_) {
    return kSampleProfilingActive;
  } else {
    return kMethodTracingActive;
  }
}

Trace::Trace(File* trace_file, int buffer_size, int flags, bool sampling_enabled)
    : trace_file_(trace_file), buf_(new uint8_t[buffer_size]()), flags_(flags),
      sampling_enabled_(sampling_enabled), clock_source_(default_clock_source_),
      buffer_size_(buffer_size), start_time_(MicroTime()), cur_offset_(0),  overflow_(false) {
  // Set up the beginning of the trace.
  uint16_t trace_version = GetTraceVersion(clock_source_);
  memset(buf_.get(), 0, kTraceHeaderLength);
  Append4LE(buf_.get(), kTraceMagicValue);
  Append2LE(buf_.get() + 4, trace_version);
  Append2LE(buf_.get() + 6, kTraceHeaderLength);
  Append8LE(buf_.get() + 8, start_time_);
  if (trace_version >= kTraceVersionDualClock) {
    uint16_t record_size = GetRecordSize(clock_source_);
    Append2LE(buf_.get() + 16, record_size);
  }

  // Update current offset.
  cur_offset_ = kTraceHeaderLength;
}

static void DumpBuf(uint8_t* buf, size_t buf_size, ProfilerClockSource clock_source)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint8_t* ptr = buf + kTraceHeaderLength;
  uint8_t* end = buf + buf_size;

  while (ptr < end) {
    uint32_t tmid = ptr[2] | (ptr[3] << 8) | (ptr[4] << 16) | (ptr[5] << 24);
    mirror::ArtMethod* method = DecodeTraceMethodId(tmid);
    TraceAction action = DecodeTraceAction(tmid);
    LOG(INFO) << PrettyMethod(method) << " " << static_cast<int>(action);
    ptr += GetRecordSize(clock_source);
  }
}

void Trace::FinishTracing() {
  // Compute elapsed time.
  uint64_t elapsed = MicroTime() - start_time_;

  size_t final_offset = cur_offset_;
  uint32_t clock_overhead_ns = GetClockOverheadNanoSeconds(this);

  if ((flags_ & kTraceCountAllocs) != 0) {
    Runtime::Current()->SetStatsEnabled(false);
  }

  std::set<mirror::ArtMethod*> visited_methods;
  GetVisitedMethods(final_offset, &visited_methods);

  std::ostringstream os;

  os << StringPrintf("%cversion\n", kTraceTokenChar);
  os << StringPrintf("%d\n", GetTraceVersion(clock_source_));
  os << StringPrintf("data-file-overflow=%s\n", overflow_ ? "true" : "false");
  if (UseThreadCpuClock()) {
    if (UseWallClock()) {
      os << StringPrintf("clock=dual\n");
    } else {
      os << StringPrintf("clock=thread-cpu\n");
    }
  } else {
    os << StringPrintf("clock=wall\n");
  }
  os << StringPrintf("elapsed-time-usec=%llu\n", elapsed);
  size_t num_records = (final_offset - kTraceHeaderLength) / GetRecordSize(clock_source_);
  os << StringPrintf("num-method-calls=%zd\n", num_records);
  os << StringPrintf("clock-call-overhead-nsec=%d\n", clock_overhead_ns);
  os << StringPrintf("vm=art\n");
  if ((flags_ & kTraceCountAllocs) != 0) {
    os << StringPrintf("alloc-count=%d\n", Runtime::Current()->GetStat(KIND_ALLOCATED_OBJECTS));
    os << StringPrintf("alloc-size=%d\n", Runtime::Current()->GetStat(KIND_ALLOCATED_BYTES));
    os << StringPrintf("gc-count=%d\n", Runtime::Current()->GetStat(KIND_GC_INVOCATIONS));
  }
  os << StringPrintf("%cthreads\n", kTraceTokenChar);
  DumpThreadList(os);
  os << StringPrintf("%cmethods\n", kTraceTokenChar);
  DumpMethodList(os, visited_methods);
  os << StringPrintf("%cend\n", kTraceTokenChar);

  std::string header(os.str());
  if (trace_file_.get() == NULL) {
    iovec iov[2];
    iov[0].iov_base = reinterpret_cast<void*>(const_cast<char*>(header.c_str()));
    iov[0].iov_len = header.length();
    iov[1].iov_base = buf_.get();
    iov[1].iov_len = final_offset;
    Dbg::DdmSendChunkV(CHUNK_TYPE("MPSE"), iov, 2);
    const bool kDumpTraceInfo = false;
    if (kDumpTraceInfo) {
      LOG(INFO) << "Trace sent:\n" << header;
      DumpBuf(buf_.get(), final_offset, clock_source_);
    }
  } else {
    if (!trace_file_->WriteFully(header.c_str(), header.length()) ||
        !trace_file_->WriteFully(buf_.get(), final_offset)) {
      std::string detail(StringPrintf("Trace data write failed: %s", strerror(errno)));
      PLOG(ERROR) << detail;
      ThrowRuntimeException("%s", detail.c_str());
    }
  }
}

void Trace::DexPcMoved(Thread* thread, mirror::Object* this_object,
                       const mirror::ArtMethod* method, uint32_t new_dex_pc) {
  // We're not recorded to listen to this kind of event, so complain.
  LOG(ERROR) << "Unexpected dex PC event in tracing " << PrettyMethod(method) << " " << new_dex_pc;
};

void Trace::MethodEntered(Thread* thread, mirror::Object* this_object,
                          const mirror::ArtMethod* method, uint32_t dex_pc) {
  uint32_t thread_clock_diff = 0;
  uint32_t wall_clock_diff = 0;
  ReadClocks(thread, &thread_clock_diff, &wall_clock_diff);
  LogMethodTraceEvent(thread, method, instrumentation::Instrumentation::kMethodEntered,
                      thread_clock_diff, wall_clock_diff);
}

void Trace::MethodExited(Thread* thread, mirror::Object* this_object,
                         const mirror::ArtMethod* method, uint32_t dex_pc,
                         const JValue& return_value) {
  UNUSED(return_value);
  uint32_t thread_clock_diff = 0;
  uint32_t wall_clock_diff = 0;
  ReadClocks(thread, &thread_clock_diff, &wall_clock_diff);
  LogMethodTraceEvent(thread, method, instrumentation::Instrumentation::kMethodExited,
                      thread_clock_diff, wall_clock_diff);
}

void Trace::MethodUnwind(Thread* thread, const mirror::ArtMethod* method, uint32_t dex_pc) {
  uint32_t thread_clock_diff = 0;
  uint32_t wall_clock_diff = 0;
  ReadClocks(thread, &thread_clock_diff, &wall_clock_diff);
  LogMethodTraceEvent(thread, method, instrumentation::Instrumentation::kMethodUnwind,
                      thread_clock_diff, wall_clock_diff);
}

void Trace::ExceptionCaught(Thread* thread, const ThrowLocation& throw_location,
                            mirror::ArtMethod* catch_method, uint32_t catch_dex_pc,
                            mirror::Throwable* exception_object)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  LOG(ERROR) << "Unexpected exception caught event in tracing";
}

void Trace::ReadClocks(Thread* thread, uint32_t* thread_clock_diff, uint32_t* wall_clock_diff) {
  if (UseThreadCpuClock()) {
    uint64_t clock_base = thread->GetTraceClockBase();
    if (UNLIKELY(clock_base == 0)) {
      // First event, record the base time in the map.
      uint64_t time = thread->GetCpuMicroTime();
      thread->SetTraceClockBase(time);
    } else {
      *thread_clock_diff = thread->GetCpuMicroTime() - clock_base;
    }
  }
  if (UseWallClock()) {
    *wall_clock_diff = MicroTime() - start_time_;
  }
}

void Trace::LogMethodTraceEvent(Thread* thread, const mirror::ArtMethod* method,
                                instrumentation::Instrumentation::InstrumentationEvent event,
                                uint32_t thread_clock_diff, uint32_t wall_clock_diff) {
  // Advance cur_offset_ atomically.
  int32_t new_offset;
  int32_t old_offset;
  do {
    old_offset = cur_offset_;
    new_offset = old_offset + GetRecordSize(clock_source_);
    if (new_offset > buffer_size_) {
      overflow_ = true;
      return;
    }
  } while (android_atomic_release_cas(old_offset, new_offset, &cur_offset_) != 0);

  TraceAction action = kTraceMethodEnter;
  switch (event) {
    case instrumentation::Instrumentation::kMethodEntered:
      action = kTraceMethodEnter;
      break;
    case instrumentation::Instrumentation::kMethodExited:
      action = kTraceMethodExit;
      break;
    case instrumentation::Instrumentation::kMethodUnwind:
      action = kTraceUnroll;
      break;
    default:
      UNIMPLEMENTED(FATAL) << "Unexpected event: " << event;
  }

  uint32_t method_value = EncodeTraceMethodAndAction(method, action);

  // Write data
  uint8_t* ptr = buf_.get() + old_offset;
  Append2LE(ptr, thread->GetTid());
  Append4LE(ptr + 2, method_value);
  ptr += 6;

  if (UseThreadCpuClock()) {
    Append4LE(ptr, thread_clock_diff);
    ptr += 4;
  }
  if (UseWallClock()) {
    Append4LE(ptr, wall_clock_diff);
  }
}

void Trace::GetVisitedMethods(size_t buf_size,
                              std::set<mirror::ArtMethod*>* visited_methods) {
  uint8_t* ptr = buf_.get() + kTraceHeaderLength;
  uint8_t* end = buf_.get() + buf_size;

  while (ptr < end) {
    uint32_t tmid = ptr[2] | (ptr[3] << 8) | (ptr[4] << 16) | (ptr[5] << 24);
    mirror::ArtMethod* method = DecodeTraceMethodId(tmid);
    visited_methods->insert(method);
    ptr += GetRecordSize(clock_source_);
  }
}

void Trace::DumpMethodList(std::ostream& os, const std::set<mirror::ArtMethod*>& visited_methods) {
  MethodHelper mh;
  for (const auto& method : visited_methods) {
    mh.ChangeMethod(method);
    os << StringPrintf("%p\t%s\t%s\t%s\t%s\n", method,
        PrettyDescriptor(mh.GetDeclaringClassDescriptor()).c_str(), mh.GetName(),
        mh.GetSignature().c_str(), mh.GetDeclaringClassSourceFile());
  }
}

static void DumpThread(Thread* t, void* arg) {
  std::ostream& os = *reinterpret_cast<std::ostream*>(arg);
  std::string name;
  t->GetThreadName(name);
  os << t->GetTid() << "\t" << name << "\n";
}

void Trace::DumpThreadList(std::ostream& os) {
  Thread* self = Thread::Current();
  Locks::thread_list_lock_->AssertNotHeld(self);
  MutexLock mu(self, *Locks::thread_list_lock_);
  Runtime::Current()->GetThreadList()->ForEach(DumpThread, &os);
}

}  // namespace art
