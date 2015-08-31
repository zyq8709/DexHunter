/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <string.h>
#include <unistd.h>

#include "class_linker.h"
#include "common_throws.h"
#include "debugger.h"
#include "gc/space/dlmalloc_space.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "hprof/hprof.h"
#include "jni_internal.h"
#include "mirror/class.h"
#include "ScopedUtfChars.h"
#include "scoped_thread_state_change.h"
#include "toStringArray.h"
#include "trace.h"

namespace art {

static jobjectArray VMDebug_getVmFeatureList(JNIEnv* env, jclass) {
  std::vector<std::string> features;
  features.push_back("method-trace-profiling");
  features.push_back("method-trace-profiling-streaming");
  features.push_back("method-sample-profiling");
  features.push_back("hprof-heap-dump");
  features.push_back("hprof-heap-dump-streaming");
  return toStringArray(env, features);
}

static void VMDebug_startAllocCounting(JNIEnv*, jclass) {
  Runtime::Current()->SetStatsEnabled(true);
}

static void VMDebug_stopAllocCounting(JNIEnv*, jclass) {
  Runtime::Current()->SetStatsEnabled(false);
}

static jint VMDebug_getAllocCount(JNIEnv*, jclass, jint kind) {
  return Runtime::Current()->GetStat(kind);
}

static void VMDebug_resetAllocCount(JNIEnv*, jclass, jint kinds) {
  Runtime::Current()->ResetStats(kinds);
}

static void VMDebug_startMethodTracingDdmsImpl(JNIEnv*, jclass, jint bufferSize, jint flags,
                                               jboolean samplingEnabled, jint intervalUs) {
  Trace::Start("[DDMS]", -1, bufferSize, flags, true, samplingEnabled, intervalUs);
}

static void VMDebug_startMethodTracingFd(JNIEnv* env, jclass, jstring javaTraceFilename,
                                         jobject javaFd, jint bufferSize, jint flags) {
  int originalFd = jniGetFDFromFileDescriptor(env, javaFd);
  if (originalFd < 0) {
    return;
  }

  int fd = dup(originalFd);
  if (fd < 0) {
    ScopedObjectAccess soa(env);
    ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
    soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/RuntimeException;",
                                   "dup(%d) failed: %s", originalFd, strerror(errno));
    return;
  }

  ScopedUtfChars traceFilename(env, javaTraceFilename);
  if (traceFilename.c_str() == NULL) {
    return;
  }
  Trace::Start(traceFilename.c_str(), fd, bufferSize, flags, false, false, 0);
}

static void VMDebug_startMethodTracingFilename(JNIEnv* env, jclass, jstring javaTraceFilename,
                                               jint bufferSize, jint flags) {
  ScopedUtfChars traceFilename(env, javaTraceFilename);
  if (traceFilename.c_str() == NULL) {
    return;
  }
  Trace::Start(traceFilename.c_str(), -1, bufferSize, flags, false, false, 0);
}

static jint VMDebug_getMethodTracingMode(JNIEnv*, jclass) {
  return Trace::GetMethodTracingMode();
}

static void VMDebug_stopMethodTracing(JNIEnv*, jclass) {
  Trace::Stop();
}

static void VMDebug_startEmulatorTracing(JNIEnv*, jclass) {
  UNIMPLEMENTED(WARNING);
  // dvmEmulatorTraceStart();
}

static void VMDebug_stopEmulatorTracing(JNIEnv*, jclass) {
  UNIMPLEMENTED(WARNING);
  // dvmEmulatorTraceStop();
}

static jboolean VMDebug_isDebuggerConnected(JNIEnv*, jclass) {
  return Dbg::IsDebuggerActive();
}

static jboolean VMDebug_isDebuggingEnabled(JNIEnv*, jclass) {
  return Dbg::IsJdwpConfigured();
}

static jlong VMDebug_lastDebuggerActivity(JNIEnv*, jclass) {
  return Dbg::LastDebuggerActivity();
}

static void ThrowUnsupportedOperationException(JNIEnv* env) {
  ScopedObjectAccess soa(env);
  ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
  soa.Self()->ThrowNewException(throw_location, "Ljava/lang/UnsupportedOperationException;", NULL);
}

static void VMDebug_startInstructionCounting(JNIEnv* env, jclass) {
  ThrowUnsupportedOperationException(env);
}

static void VMDebug_stopInstructionCounting(JNIEnv* env, jclass) {
  ThrowUnsupportedOperationException(env);
}

static void VMDebug_getInstructionCount(JNIEnv* env, jclass, jintArray /*javaCounts*/) {
  ThrowUnsupportedOperationException(env);
}

static void VMDebug_resetInstructionCount(JNIEnv* env, jclass) {
  ThrowUnsupportedOperationException(env);
}

static void VMDebug_printLoadedClasses(JNIEnv* env, jclass, jint flags) {
  ScopedObjectAccess soa(env);
  return Runtime::Current()->GetClassLinker()->DumpAllClasses(flags);
}

static jint VMDebug_getLoadedClassCount(JNIEnv* env, jclass) {
  ScopedObjectAccess soa(env);
  return Runtime::Current()->GetClassLinker()->NumLoadedClasses();
}

/*
 * Returns the thread-specific CPU-time clock value for the current thread,
 * or -1 if the feature isn't supported.
 */
static jlong VMDebug_threadCpuTimeNanos(JNIEnv*, jclass) {
  return ThreadCpuNanoTime();
}

/*
 * static void dumpHprofData(String fileName, FileDescriptor fd)
 *
 * Cause "hprof" data to be dumped.  We can throw an IOException if an
 * error occurs during file handling.
 */
static void VMDebug_dumpHprofData(JNIEnv* env, jclass, jstring javaFilename, jobject javaFd) {
  // Only one of these may be NULL.
  if (javaFilename == NULL && javaFd == NULL) {
    ScopedObjectAccess soa(env);
    ThrowNullPointerException(NULL, "fileName == null && fd == null");
    return;
  }

  std::string filename;
  if (javaFilename != NULL) {
    ScopedUtfChars chars(env, javaFilename);
    if (env->ExceptionCheck()) {
      return;
    }
    filename = chars.c_str();
  } else {
    filename = "[fd]";
  }

  int fd = -1;
  if (javaFd != NULL) {
    fd = jniGetFDFromFileDescriptor(env, javaFd);
    if (fd < 0) {
      ScopedObjectAccess soa(env);
      ThrowRuntimeException("Invalid file descriptor");
      return;
    }
  }

  hprof::DumpHeap(filename.c_str(), fd, false);
}

static void VMDebug_dumpHprofDataDdms(JNIEnv*, jclass) {
  hprof::DumpHeap("[DDMS]", -1, true);
}

static void VMDebug_dumpReferenceTables(JNIEnv* env, jclass) {
  ScopedObjectAccess soa(env);
  LOG(INFO) << "--- reference table dump ---";

  soa.Env()->DumpReferenceTables(LOG(INFO));
  soa.Vm()->DumpReferenceTables(LOG(INFO));

  LOG(INFO) << "---";
}

static void VMDebug_crash(JNIEnv*, jclass) {
  LOG(FATAL) << "Crashing runtime on request";
}

static void VMDebug_infopoint(JNIEnv*, jclass, jint id) {
  LOG(INFO) << "VMDebug infopoint " << id << " hit";
}

static jlong VMDebug_countInstancesOfClass(JNIEnv* env, jclass, jclass javaClass,
                                           jboolean countAssignable) {
  ScopedObjectAccess soa(env);
  mirror::Class* c = soa.Decode<mirror::Class*>(javaClass);
  if (c == NULL) {
    return 0;
  }
  std::vector<mirror::Class*> classes;
  classes.push_back(c);
  uint64_t count = 0;
  Runtime::Current()->GetHeap()->CountInstances(classes, countAssignable, &count);
  return count;
}

// We export the VM internal per-heap-space size/alloc/free metrics
// for the zygote space, alloc space (application heap), and the large
// object space for dumpsys meminfo. The other memory region data such
// as PSS, private/shared dirty/shared data are available via
// /proc/<pid>/smaps.
static void VMDebug_getHeapSpaceStats(JNIEnv* env, jclass, jlongArray data) {
  jlong* arr = reinterpret_cast<jlong*>(env->GetPrimitiveArrayCritical(data, 0));
  if (arr == NULL || env->GetArrayLength(data) < 9) {
    return;
  }

  size_t allocSize = 0;
  size_t allocUsed = 0;
  size_t zygoteSize = 0;
  size_t zygoteUsed = 0;
  size_t largeObjectsSize = 0;
  size_t largeObjectsUsed = 0;

  gc::Heap* heap = Runtime::Current()->GetHeap();
  const std::vector<gc::space::ContinuousSpace*>& continuous_spaces = heap->GetContinuousSpaces();
  const std::vector<gc::space::DiscontinuousSpace*>& discontinuous_spaces = heap->GetDiscontinuousSpaces();
  typedef std::vector<gc::space::ContinuousSpace*>::const_iterator It;
  for (It it = continuous_spaces.begin(), end = continuous_spaces.end(); it != end; ++it) {
    gc::space::ContinuousSpace* space = *it;
    if (space->IsImageSpace()) {
      // Currently don't include the image space.
    } else if (space->IsZygoteSpace()) {
      gc::space::DlMallocSpace* dlmalloc_space = space->AsDlMallocSpace();
      zygoteSize += dlmalloc_space->GetFootprint();
      zygoteUsed += dlmalloc_space->GetBytesAllocated();
    } else {
      // This is the alloc space.
      gc::space::DlMallocSpace* dlmalloc_space = space->AsDlMallocSpace();
      allocSize += dlmalloc_space->GetFootprint();
      allocUsed += dlmalloc_space->GetBytesAllocated();
    }
  }
  typedef std::vector<gc::space::DiscontinuousSpace*>::const_iterator It2;
  for (It2 it = discontinuous_spaces.begin(), end = discontinuous_spaces.end(); it != end; ++it) {
    gc::space::DiscontinuousSpace* space = *it;
    if (space->IsLargeObjectSpace()) {
      largeObjectsSize += space->AsLargeObjectSpace()->GetBytesAllocated();
      largeObjectsUsed += largeObjectsSize;
    }
  }

  size_t allocFree = allocSize - allocUsed;
  size_t zygoteFree = zygoteSize - zygoteUsed;
  size_t largeObjectsFree = largeObjectsSize - largeObjectsUsed;

  int j = 0;
  arr[j++] = allocSize;
  arr[j++] = allocUsed;
  arr[j++] = allocFree;
  arr[j++] = zygoteSize;
  arr[j++] = zygoteUsed;
  arr[j++] = zygoteFree;
  arr[j++] = largeObjectsSize;
  arr[j++] = largeObjectsUsed;
  arr[j++] = largeObjectsFree;
  env->ReleasePrimitiveArrayCritical(data, arr, 0);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(VMDebug, countInstancesOfClass, "(Ljava/lang/Class;Z)J"),
  NATIVE_METHOD(VMDebug, crash, "()V"),
  NATIVE_METHOD(VMDebug, dumpHprofData, "(Ljava/lang/String;Ljava/io/FileDescriptor;)V"),
  NATIVE_METHOD(VMDebug, dumpHprofDataDdms, "()V"),
  NATIVE_METHOD(VMDebug, dumpReferenceTables, "()V"),
  NATIVE_METHOD(VMDebug, getAllocCount, "(I)I"),
  NATIVE_METHOD(VMDebug, getHeapSpaceStats, "([J)V"),
  NATIVE_METHOD(VMDebug, getInstructionCount, "([I)V"),
  NATIVE_METHOD(VMDebug, getLoadedClassCount, "()I"),
  NATIVE_METHOD(VMDebug, getVmFeatureList, "()[Ljava/lang/String;"),
  NATIVE_METHOD(VMDebug, infopoint, "(I)V"),
  NATIVE_METHOD(VMDebug, isDebuggerConnected, "()Z"),
  NATIVE_METHOD(VMDebug, isDebuggingEnabled, "()Z"),
  NATIVE_METHOD(VMDebug, getMethodTracingMode, "()I"),
  NATIVE_METHOD(VMDebug, lastDebuggerActivity, "()J"),
  NATIVE_METHOD(VMDebug, printLoadedClasses, "(I)V"),
  NATIVE_METHOD(VMDebug, resetAllocCount, "(I)V"),
  NATIVE_METHOD(VMDebug, resetInstructionCount, "()V"),
  NATIVE_METHOD(VMDebug, startAllocCounting, "()V"),
  NATIVE_METHOD(VMDebug, startEmulatorTracing, "()V"),
  NATIVE_METHOD(VMDebug, startInstructionCounting, "()V"),
  NATIVE_METHOD(VMDebug, startMethodTracingDdmsImpl, "(IIZI)V"),
  NATIVE_METHOD(VMDebug, startMethodTracingFd, "(Ljava/lang/String;Ljava/io/FileDescriptor;II)V"),
  NATIVE_METHOD(VMDebug, startMethodTracingFilename, "(Ljava/lang/String;II)V"),
  NATIVE_METHOD(VMDebug, stopAllocCounting, "()V"),
  NATIVE_METHOD(VMDebug, stopEmulatorTracing, "()V"),
  NATIVE_METHOD(VMDebug, stopInstructionCounting, "()V"),
  NATIVE_METHOD(VMDebug, stopMethodTracing, "()V"),
  NATIVE_METHOD(VMDebug, threadCpuTimeNanos, "()J"),
};

void register_dalvik_system_VMDebug(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/VMDebug");
}

}  // namespace art
