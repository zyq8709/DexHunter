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

#include "runtime.h"

// sys/mount.h has to come before linux/fs.h due to redefinition of MS_RDONLY, MS_BIND, etc
#include <sys/mount.h>
#include <linux/fs.h>

#include <signal.h>
#include <sys/syscall.h>

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#include "arch/arm/registers_arm.h"
#include "arch/mips/registers_mips.h"
#include "arch/x86/registers_x86.h"
#include "atomic.h"
#include "class_linker.h"
#include "debugger.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "gc/space/space.h"
#include "image.h"
#include "instrumentation.h"
#include "intern_table.h"
#include "invoke_arg_array_builder.h"
#include "jni_internal.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/array.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/throwable.h"
#include "monitor.h"
#include "oat_file.h"
#include "ScopedLocalRef.h"
#include "scoped_thread_state_change.h"
#include "signal_catcher.h"
#include "signal_set.h"
#include "sirt_ref.h"
#include "thread.h"
#include "thread_list.h"
#include "trace.h"
#include "UniquePtr.h"
#include "verifier/method_verifier.h"
#include "well_known_classes.h"

#include "JniConstants.h"  // Last to avoid LOG redefinition in ics-mr1-plus-art.

namespace art {

Runtime* Runtime::instance_ = NULL;

Runtime::Runtime()
    : is_compiler_(false),
      is_zygote_(false),
      is_concurrent_gc_enabled_(true),
      is_explicit_gc_disabled_(false),
      default_stack_size_(0),
      heap_(NULL),
      monitor_list_(NULL),
      thread_list_(NULL),
      intern_table_(NULL),
      class_linker_(NULL),
      signal_catcher_(NULL),
      java_vm_(NULL),
      pre_allocated_OutOfMemoryError_(NULL),
      resolution_method_(NULL),
      threads_being_born_(0),
      shutdown_cond_(new ConditionVariable("Runtime shutdown", *Locks::runtime_shutdown_lock_)),
      shutting_down_(false),
      shutting_down_started_(false),
      started_(false),
      finished_starting_(false),
      vfprintf_(NULL),
      exit_(NULL),
      abort_(NULL),
      stats_enabled_(false),
      method_trace_(0),
      method_trace_file_size_(0),
      instrumentation_(),
      use_compile_time_class_path_(false),
      main_thread_group_(NULL),
      system_thread_group_(NULL),
      system_class_loader_(NULL) {
  for (int i = 0; i < Runtime::kLastCalleeSaveType; i++) {
    callee_save_methods_[i] = NULL;
  }
}

Runtime::~Runtime() {
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
    shutting_down_started_ = true;
    while (threads_being_born_ > 0) {
      shutdown_cond_->Wait(self);
    }
    shutting_down_ = true;
  }
  Trace::Shutdown();

  // Make sure to let the GC complete if it is running.
  heap_->WaitForConcurrentGcToComplete(self);
  heap_->DeleteThreadPool();

  // Make sure our internal threads are dead before we start tearing down things they're using.
  Dbg::StopJdwp();
  delete signal_catcher_;

  // Make sure all other non-daemon threads have terminated, and all daemon threads are suspended.
  delete thread_list_;
  delete monitor_list_;
  delete class_linker_;
  delete heap_;
  delete intern_table_;
  delete java_vm_;
  Thread::Shutdown();
  QuasiAtomic::Shutdown();
  verifier::MethodVerifier::Shutdown();
  // TODO: acquire a static mutex on Runtime to avoid racing.
  CHECK(instance_ == NULL || instance_ == this);
  instance_ = NULL;
}

struct AbortState {
  void Dump(std::ostream& os) {
    if (gAborting > 1) {
      os << "Runtime aborting --- recursively, so no thread-specific detail!\n";
      return;
    }
    gAborting++;
    os << "Runtime aborting...\n";
    if (Runtime::Current() == NULL) {
      os << "(Runtime does not yet exist!)\n";
      return;
    }
    Thread* self = Thread::Current();
    if (self == NULL) {
      os << "(Aborting thread was not attached to runtime!)\n";
    } else {
      // TODO: we're aborting and the ScopedObjectAccess may attempt to acquire the mutator_lock_
      //       which may block indefinitely if there's a misbehaving thread holding it exclusively.
      //       The code below should be made robust to this.
      ScopedObjectAccess soa(self);
      os << "Aborting thread:\n";
      self->Dump(os);
      if (self->IsExceptionPending()) {
        ThrowLocation throw_location;
        mirror::Throwable* exception = self->GetException(&throw_location);
        os << "Pending exception " << PrettyTypeOf(exception)
            << " thrown by '" << throw_location.Dump() << "'\n"
            << exception->Dump();
      }
    }
    DumpAllThreads(os, self);
  }

  void DumpAllThreads(std::ostream& os, Thread* self) NO_THREAD_SAFETY_ANALYSIS {
    bool tll_already_held = Locks::thread_list_lock_->IsExclusiveHeld(self);
    bool ml_already_held = Locks::mutator_lock_->IsSharedHeld(self);
    if (!tll_already_held || !ml_already_held) {
      os << "Dumping all threads without appropriate locks held:"
          << (!tll_already_held ? " thread list lock" : "")
          << (!ml_already_held ? " mutator lock" : "")
          << "\n";
    }
    os << "All threads:\n";
    Runtime::Current()->GetThreadList()->DumpLocked(os);
  }
};

void Runtime::Abort() {
  gAborting++;  // set before taking any locks

  // Ensure that we don't have multiple threads trying to abort at once,
  // which would result in significantly worse diagnostics.
  MutexLock mu(Thread::Current(), *Locks::abort_lock_);

  // Get any pending output out of the way.
  fflush(NULL);

  // Many people have difficulty distinguish aborts from crashes,
  // so be explicit.
  AbortState state;
  LOG(INTERNAL_FATAL) << Dumpable<AbortState>(state);

  // Call the abort hook if we have one.
  if (Runtime::Current() != NULL && Runtime::Current()->abort_ != NULL) {
    LOG(INTERNAL_FATAL) << "Calling abort hook...";
    Runtime::Current()->abort_();
    // notreached
    LOG(INTERNAL_FATAL) << "Unexpectedly returned from abort hook!";
  }

#if defined(__GLIBC__)
  // TODO: we ought to be able to use pthread_kill(3) here (or abort(3),
  // which POSIX defines in terms of raise(3), which POSIX defines in terms
  // of pthread_kill(3)). On Linux, though, libcorkscrew can't unwind through
  // libpthread, which means the stacks we dump would be useless. Calling
  // tgkill(2) directly avoids that.
  syscall(__NR_tgkill, getpid(), GetTid(), SIGABRT);
  // TODO: LLVM installs it's own SIGABRT handler so exit to be safe... Can we disable that in LLVM?
  // If not, we could use sigaction(3) before calling tgkill(2) and lose this call to exit(3).
  exit(1);
#else
  abort();
#endif
  // notreached
}

bool Runtime::PreZygoteFork() {
  heap_->PreZygoteFork();
  return true;
}

void Runtime::CallExitHook(jint status) {
  if (exit_ != NULL) {
    ScopedThreadStateChange tsc(Thread::Current(), kNative);
    exit_(status);
    LOG(WARNING) << "Exit hook returned instead of exiting!";
  }
}

// Parse a string of the form /[0-9]+[kKmMgG]?/, which is used to specify
// memory sizes.  [kK] indicates kilobytes, [mM] megabytes, and
// [gG] gigabytes.
//
// "s" should point just past the "-Xm?" part of the string.
// "div" specifies a divisor, e.g. 1024 if the value must be a multiple
// of 1024.
//
// The spec says the -Xmx and -Xms options must be multiples of 1024.  It
// doesn't say anything about -Xss.
//
// Returns 0 (a useless size) if "s" is malformed or specifies a low or
// non-evenly-divisible value.
//
size_t ParseMemoryOption(const char* s, size_t div) {
  // strtoul accepts a leading [+-], which we don't want,
  // so make sure our string starts with a decimal digit.
  if (isdigit(*s)) {
    char* s2;
    size_t val = strtoul(s, &s2, 10);
    if (s2 != s) {
      // s2 should be pointing just after the number.
      // If this is the end of the string, the user
      // has specified a number of bytes.  Otherwise,
      // there should be exactly one more character
      // that specifies a multiplier.
      if (*s2 != '\0') {
        // The remainder of the string is either a single multiplier
        // character, or nothing to indicate that the value is in
        // bytes.
        char c = *s2++;
        if (*s2 == '\0') {
          size_t mul;
          if (c == '\0') {
            mul = 1;
          } else if (c == 'k' || c == 'K') {
            mul = KB;
          } else if (c == 'm' || c == 'M') {
            mul = MB;
          } else if (c == 'g' || c == 'G') {
            mul = GB;
          } else {
            // Unknown multiplier character.
            return 0;
          }

          if (val <= std::numeric_limits<size_t>::max() / mul) {
            val *= mul;
          } else {
            // Clamp to a multiple of 1024.
            val = std::numeric_limits<size_t>::max() & ~(1024-1);
          }
        } else {
          // There's more than one character after the numeric part.
          return 0;
        }
      }
      // The man page says that a -Xm value must be a multiple of 1024.
      if (val % div == 0) {
        return val;
      }
    }
  }
  return 0;
}

size_t ParseIntegerOrDie(const std::string& s) {
  std::string::size_type colon = s.find(':');
  if (colon == std::string::npos) {
    LOG(FATAL) << "Missing integer: " << s;
  }
  const char* begin = &s[colon + 1];
  char* end;
  size_t result = strtoul(begin, &end, 10);
  if (begin == end || *end != '\0') {
    LOG(FATAL) << "Failed to parse integer in: " << s;
  }
  return result;
}

Runtime::ParsedOptions* Runtime::ParsedOptions::Create(const Options& options, bool ignore_unrecognized) {
  UniquePtr<ParsedOptions> parsed(new ParsedOptions());
  const char* boot_class_path_string = getenv("BOOTCLASSPATH");
  if (boot_class_path_string != NULL) {
    parsed->boot_class_path_string_ = boot_class_path_string;
  }
  const char* class_path_string = getenv("CLASSPATH");
  if (class_path_string != NULL) {
    parsed->class_path_string_ = class_path_string;
  }
  // -Xcheck:jni is off by default for regular builds but on by default in debug builds.
  parsed->check_jni_ = kIsDebugBuild;

  parsed->heap_initial_size_ = gc::Heap::kDefaultInitialSize;
  parsed->heap_maximum_size_ = gc::Heap::kDefaultMaximumSize;
  parsed->heap_min_free_ = gc::Heap::kDefaultMinFree;
  parsed->heap_max_free_ = gc::Heap::kDefaultMaxFree;
  parsed->heap_target_utilization_ = gc::Heap::kDefaultTargetUtilization;
  parsed->heap_growth_limit_ = 0;  // 0 means no growth limit.
  // Default to number of processors minus one since the main GC thread also does work.
  parsed->parallel_gc_threads_ = sysconf(_SC_NPROCESSORS_CONF) - 1;
  // Only the main GC thread, no workers.
  parsed->conc_gc_threads_ = 0;
  parsed->stack_size_ = 0;  // 0 means default.
  parsed->low_memory_mode_ = false;

  parsed->is_compiler_ = false;
  parsed->is_zygote_ = false;
  parsed->interpreter_only_ = false;
  parsed->is_concurrent_gc_enabled_ = true;
  parsed->is_explicit_gc_disabled_ = false;

  parsed->long_pause_log_threshold_ = gc::Heap::kDefaultLongPauseLogThreshold;
  parsed->long_gc_log_threshold_ = gc::Heap::kDefaultLongGCLogThreshold;
  parsed->ignore_max_footprint_ = false;

  parsed->lock_profiling_threshold_ = 0;
  parsed->hook_is_sensitive_thread_ = NULL;

  parsed->hook_vfprintf_ = vfprintf;
  parsed->hook_exit_ = exit;
  parsed->hook_abort_ = NULL;  // We don't call abort(3) by default; see Runtime::Abort.

  parsed->compiler_filter_ = Runtime::kDefaultCompilerFilter;
  parsed->huge_method_threshold_ = Runtime::kDefaultHugeMethodThreshold;
  parsed->large_method_threshold_ = Runtime::kDefaultLargeMethodThreshold;
  parsed->small_method_threshold_ = Runtime::kDefaultSmallMethodThreshold;
  parsed->tiny_method_threshold_ = Runtime::kDefaultTinyMethodThreshold;
  parsed->num_dex_methods_threshold_ = Runtime::kDefaultNumDexMethodsThreshold;

  parsed->sea_ir_mode_ = false;
//  gLogVerbosity.class_linker = true;  // TODO: don't check this in!
//  gLogVerbosity.compiler = true;  // TODO: don't check this in!
//  gLogVerbosity.verifier = true;  // TODO: don't check this in!
//  gLogVerbosity.heap = true;  // TODO: don't check this in!
//  gLogVerbosity.gc = true;  // TODO: don't check this in!
//  gLogVerbosity.jdwp = true;  // TODO: don't check this in!
//  gLogVerbosity.jni = true;  // TODO: don't check this in!
//  gLogVerbosity.monitor = true;  // TODO: don't check this in!
//  gLogVerbosity.startup = true;  // TODO: don't check this in!
//  gLogVerbosity.third_party_jni = true;  // TODO: don't check this in!
//  gLogVerbosity.threads = true;  // TODO: don't check this in!

  parsed->method_trace_ = false;
  parsed->method_trace_file_ = "/data/method-trace-file.bin";
  parsed->method_trace_file_size_ = 10 * MB;

  for (size_t i = 0; i < options.size(); ++i) {
    const std::string option(options[i].first);
    if (true && options[0].first == "-Xzygote") {
      LOG(INFO) << "option[" << i << "]=" << option;
    }
    if (StartsWith(option, "-Xbootclasspath:")) {
      parsed->boot_class_path_string_ = option.substr(strlen("-Xbootclasspath:")).data();
    } else if (option == "-classpath" || option == "-cp") {
      // TODO: support -Djava.class.path
      i++;
      if (i == options.size()) {
        // TODO: usage
        LOG(FATAL) << "Missing required class path value for " << option;
        return NULL;
      }
      const StringPiece& value = options[i].first;
      parsed->class_path_string_ = value.data();
    } else if (option == "bootclasspath") {
      parsed->boot_class_path_
          = reinterpret_cast<const std::vector<const DexFile*>*>(options[i].second);
    } else if (StartsWith(option, "-Ximage:")) {
      parsed->image_ = option.substr(strlen("-Ximage:")).data();
    } else if (StartsWith(option, "-Xcheck:jni")) {
      parsed->check_jni_ = true;
    } else if (StartsWith(option, "-Xrunjdwp:") || StartsWith(option, "-agentlib:jdwp=")) {
      std::string tail(option.substr(option[1] == 'X' ? 10 : 15));
      if (tail == "help" || !Dbg::ParseJdwpOptions(tail)) {
        LOG(FATAL) << "Example: -Xrunjdwp:transport=dt_socket,address=8000,server=y\n"
                   << "Example: -Xrunjdwp:transport=dt_socket,address=localhost:6500,server=n";
        return NULL;
      }
    } else if (StartsWith(option, "-Xms")) {
      size_t size = ParseMemoryOption(option.substr(strlen("-Xms")).c_str(), 1024);
      if (size == 0) {
        if (ignore_unrecognized) {
          continue;
        }
        // TODO: usage
        LOG(FATAL) << "Failed to parse " << option;
        return NULL;
      }
      parsed->heap_initial_size_ = size;
    } else if (StartsWith(option, "-Xmx")) {
      size_t size = ParseMemoryOption(option.substr(strlen("-Xmx")).c_str(), 1024);
      if (size == 0) {
        if (ignore_unrecognized) {
          continue;
        }
        // TODO: usage
        LOG(FATAL) << "Failed to parse " << option;
        return NULL;
      }
      parsed->heap_maximum_size_ = size;
    } else if (StartsWith(option, "-XX:HeapGrowthLimit=")) {
      size_t size = ParseMemoryOption(option.substr(strlen("-XX:HeapGrowthLimit=")).c_str(), 1024);
      if (size == 0) {
        if (ignore_unrecognized) {
          continue;
        }
        // TODO: usage
        LOG(FATAL) << "Failed to parse " << option;
        return NULL;
      }
      parsed->heap_growth_limit_ = size;
    } else if (StartsWith(option, "-XX:HeapMinFree=")) {
      size_t size = ParseMemoryOption(option.substr(strlen("-XX:HeapMinFree=")).c_str(), 1024);
      if (size == 0) {
        if (ignore_unrecognized) {
          continue;
        }
        // TODO: usage
        LOG(FATAL) << "Failed to parse " << option;
        return NULL;
      }
      parsed->heap_min_free_ = size;
    } else if (StartsWith(option, "-XX:HeapMaxFree=")) {
      size_t size = ParseMemoryOption(option.substr(strlen("-XX:HeapMaxFree=")).c_str(), 1024);
      if (size == 0) {
        if (ignore_unrecognized) {
          continue;
        }
        // TODO: usage
        LOG(FATAL) << "Failed to parse " << option;
        return NULL;
      }
      parsed->heap_max_free_ = size;
    } else if (StartsWith(option, "-XX:HeapTargetUtilization=")) {
      std::istringstream iss(option.substr(strlen("-XX:HeapTargetUtilization=")));
      double value;
      iss >> value;
      // Ensure that we have a value, there was no cruft after it and it satisfies a sensible range.
      const bool sane_val = iss.eof() && (value >= 0.1) && (value <= 0.9);
      if (!sane_val) {
        if (ignore_unrecognized) {
          continue;
        }
        LOG(FATAL) << "Invalid option '" << option << "'";
        return NULL;
      }
      parsed->heap_target_utilization_ = value;
    } else if (StartsWith(option, "-XX:ParallelGCThreads=")) {
      parsed->parallel_gc_threads_ =
          ParseMemoryOption(option.substr(strlen("-XX:ParallelGCThreads=")).c_str(), 1024);
    } else if (StartsWith(option, "-XX:ConcGCThreads=")) {
      parsed->conc_gc_threads_ =
          ParseMemoryOption(option.substr(strlen("-XX:ConcGCThreads=")).c_str(), 1024);
    } else if (StartsWith(option, "-Xss")) {
      size_t size = ParseMemoryOption(option.substr(strlen("-Xss")).c_str(), 1);
      if (size == 0) {
        if (ignore_unrecognized) {
          continue;
        }
        // TODO: usage
        LOG(FATAL) << "Failed to parse " << option;
        return NULL;
      }
      parsed->stack_size_ = size;
    } else if (option == "-XX:LongPauseLogThreshold") {
      parsed->long_pause_log_threshold_ =
          ParseMemoryOption(option.substr(strlen("-XX:LongPauseLogThreshold=")).c_str(), 1024);
    } else if (option == "-XX:LongGCLogThreshold") {
          parsed->long_gc_log_threshold_ =
              ParseMemoryOption(option.substr(strlen("-XX:LongGCLogThreshold")).c_str(), 1024);
    } else if (option == "-XX:IgnoreMaxFootprint") {
      parsed->ignore_max_footprint_ = true;
    } else if (option == "-XX:LowMemoryMode") {
      parsed->low_memory_mode_ = true;
    } else if (StartsWith(option, "-D")) {
      parsed->properties_.push_back(option.substr(strlen("-D")));
    } else if (StartsWith(option, "-Xjnitrace:")) {
      parsed->jni_trace_ = option.substr(strlen("-Xjnitrace:"));
    } else if (option == "compiler") {
      parsed->is_compiler_ = true;
    } else if (option == "-Xzygote") {
      parsed->is_zygote_ = true;
    } else if (option == "-Xint") {
      parsed->interpreter_only_ = true;
    } else if (StartsWith(option, "-Xgc:")) {
      std::vector<std::string> gc_options;
      Split(option.substr(strlen("-Xgc:")), ',', gc_options);
      for (size_t i = 0; i < gc_options.size(); ++i) {
        if (gc_options[i] == "noconcurrent") {
          parsed->is_concurrent_gc_enabled_ = false;
        } else if (gc_options[i] == "concurrent") {
          parsed->is_concurrent_gc_enabled_ = true;
        } else {
          LOG(WARNING) << "Ignoring unknown -Xgc option: " << gc_options[i];
        }
      }
    } else if (option == "-XX:+DisableExplicitGC") {
      parsed->is_explicit_gc_disabled_ = true;
    } else if (StartsWith(option, "-verbose:")) {
      std::vector<std::string> verbose_options;
      Split(option.substr(strlen("-verbose:")), ',', verbose_options);
      for (size_t i = 0; i < verbose_options.size(); ++i) {
        if (verbose_options[i] == "class") {
          gLogVerbosity.class_linker = true;
        } else if (verbose_options[i] == "verifier") {
          gLogVerbosity.verifier = true;
        } else if (verbose_options[i] == "compiler") {
          gLogVerbosity.compiler = true;
        } else if (verbose_options[i] == "heap") {
          gLogVerbosity.heap = true;
        } else if (verbose_options[i] == "gc") {
          gLogVerbosity.gc = true;
        } else if (verbose_options[i] == "jdwp") {
          gLogVerbosity.jdwp = true;
        } else if (verbose_options[i] == "jni") {
          gLogVerbosity.jni = true;
        } else if (verbose_options[i] == "monitor") {
          gLogVerbosity.monitor = true;
        } else if (verbose_options[i] == "startup") {
          gLogVerbosity.startup = true;
        } else if (verbose_options[i] == "third-party-jni") {
          gLogVerbosity.third_party_jni = true;
        } else if (verbose_options[i] == "threads") {
          gLogVerbosity.threads = true;
        } else {
          LOG(WARNING) << "Ignoring unknown -verbose option: " << verbose_options[i];
        }
      }
    } else if (StartsWith(option, "-Xjnigreflimit:")) {
      // Silently ignored for backwards compatibility.
    } else if (StartsWith(option, "-Xlockprofthreshold:")) {
      parsed->lock_profiling_threshold_ = ParseIntegerOrDie(option);
    } else if (StartsWith(option, "-Xstacktracefile:")) {
      parsed->stack_trace_file_ = option.substr(strlen("-Xstacktracefile:"));
    } else if (option == "sensitiveThread") {
      parsed->hook_is_sensitive_thread_ = reinterpret_cast<bool (*)()>(const_cast<void*>(options[i].second));
    } else if (option == "vfprintf") {
      parsed->hook_vfprintf_ =
          reinterpret_cast<int (*)(FILE *, const char*, va_list)>(const_cast<void*>(options[i].second));
    } else if (option == "exit") {
      parsed->hook_exit_ = reinterpret_cast<void(*)(jint)>(const_cast<void*>(options[i].second));
    } else if (option == "abort") {
      parsed->hook_abort_ = reinterpret_cast<void(*)()>(const_cast<void*>(options[i].second));
    } else if (option == "host-prefix") {
      parsed->host_prefix_ = reinterpret_cast<const char*>(options[i].second);
    } else if (option == "-Xgenregmap" || option == "-Xgc:precise") {
      // We silently ignore these for backwards compatibility.
    } else if (option == "-Xmethod-trace") {
      parsed->method_trace_ = true;
    } else if (StartsWith(option, "-Xmethod-trace-file:")) {
      parsed->method_trace_file_ = option.substr(strlen("-Xmethod-trace-file:"));
    } else if (StartsWith(option, "-Xmethod-trace-file-size:")) {
      parsed->method_trace_file_size_ = ParseIntegerOrDie(option);
    } else if (option == "-Xprofile:threadcpuclock") {
      Trace::SetDefaultClockSource(kProfilerClockSourceThreadCpu);
    } else if (option == "-Xprofile:wallclock") {
      Trace::SetDefaultClockSource(kProfilerClockSourceWall);
    } else if (option == "-Xprofile:dualclock") {
      Trace::SetDefaultClockSource(kProfilerClockSourceDual);
    } else if (option == "-compiler-filter:interpret-only") {
      parsed->compiler_filter_ = kInterpretOnly;
    } else if (option == "-compiler-filter:space") {
      parsed->compiler_filter_ = kSpace;
    } else if (option == "-compiler-filter:balanced") {
      parsed->compiler_filter_ = kBalanced;
    } else if (option == "-compiler-filter:speed") {
      parsed->compiler_filter_ = kSpeed;
    } else if (option == "-compiler-filter:everything") {
      parsed->compiler_filter_ = kEverything;
    } else if (option == "-sea_ir") {
      parsed->sea_ir_mode_ = true;
    } else if (StartsWith(option, "-huge-method-max:")) {
      parsed->huge_method_threshold_ = ParseIntegerOrDie(option);
    } else if (StartsWith(option, "-large-method-max:")) {
      parsed->large_method_threshold_ = ParseIntegerOrDie(option);
    } else if (StartsWith(option, "-small-method-max:")) {
      parsed->small_method_threshold_ = ParseIntegerOrDie(option);
    } else if (StartsWith(option, "-tiny-method-max:")) {
      parsed->tiny_method_threshold_ = ParseIntegerOrDie(option);
    } else if (StartsWith(option, "-num-dex-methods-max:")) {
      parsed->num_dex_methods_threshold_ = ParseIntegerOrDie(option);
    } else {
      if (!ignore_unrecognized) {
        // TODO: print usage via vfprintf
        LOG(ERROR) << "Unrecognized option " << option;
        // TODO: this should exit, but for now tolerate unknown options
        // return NULL;
      }
    }
  }

  // If a reference to the dalvik core.jar snuck in, replace it with
  // the art specific version. This can happen with on device
  // boot.art/boot.oat generation by GenerateImage which relies on the
  // value of BOOTCLASSPATH.
  std::string core_jar("/core.jar");
  size_t core_jar_pos = parsed->boot_class_path_string_.find(core_jar);
  if (core_jar_pos != std::string::npos) {
    parsed->boot_class_path_string_.replace(core_jar_pos, core_jar.size(), "/core-libart.jar");
  }

  if (!parsed->is_compiler_ && parsed->image_.empty()) {
    parsed->image_ += GetAndroidRoot();
    parsed->image_ += "/framework/boot.art";
  }
  if (parsed->heap_growth_limit_ == 0) {
    parsed->heap_growth_limit_ = parsed->heap_maximum_size_;
  }

  return parsed.release();
}

bool Runtime::Create(const Options& options, bool ignore_unrecognized) {
  // TODO: acquire a static mutex on Runtime to avoid racing.
  if (Runtime::instance_ != NULL) {
    return false;
  }
  InitLogging(NULL);  // Calls Locks::Init() as a side effect.
  instance_ = new Runtime;
  if (!instance_->Init(options, ignore_unrecognized)) {
    delete instance_;
    instance_ = NULL;
    return false;
  }
  return true;
}

jobject CreateSystemClassLoader() {
  if (Runtime::Current()->UseCompileTimeClassPath()) {
    return NULL;
  }

  ScopedObjectAccess soa(Thread::Current());

  mirror::Class* class_loader_class =
      soa.Decode<mirror::Class*>(WellKnownClasses::java_lang_ClassLoader);
  CHECK(Runtime::Current()->GetClassLinker()->EnsureInitialized(class_loader_class, true, true));

  mirror::ArtMethod* getSystemClassLoader =
      class_loader_class->FindDirectMethod("getSystemClassLoader", "()Ljava/lang/ClassLoader;");
  CHECK(getSystemClassLoader != NULL);

  JValue result;
  ArgArray arg_array(NULL, 0);
  InvokeWithArgArray(soa, getSystemClassLoader, &arg_array, &result, 'L');
  mirror::ClassLoader* class_loader = down_cast<mirror::ClassLoader*>(result.GetL());
  CHECK(class_loader != NULL);

  JNIEnv* env = soa.Self()->GetJniEnv();
  ScopedLocalRef<jobject> system_class_loader(env, soa.AddLocalReference<jobject>(class_loader));
  CHECK(system_class_loader.get() != NULL);

  soa.Self()->SetClassLoaderOverride(class_loader);

  mirror::Class* thread_class = soa.Decode<mirror::Class*>(WellKnownClasses::java_lang_Thread);
  CHECK(Runtime::Current()->GetClassLinker()->EnsureInitialized(thread_class, true, true));

  mirror::ArtField* contextClassLoader = thread_class->FindDeclaredInstanceField("contextClassLoader",
                                                                                 "Ljava/lang/ClassLoader;");
  CHECK(contextClassLoader != NULL);

  contextClassLoader->SetObject(soa.Self()->GetPeer(), class_loader);

  return env->NewGlobalRef(system_class_loader.get());
}

bool Runtime::Start() {
  VLOG(startup) << "Runtime::Start entering";

  CHECK(host_prefix_.empty()) << host_prefix_;

  // Restore main thread state to kNative as expected by native code.
  Thread* self = Thread::Current();
  self->TransitionFromRunnableToSuspended(kNative);

  started_ = true;

  // InitNativeMethods needs to be after started_ so that the classes
  // it touches will have methods linked to the oat file if necessary.
  InitNativeMethods();

  // Initialize well known thread group values that may be accessed threads while attaching.
  InitThreadGroups(self);

  Thread::FinishStartup();

  if (is_zygote_) {
    if (!InitZygote()) {
      return false;
    }
  } else {
    DidForkFromZygote();
  }

  StartDaemonThreads();

  system_class_loader_ = CreateSystemClassLoader();

  self->GetJniEnv()->locals.AssertEmpty();

  VLOG(startup) << "Runtime::Start exiting";

  finished_starting_ = true;

  return true;
}

void Runtime::EndThreadBirth() EXCLUSIVE_LOCKS_REQUIRED(Locks::runtime_shutdown_lock_) {
  DCHECK_GT(threads_being_born_, 0U);
  threads_being_born_--;
  if (shutting_down_started_ && threads_being_born_ == 0) {
    shutdown_cond_->Broadcast(Thread::Current());
  }
}

// Do zygote-mode-only initialization.
bool Runtime::InitZygote() {
  // zygote goes into its own process group
  setpgid(0, 0);

  // See storage config details at http://source.android.com/tech/storage/
  // Create private mount namespace shared by all children
  if (unshare(CLONE_NEWNS) == -1) {
    PLOG(WARNING) << "Failed to unshare()";
    return false;
  }

  // Mark rootfs as being a slave so that changes from default
  // namespace only flow into our children.
  if (mount("rootfs", "/", NULL, (MS_SLAVE | MS_REC), NULL) == -1) {
    PLOG(WARNING) << "Failed to mount() rootfs as MS_SLAVE";
    return false;
  }

  // Create a staging tmpfs that is shared by our children; they will
  // bind mount storage into their respective private namespaces, which
  // are isolated from each other.
  const char* target_base = getenv("EMULATED_STORAGE_TARGET");
  if (target_base != NULL) {
    if (mount("tmpfs", target_base, "tmpfs", MS_NOSUID | MS_NODEV,
              "uid=0,gid=1028,mode=0751") == -1) {
      LOG(WARNING) << "Failed to mount tmpfs to " << target_base;
      return false;
    }
  }

  return true;
}

void Runtime::DidForkFromZygote() {
  is_zygote_ = false;

  // Create the thread pool.
  heap_->CreateThreadPool();

  StartSignalCatcher();

  // Start the JDWP thread. If the command-line debugger flags specified "suspend=y",
  // this will pause the runtime, so we probably want this to come last.
  Dbg::StartJdwp();
}

void Runtime::StartSignalCatcher() {
  if (!is_zygote_) {
    signal_catcher_ = new SignalCatcher(stack_trace_file_);
  }
}

void Runtime::StartDaemonThreads() {
  VLOG(startup) << "Runtime::StartDaemonThreads entering";

  Thread* self = Thread::Current();

  // Must be in the kNative state for calling native methods.
  CHECK_EQ(self->GetState(), kNative);

  JNIEnv* env = self->GetJniEnv();
  env->CallStaticVoidMethod(WellKnownClasses::java_lang_Daemons,
                            WellKnownClasses::java_lang_Daemons_start);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    LOG(FATAL) << "Error starting java.lang.Daemons";
  }

  VLOG(startup) << "Runtime::StartDaemonThreads exiting";
}

bool Runtime::Init(const Options& raw_options, bool ignore_unrecognized) {
  CHECK_EQ(sysconf(_SC_PAGE_SIZE), kPageSize);

  UniquePtr<ParsedOptions> options(ParsedOptions::Create(raw_options, ignore_unrecognized));
  if (options.get() == NULL) {
    LOG(ERROR) << "Failed to parse options";
    return false;
  }
  VLOG(startup) << "Runtime::Init -verbose:startup enabled";

  QuasiAtomic::Startup();

  Monitor::Init(options->lock_profiling_threshold_, options->hook_is_sensitive_thread_);

  host_prefix_ = options->host_prefix_;
  boot_class_path_string_ = options->boot_class_path_string_;
  class_path_string_ = options->class_path_string_;
  properties_ = options->properties_;

  is_compiler_ = options->is_compiler_;
  is_zygote_ = options->is_zygote_;
  is_concurrent_gc_enabled_ = options->is_concurrent_gc_enabled_;
  is_explicit_gc_disabled_ = options->is_explicit_gc_disabled_;

  compiler_filter_ = options->compiler_filter_;
  huge_method_threshold_ = options->huge_method_threshold_;
  large_method_threshold_ = options->large_method_threshold_;
  small_method_threshold_ = options->small_method_threshold_;
  tiny_method_threshold_ = options->tiny_method_threshold_;
  num_dex_methods_threshold_ = options->num_dex_methods_threshold_;

  sea_ir_mode_ = options->sea_ir_mode_;
  vfprintf_ = options->hook_vfprintf_;
  exit_ = options->hook_exit_;
  abort_ = options->hook_abort_;

  default_stack_size_ = options->stack_size_;
  stack_trace_file_ = options->stack_trace_file_;

  monitor_list_ = new MonitorList;
  thread_list_ = new ThreadList;
  intern_table_ = new InternTable;


  if (options->interpreter_only_) {
    GetInstrumentation()->ForceInterpretOnly();
  }

  heap_ = new gc::Heap(options->heap_initial_size_,
                       options->heap_growth_limit_,
                       options->heap_min_free_,
                       options->heap_max_free_,
                       options->heap_target_utilization_,
                       options->heap_maximum_size_,
                       options->image_,
                       options->is_concurrent_gc_enabled_,
                       options->parallel_gc_threads_,
                       options->conc_gc_threads_,
                       options->low_memory_mode_,
                       options->long_pause_log_threshold_,
                       options->long_gc_log_threshold_,
                       options->ignore_max_footprint_);

  BlockSignals();
  InitPlatformSignalHandlers();

  java_vm_ = new JavaVMExt(this, options.get());

  Thread::Startup();

  // ClassLinker needs an attached thread, but we can't fully attach a thread without creating
  // objects. We can't supply a thread group yet; it will be fixed later. Since we are the main
  // thread, we do not get a java peer.
  Thread* self = Thread::Attach("main", false, NULL, false);
  CHECK_EQ(self->thin_lock_id_, ThreadList::kMainId);
  CHECK(self != NULL);

  // Set us to runnable so tools using a runtime can allocate and GC by default
  self->TransitionFromSuspendedToRunnable();

  // Now we're attached, we can take the heap locks and validate the heap.
  GetHeap()->EnableObjectValidation();

  CHECK_GE(GetHeap()->GetContinuousSpaces().size(), 1U);
  if (GetHeap()->GetContinuousSpaces()[0]->IsImageSpace()) {
    class_linker_ = ClassLinker::CreateFromImage(intern_table_);
  } else {
    CHECK(options->boot_class_path_ != NULL);
    CHECK_NE(options->boot_class_path_->size(), 0U);
    class_linker_ = ClassLinker::CreateFromCompiler(*options->boot_class_path_, intern_table_);
  }
  CHECK(class_linker_ != NULL);
  verifier::MethodVerifier::Init();

  method_trace_ = options->method_trace_;
  method_trace_file_ = options->method_trace_file_;
  method_trace_file_size_ = options->method_trace_file_size_;

  if (options->method_trace_) {
    Trace::Start(options->method_trace_file_.c_str(), -1, options->method_trace_file_size_, 0,
                 false, false, 0);
  }

  // Pre-allocate an OutOfMemoryError for the double-OOME case.
  self->ThrowNewException(ThrowLocation(), "Ljava/lang/OutOfMemoryError;",
                          "OutOfMemoryError thrown while trying to throw OutOfMemoryError; no stack available");
  pre_allocated_OutOfMemoryError_ = self->GetException(NULL);
  self->ClearException();

  VLOG(startup) << "Runtime::Init exiting";
  return true;
}

void Runtime::InitNativeMethods() {
  VLOG(startup) << "Runtime::InitNativeMethods entering";
  Thread* self = Thread::Current();
  JNIEnv* env = self->GetJniEnv();

  // Must be in the kNative state for calling native methods (JNI_OnLoad code).
  CHECK_EQ(self->GetState(), kNative);

  // First set up JniConstants, which is used by both the runtime's built-in native
  // methods and libcore.
  JniConstants::init(env);
  WellKnownClasses::Init(env);

  // Then set up the native methods provided by the runtime itself.
  RegisterRuntimeNativeMethods(env);

  // Then set up libcore, which is just a regular JNI library with a regular JNI_OnLoad.
  // Most JNI libraries can just use System.loadLibrary, but libcore can't because it's
  // the library that implements System.loadLibrary!
  {
    std::string mapped_name(StringPrintf(OS_SHARED_LIB_FORMAT_STR, "javacore"));
    std::string reason;
    self->TransitionFromSuspendedToRunnable();
    if (!instance_->java_vm_->LoadNativeLibrary(mapped_name, NULL, reason)) {
      LOG(FATAL) << "LoadNativeLibrary failed for \"" << mapped_name << "\": " << reason;
    }
    self->TransitionFromRunnableToSuspended(kNative);
  }

  // Initialize well known classes that may invoke runtime native methods.
  WellKnownClasses::LateInit(env);

  VLOG(startup) << "Runtime::InitNativeMethods exiting";
}

void Runtime::InitThreadGroups(Thread* self) {
  JNIEnvExt* env = self->GetJniEnv();
  ScopedJniEnvLocalRefState env_state(env);
  main_thread_group_ =
      env->NewGlobalRef(env->GetStaticObjectField(WellKnownClasses::java_lang_ThreadGroup,
                                                  WellKnownClasses::java_lang_ThreadGroup_mainThreadGroup));
  CHECK(main_thread_group_ != NULL || IsCompiler());
  system_thread_group_ =
      env->NewGlobalRef(env->GetStaticObjectField(WellKnownClasses::java_lang_ThreadGroup,
                                                  WellKnownClasses::java_lang_ThreadGroup_systemThreadGroup));
  CHECK(system_thread_group_ != NULL || IsCompiler());
}

jobject Runtime::GetMainThreadGroup() const {
  CHECK(main_thread_group_ != NULL || IsCompiler());
  return main_thread_group_;
}

jobject Runtime::GetSystemThreadGroup() const {
  CHECK(system_thread_group_ != NULL || IsCompiler());
  return system_thread_group_;
}

jobject Runtime::GetSystemClassLoader() const {
  CHECK(system_class_loader_ != NULL || IsCompiler());
  return system_class_loader_;
}

void Runtime::RegisterRuntimeNativeMethods(JNIEnv* env) {
#define REGISTER(FN) extern void FN(JNIEnv*); FN(env)
  // Register Throwable first so that registration of other native methods can throw exceptions
  REGISTER(register_java_lang_Throwable);
  REGISTER(register_dalvik_system_DexFile);
  REGISTER(register_dalvik_system_VMDebug);
  REGISTER(register_dalvik_system_VMRuntime);
  REGISTER(register_dalvik_system_VMStack);
  REGISTER(register_dalvik_system_Zygote);
  REGISTER(register_java_lang_Class);
  REGISTER(register_java_lang_DexCache);
  REGISTER(register_java_lang_Object);
  REGISTER(register_java_lang_Runtime);
  REGISTER(register_java_lang_String);
  REGISTER(register_java_lang_System);
  REGISTER(register_java_lang_Thread);
  REGISTER(register_java_lang_VMClassLoader);
  REGISTER(register_java_lang_reflect_Array);
  REGISTER(register_java_lang_reflect_Constructor);
  REGISTER(register_java_lang_reflect_Field);
  REGISTER(register_java_lang_reflect_Method);
  REGISTER(register_java_lang_reflect_Proxy);
  REGISTER(register_java_util_concurrent_atomic_AtomicLong);
  REGISTER(register_org_apache_harmony_dalvik_ddmc_DdmServer);
  REGISTER(register_org_apache_harmony_dalvik_ddmc_DdmVmInternal);
  REGISTER(register_sun_misc_Unsafe);
#undef REGISTER
}

void Runtime::DumpForSigQuit(std::ostream& os) {
  GetClassLinker()->DumpForSigQuit(os);
  GetInternTable()->DumpForSigQuit(os);
  GetJavaVM()->DumpForSigQuit(os);
  GetHeap()->DumpForSigQuit(os);
  os << "\n";

  thread_list_->DumpForSigQuit(os);
  BaseMutex::DumpAll(os);
}

void Runtime::DumpLockHolders(std::ostream& os) {
  uint64_t mutator_lock_owner = Locks::mutator_lock_->GetExclusiveOwnerTid();
  pid_t thread_list_lock_owner = GetThreadList()->GetLockOwner();
  pid_t classes_lock_owner = GetClassLinker()->GetClassesLockOwner();
  pid_t dex_lock_owner = GetClassLinker()->GetDexLockOwner();
  if ((thread_list_lock_owner | classes_lock_owner | dex_lock_owner) != 0) {
    os << "Mutator lock exclusive owner tid: " << mutator_lock_owner << "\n"
       << "ThreadList lock owner tid: " << thread_list_lock_owner << "\n"
       << "ClassLinker classes lock owner tid: " << classes_lock_owner << "\n"
       << "ClassLinker dex lock owner tid: " << dex_lock_owner << "\n";
  }
}

void Runtime::SetStatsEnabled(bool new_state) {
  if (new_state == true) {
    GetStats()->Clear(~0);
    // TODO: wouldn't it make more sense to clear _all_ threads' stats?
    Thread::Current()->GetStats()->Clear(~0);
  }
  stats_enabled_ = new_state;
}

void Runtime::ResetStats(int kinds) {
  GetStats()->Clear(kinds & 0xffff);
  // TODO: wouldn't it make more sense to clear _all_ threads' stats?
  Thread::Current()->GetStats()->Clear(kinds >> 16);
}

int32_t Runtime::GetStat(int kind) {
  RuntimeStats* stats;
  if (kind < (1<<16)) {
    stats = GetStats();
  } else {
    stats = Thread::Current()->GetStats();
    kind >>= 16;
  }
  switch (kind) {
  case KIND_ALLOCATED_OBJECTS:
    return stats->allocated_objects;
  case KIND_ALLOCATED_BYTES:
    return stats->allocated_bytes;
  case KIND_FREED_OBJECTS:
    return stats->freed_objects;
  case KIND_FREED_BYTES:
    return stats->freed_bytes;
  case KIND_GC_INVOCATIONS:
    return stats->gc_for_alloc_count;
  case KIND_CLASS_INIT_COUNT:
    return stats->class_init_count;
  case KIND_CLASS_INIT_TIME:
    // Convert ns to us, reduce to 32 bits.
    return static_cast<int>(stats->class_init_time_ns / 1000);
  case KIND_EXT_ALLOCATED_OBJECTS:
  case KIND_EXT_ALLOCATED_BYTES:
  case KIND_EXT_FREED_OBJECTS:
  case KIND_EXT_FREED_BYTES:
    return 0;  // backward compatibility
  default:
    LOG(FATAL) << "Unknown statistic " << kind;
    return -1;  // unreachable
  }
}

void Runtime::BlockSignals() {
  SignalSet signals;
  signals.Add(SIGPIPE);
  // SIGQUIT is used to dump the runtime's state (including stack traces).
  signals.Add(SIGQUIT);
  // SIGUSR1 is used to initiate a GC.
  signals.Add(SIGUSR1);
  signals.Block();
}

bool Runtime::AttachCurrentThread(const char* thread_name, bool as_daemon, jobject thread_group,
                                  bool create_peer) {
  bool success = Thread::Attach(thread_name, as_daemon, thread_group, create_peer) != NULL;
  if (thread_name == NULL) {
    LOG(WARNING) << *Thread::Current() << " attached without supplying a name";
  }
  return success;
}

void Runtime::DetachCurrentThread() {
  Thread* self = Thread::Current();
  if (self == NULL) {
    LOG(FATAL) << "attempting to detach thread that is not attached";
  }
  if (self->HasManagedStack()) {
    LOG(FATAL) << *Thread::Current() << " attempting to detach while still running code";
  }
  thread_list_->Unregister(self);
}

  mirror::Throwable* Runtime::GetPreAllocatedOutOfMemoryError() const {
  if (pre_allocated_OutOfMemoryError_ == NULL) {
    LOG(ERROR) << "Failed to return pre-allocated OOME";
  }
  return pre_allocated_OutOfMemoryError_;
}

void Runtime::VisitConcurrentRoots(RootVisitor* visitor, void* arg, bool only_dirty,
                                   bool clean_dirty) {
  intern_table_->VisitRoots(visitor, arg, only_dirty, clean_dirty);
  class_linker_->VisitRoots(visitor, arg, only_dirty, clean_dirty);
}

void Runtime::VisitNonThreadRoots(RootVisitor* visitor, void* arg) {
  java_vm_->VisitRoots(visitor, arg);
  if (pre_allocated_OutOfMemoryError_ != NULL) {
    visitor(pre_allocated_OutOfMemoryError_, arg);
  }
  visitor(resolution_method_, arg);
  for (int i = 0; i < Runtime::kLastCalleeSaveType; i++) {
    visitor(callee_save_methods_[i], arg);
  }
}

void Runtime::VisitNonConcurrentRoots(RootVisitor* visitor, void* arg) {
  thread_list_->VisitRoots(visitor, arg);
  VisitNonThreadRoots(visitor, arg);
}

void Runtime::VisitRoots(RootVisitor* visitor, void* arg, bool only_dirty, bool clean_dirty) {
  VisitConcurrentRoots(visitor, arg, only_dirty, clean_dirty);
  VisitNonConcurrentRoots(visitor, arg);
}

mirror::ArtMethod* Runtime::CreateResolutionMethod() {
  mirror::Class* method_class = mirror::ArtMethod::GetJavaLangReflectArtMethod();
  Thread* self = Thread::Current();
  SirtRef<mirror::ArtMethod>
      method(self, down_cast<mirror::ArtMethod*>(method_class->AllocObject(self)));
  method->SetDeclaringClass(method_class);
  // TODO: use a special method for resolution method saves
  method->SetDexMethodIndex(DexFile::kDexNoIndex);
  // When compiling, the code pointer will get set later when the image is loaded.
  Runtime* r = Runtime::Current();
  ClassLinker* cl = r->GetClassLinker();
  method->SetEntryPointFromCompiledCode(r->IsCompiler() ? NULL : GetResolutionTrampoline(cl));
  return method.get();
}

mirror::ArtMethod* Runtime::CreateCalleeSaveMethod(InstructionSet instruction_set,
                                                        CalleeSaveType type) {
  mirror::Class* method_class = mirror::ArtMethod::GetJavaLangReflectArtMethod();
  Thread* self = Thread::Current();
  SirtRef<mirror::ArtMethod>
      method(self, down_cast<mirror::ArtMethod*>(method_class->AllocObject(self)));
  method->SetDeclaringClass(method_class);
  // TODO: use a special method for callee saves
  method->SetDexMethodIndex(DexFile::kDexNoIndex);
  method->SetEntryPointFromCompiledCode(NULL);
  if ((instruction_set == kThumb2) || (instruction_set == kArm)) {
    uint32_t ref_spills = (1 << art::arm::R5) | (1 << art::arm::R6)  | (1 << art::arm::R7) |
                          (1 << art::arm::R8) | (1 << art::arm::R10) | (1 << art::arm::R11);
    uint32_t arg_spills = (1 << art::arm::R1) | (1 << art::arm::R2) | (1 << art::arm::R3);
    uint32_t all_spills = (1 << art::arm::R4) | (1 << art::arm::R9);
    uint32_t core_spills = ref_spills | (type == kRefsAndArgs ? arg_spills : 0) |
                           (type == kSaveAll ? all_spills : 0) | (1 << art::arm::LR);
    uint32_t fp_all_spills = (1 << art::arm::S0)  | (1 << art::arm::S1)  | (1 << art::arm::S2) |
                             (1 << art::arm::S3)  | (1 << art::arm::S4)  | (1 << art::arm::S5) |
                             (1 << art::arm::S6)  | (1 << art::arm::S7)  | (1 << art::arm::S8) |
                             (1 << art::arm::S9)  | (1 << art::arm::S10) | (1 << art::arm::S11) |
                             (1 << art::arm::S12) | (1 << art::arm::S13) | (1 << art::arm::S14) |
                             (1 << art::arm::S15) | (1 << art::arm::S16) | (1 << art::arm::S17) |
                             (1 << art::arm::S18) | (1 << art::arm::S19) | (1 << art::arm::S20) |
                             (1 << art::arm::S21) | (1 << art::arm::S22) | (1 << art::arm::S23) |
                             (1 << art::arm::S24) | (1 << art::arm::S25) | (1 << art::arm::S26) |
                             (1 << art::arm::S27) | (1 << art::arm::S28) | (1 << art::arm::S29) |
                             (1 << art::arm::S30) | (1 << art::arm::S31);
    uint32_t fp_spills = type == kSaveAll ? fp_all_spills : 0;
    size_t frame_size = RoundUp((__builtin_popcount(core_spills) /* gprs */ +
                                 __builtin_popcount(fp_spills) /* fprs */ +
                                 1 /* Method* */) * kPointerSize, kStackAlignment);
    method->SetFrameSizeInBytes(frame_size);
    method->SetCoreSpillMask(core_spills);
    method->SetFpSpillMask(fp_spills);
  } else if (instruction_set == kMips) {
    uint32_t ref_spills = (1 << art::mips::S2) | (1 << art::mips::S3) | (1 << art::mips::S4) |
                          (1 << art::mips::S5) | (1 << art::mips::S6) | (1 << art::mips::S7) |
                          (1 << art::mips::GP) | (1 << art::mips::FP);
    uint32_t arg_spills = (1 << art::mips::A1) | (1 << art::mips::A2) | (1 << art::mips::A3);
    uint32_t all_spills = (1 << art::mips::S0) | (1 << art::mips::S1);
    uint32_t core_spills = ref_spills | (type == kRefsAndArgs ? arg_spills : 0) |
                           (type == kSaveAll ? all_spills : 0) | (1 << art::mips::RA);
    size_t frame_size = RoundUp((__builtin_popcount(core_spills) /* gprs */ +
                                (type == kRefsAndArgs ? 0 : 3) + 1 /* Method* */) *
                                kPointerSize, kStackAlignment);
    method->SetFrameSizeInBytes(frame_size);
    method->SetCoreSpillMask(core_spills);
    method->SetFpSpillMask(0);
  } else if (instruction_set == kX86) {
    uint32_t ref_spills = (1 << art::x86::EBP) | (1 << art::x86::ESI) | (1 << art::x86::EDI);
    uint32_t arg_spills = (1 << art::x86::ECX) | (1 << art::x86::EDX) | (1 << art::x86::EBX);
    uint32_t core_spills = ref_spills | (type == kRefsAndArgs ? arg_spills : 0) |
                         (1 << art::x86::kNumberOfCpuRegisters);  // fake return address callee save
    size_t frame_size = RoundUp((__builtin_popcount(core_spills) /* gprs */ +
                                 1 /* Method* */) * kPointerSize, kStackAlignment);
    method->SetFrameSizeInBytes(frame_size);
    method->SetCoreSpillMask(core_spills);
    method->SetFpSpillMask(0);
  } else {
    UNIMPLEMENTED(FATAL);
  }
  return method.get();
}

void Runtime::DisallowNewSystemWeaks() {
  monitor_list_->DisallowNewMonitors();
  intern_table_->DisallowNewInterns();
  java_vm_->DisallowNewWeakGlobals();
}

void Runtime::AllowNewSystemWeaks() {
  monitor_list_->AllowNewMonitors();
  intern_table_->AllowNewInterns();
  java_vm_->AllowNewWeakGlobals();
}

void Runtime::SetCalleeSaveMethod(mirror::ArtMethod* method, CalleeSaveType type) {
  DCHECK_LT(static_cast<int>(type), static_cast<int>(kLastCalleeSaveType));
  callee_save_methods_[type] = method;
}

const std::vector<const DexFile*>& Runtime::GetCompileTimeClassPath(jobject class_loader) {
  if (class_loader == NULL) {
    return GetClassLinker()->GetBootClassPath();
  }
  CHECK(UseCompileTimeClassPath());
  CompileTimeClassPaths::const_iterator it = compile_time_class_paths_.find(class_loader);
  CHECK(it != compile_time_class_paths_.end());
  return it->second;
}

void Runtime::SetCompileTimeClassPath(jobject class_loader, std::vector<const DexFile*>& class_path) {
  CHECK(!IsStarted());
  use_compile_time_class_path_ = true;
  compile_time_class_paths_.Put(class_loader, class_path);
}

}  // namespace art
