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

#include <limits.h>

#include "class_linker.h"
#include "common_throws.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "gc/allocator/dlmalloc.h"
#include "gc/heap.h"
#include "gc/space/dlmalloc_space.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/object.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "scoped_thread_state_change.h"
#include "thread.h"
#include "thread_list.h"
#include "toStringArray.h"

namespace art {

static jfloat VMRuntime_getTargetHeapUtilization(JNIEnv*, jobject) {
  return Runtime::Current()->GetHeap()->GetTargetHeapUtilization();
}

static void VMRuntime_nativeSetTargetHeapUtilization(JNIEnv*, jobject, jfloat target) {
  Runtime::Current()->GetHeap()->SetTargetHeapUtilization(target);
}

static void VMRuntime_startJitCompilation(JNIEnv*, jobject) {
}

static void VMRuntime_disableJitCompilation(JNIEnv*, jobject) {
}

static jobject VMRuntime_newNonMovableArray(JNIEnv* env, jobject, jclass javaElementClass, jint length) {
  ScopedObjectAccess soa(env);
#ifdef MOVING_GARBAGE_COLLECTOR
  // TODO: right now, we don't have a copying collector, so there's no need
  // to do anything special here, but we ought to pass the non-movability
  // through to the allocator.
  UNIMPLEMENTED(FATAL);
#endif

  mirror::Class* element_class = soa.Decode<mirror::Class*>(javaElementClass);
  if (element_class == NULL) {
    ThrowNullPointerException(NULL, "element class == null");
    return NULL;
  }
  if (length < 0) {
    ThrowNegativeArraySizeException(length);
    return NULL;
  }

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  std::string descriptor;
  descriptor += "[";
  descriptor += ClassHelper(element_class).GetDescriptor();
  mirror::Class* array_class = class_linker->FindClass(descriptor.c_str(), NULL);
  mirror::Array* result = mirror::Array::Alloc(soa.Self(), array_class, length);
  return soa.AddLocalReference<jobject>(result);
}

static jlong VMRuntime_addressOf(JNIEnv* env, jobject, jobject javaArray) {
  if (javaArray == NULL) {  // Most likely allocation failed
    return 0;
  }
  ScopedObjectAccess soa(env);
  mirror::Array* array = soa.Decode<mirror::Array*>(javaArray);
  if (!array->IsArrayInstance()) {
    ThrowIllegalArgumentException(NULL, "not an array");
    return 0;
  }
  // TODO: we should also check that this is a non-movable array.
  return reinterpret_cast<uintptr_t>(array->GetRawData(array->GetClass()->GetComponentSize()));
}

static void VMRuntime_clearGrowthLimit(JNIEnv*, jobject) {
  Runtime::Current()->GetHeap()->ClearGrowthLimit();
}

static jboolean VMRuntime_isDebuggerActive(JNIEnv*, jobject) {
  return Dbg::IsDebuggerActive();
}

static jobjectArray VMRuntime_properties(JNIEnv* env, jobject) {
  return toStringArray(env, Runtime::Current()->GetProperties());
}

// This is for backward compatibility with dalvik which returned the
// meaningless "." when no boot classpath or classpath was
// specified. Unfortunately, some tests were using java.class.path to
// lookup relative file locations, so they are counting on this to be
// ".", presumably some applications or libraries could have as well.
static const char* DefaultToDot(const std::string& class_path) {
  return class_path.empty() ? "." : class_path.c_str();
}

static jstring VMRuntime_bootClassPath(JNIEnv* env, jobject) {
  return env->NewStringUTF(DefaultToDot(Runtime::Current()->GetBootClassPathString()));
}

static jstring VMRuntime_classPath(JNIEnv* env, jobject) {
  return env->NewStringUTF(DefaultToDot(Runtime::Current()->GetClassPathString()));
}

static jstring VMRuntime_vmVersion(JNIEnv* env, jobject) {
  return env->NewStringUTF(Runtime::Current()->GetVersion());
}

static jstring VMRuntime_vmLibrary(JNIEnv* env, jobject) {
  return env->NewStringUTF(kIsDebugBuild ? "libartd.so" : "libart.so");
}

static void VMRuntime_setTargetSdkVersion(JNIEnv* env, jobject, jint targetSdkVersion) {
  // This is the target SDK version of the app we're about to run.
  // Note that targetSdkVersion may be CUR_DEVELOPMENT (10000).
  // Note that targetSdkVersion may be 0, meaning "current".
  if (targetSdkVersion > 0 && targetSdkVersion <= 13 /* honeycomb-mr2 */) {
    Runtime* runtime = Runtime::Current();
    JavaVMExt* vm = runtime->GetJavaVM();
    if (vm->check_jni) {
      LOG(INFO) << "CheckJNI enabled: not enabling JNI app bug workarounds.";
    } else {
      LOG(INFO) << "Turning on JNI app bug workarounds for target SDK version "
          << targetSdkVersion << "...";

      vm->work_around_app_jni_bugs = true;
    }
  }
}

static void VMRuntime_registerNativeAllocation(JNIEnv* env, jobject, jint bytes) {
  ScopedObjectAccess soa(env);
  if (bytes < 0) {
    ThrowRuntimeException("allocation size negative %d", bytes);
    return;
  }
  Runtime::Current()->GetHeap()->RegisterNativeAllocation(bytes);
}

static void VMRuntime_registerNativeFree(JNIEnv* env, jobject, jint bytes) {
  ScopedObjectAccess soa(env);
  if (bytes < 0) {
    ThrowRuntimeException("allocation size negative %d", bytes);
    return;
  }
  Runtime::Current()->GetHeap()->RegisterNativeFree(bytes);
}

static void VMRuntime_trimHeap(JNIEnv*, jobject) {
  uint64_t start_ns = NanoTime();

  // Trim the managed heap.
  gc::Heap* heap = Runtime::Current()->GetHeap();
  gc::space::DlMallocSpace* alloc_space = heap->GetAllocSpace();
  size_t alloc_space_size = alloc_space->Size();
  float managed_utilization =
      static_cast<float>(alloc_space->GetBytesAllocated()) / alloc_space_size;
  size_t managed_reclaimed = heap->Trim();

  uint64_t gc_heap_end_ns = NanoTime();

  // Trim the native heap.
  dlmalloc_trim(0);
  size_t native_reclaimed = 0;
  dlmalloc_inspect_all(DlmallocMadviseCallback, &native_reclaimed);

  uint64_t end_ns = NanoTime();

  LOG(INFO) << "Heap trim of managed (duration=" << PrettyDuration(gc_heap_end_ns - start_ns)
      << ", advised=" << PrettySize(managed_reclaimed) << ") and native (duration="
      << PrettyDuration(end_ns - gc_heap_end_ns) << ", advised=" << PrettySize(native_reclaimed)
      << ") heaps. Managed heap utilization of " << static_cast<int>(100 * managed_utilization)
      << "%.";
}

static void VMRuntime_concurrentGC(JNIEnv* env, jobject) {
  Thread* self = static_cast<JNIEnvExt*>(env)->self;
  Runtime::Current()->GetHeap()->ConcurrentGC(self);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(VMRuntime, addressOf, "(Ljava/lang/Object;)J"),
  NATIVE_METHOD(VMRuntime, bootClassPath, "()Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, classPath, "()Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, clearGrowthLimit, "()V"),
  NATIVE_METHOD(VMRuntime, concurrentGC, "()V"),
  NATIVE_METHOD(VMRuntime, disableJitCompilation, "()V"),
  NATIVE_METHOD(VMRuntime, getTargetHeapUtilization, "()F"),
  NATIVE_METHOD(VMRuntime, isDebuggerActive, "()Z"),
  NATIVE_METHOD(VMRuntime, nativeSetTargetHeapUtilization, "(F)V"),
  NATIVE_METHOD(VMRuntime, newNonMovableArray, "(Ljava/lang/Class;I)Ljava/lang/Object;"),
  NATIVE_METHOD(VMRuntime, properties, "()[Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, setTargetSdkVersion, "(I)V"),
  NATIVE_METHOD(VMRuntime, registerNativeAllocation, "(I)V"),
  NATIVE_METHOD(VMRuntime, registerNativeFree, "(I)V"),
  NATIVE_METHOD(VMRuntime, startJitCompilation, "()V"),
  NATIVE_METHOD(VMRuntime, trimHeap, "()V"),
  NATIVE_METHOD(VMRuntime, vmVersion, "()Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, vmLibrary, "()Ljava/lang/String;"),
};

void register_dalvik_system_VMRuntime(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/VMRuntime");
}

}  // namespace art
