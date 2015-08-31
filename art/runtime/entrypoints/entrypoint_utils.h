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

#ifndef ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_
#define ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_
#include "object_utils.h"
#include "class_linker.h"
#include "common_throws.h"
#include "dex_file.h"
#include "indirect_reference_table.h"
#include "invoke_type.h"
#include "jni_internal.h"
#include "mirror/art_method.h"
#include "mirror/array.h"
#include "mirror/class-inl.h"
#include "mirror/throwable.h"

#include "thread.h"

namespace art {

namespace mirror {
  class Class;
  class ArtField;
  class Object;
}  // namespace mirror

// Given the context of a calling Method, use its DexCache to resolve a type to a Class. If it
// cannot be resolved, throw an error. If it can, use it to create an instance.
// When verification/compiler hasn't been able to verify access, optionally perform an access
// check.
static inline mirror::Object* AllocObjectFromCode(uint32_t type_idx, mirror::ArtMethod* method,
                                                  Thread* self,
                                                  bool access_check)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Class* klass = method->GetDexCacheResolvedTypes()->Get(type_idx);
  Runtime* runtime = Runtime::Current();
  if (UNLIKELY(klass == NULL)) {
    klass = runtime->GetClassLinker()->ResolveType(type_idx, method);
    if (klass == NULL) {
      DCHECK(self->IsExceptionPending());
      return NULL;  // Failure
    }
  }
  if (access_check) {
    if (UNLIKELY(!klass->IsInstantiable())) {
      ThrowLocation throw_location = self->GetCurrentLocationForThrow();
      self->ThrowNewException(throw_location, "Ljava/lang/InstantiationError;",
                              PrettyDescriptor(klass).c_str());
      return NULL;  // Failure
    }
    mirror::Class* referrer = method->GetDeclaringClass();
    if (UNLIKELY(!referrer->CanAccess(klass))) {
      ThrowIllegalAccessErrorClass(referrer, klass);
      return NULL;  // Failure
    }
  }
  if (!klass->IsInitialized() &&
      !runtime->GetClassLinker()->EnsureInitialized(klass, true, true)) {
    DCHECK(self->IsExceptionPending());
    return NULL;  // Failure
  }
  return klass->AllocObject(self);
}

// Given the context of a calling Method, use its DexCache to resolve a type to an array Class. If
// it cannot be resolved, throw an error. If it can, use it to create an array.
// When verification/compiler hasn't been able to verify access, optionally perform an access
// check.
static inline mirror::Array* AllocArrayFromCode(uint32_t type_idx, mirror::ArtMethod* method,
                                                int32_t component_count,
                                                Thread* self, bool access_check)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (UNLIKELY(component_count < 0)) {
    ThrowNegativeArraySizeException(component_count);
    return NULL;  // Failure
  }
  mirror::Class* klass = method->GetDexCacheResolvedTypes()->Get(type_idx);
  if (UNLIKELY(klass == NULL)) {  // Not in dex cache so try to resolve
    klass = Runtime::Current()->GetClassLinker()->ResolveType(type_idx, method);
    if (klass == NULL) {  // Error
      DCHECK(Thread::Current()->IsExceptionPending());
      return NULL;  // Failure
    }
    CHECK(klass->IsArrayClass()) << PrettyClass(klass);
  }
  if (access_check) {
    mirror::Class* referrer = method->GetDeclaringClass();
    if (UNLIKELY(!referrer->CanAccess(klass))) {
      ThrowIllegalAccessErrorClass(referrer, klass);
      return NULL;  // Failure
    }
  }
  return mirror::Array::Alloc(self, klass, component_count);
}

extern mirror::Array* CheckAndAllocArrayFromCode(uint32_t type_idx, mirror::ArtMethod* method,
                                                 int32_t component_count,
                                                 Thread* self, bool access_check)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Type of find field operation for fast and slow case.
enum FindFieldType {
  InstanceObjectRead,
  InstanceObjectWrite,
  InstancePrimitiveRead,
  InstancePrimitiveWrite,
  StaticObjectRead,
  StaticObjectWrite,
  StaticPrimitiveRead,
  StaticPrimitiveWrite,
};

// Slow field find that can initialize classes and may throw exceptions.
extern mirror::ArtField* FindFieldFromCode(uint32_t field_idx, const mirror::ArtMethod* referrer,
                                           Thread* self, FindFieldType type, size_t expected_size,
                                           bool access_check)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Fast path field resolution that can't initialize classes or throw exceptions.
static inline mirror::ArtField* FindFieldFast(uint32_t field_idx,
                                              const mirror::ArtMethod* referrer,
                                              FindFieldType type, size_t expected_size)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* resolved_field =
      referrer->GetDeclaringClass()->GetDexCache()->GetResolvedField(field_idx);
  if (UNLIKELY(resolved_field == NULL)) {
    return NULL;
  }
  mirror::Class* fields_class = resolved_field->GetDeclaringClass();
  // Check class is initiliazed or initializing.
  if (UNLIKELY(!fields_class->IsInitializing())) {
    return NULL;
  }
  // Check for incompatible class change.
  bool is_primitive;
  bool is_set;
  bool is_static;
  switch (type) {
    case InstanceObjectRead:     is_primitive = false; is_set = false; is_static = false; break;
    case InstanceObjectWrite:    is_primitive = false; is_set = true;  is_static = false; break;
    case InstancePrimitiveRead:  is_primitive = true;  is_set = false; is_static = false; break;
    case InstancePrimitiveWrite: is_primitive = true;  is_set = true;  is_static = false; break;
    case StaticObjectRead:       is_primitive = false; is_set = false; is_static = true;  break;
    case StaticObjectWrite:      is_primitive = false; is_set = true;  is_static = true;  break;
    case StaticPrimitiveRead:    is_primitive = true;  is_set = false; is_static = true;  break;
    case StaticPrimitiveWrite:   is_primitive = true;  is_set = true;  is_static = true;  break;
    default:
      LOG(FATAL) << "UNREACHABLE";  // Assignment below to avoid GCC warnings.
      is_primitive = true;
      is_set = true;
      is_static = true;
      break;
  }
  if (UNLIKELY(resolved_field->IsStatic() != is_static)) {
    // Incompatible class change.
    return NULL;
  }
  mirror::Class* referring_class = referrer->GetDeclaringClass();
  if (UNLIKELY(!referring_class->CanAccess(fields_class) ||
               !referring_class->CanAccessMember(fields_class,
                                                 resolved_field->GetAccessFlags()) ||
               (is_set && resolved_field->IsFinal() && (fields_class != referring_class)))) {
    // Illegal access.
    return NULL;
  }
  FieldHelper fh(resolved_field);
  if (UNLIKELY(fh.IsPrimitiveType() != is_primitive ||
               fh.FieldSize() != expected_size)) {
    return NULL;
  }
  return resolved_field;
}

// Fast path method resolution that can't throw exceptions.
static inline mirror::ArtMethod* FindMethodFast(uint32_t method_idx,
                                                mirror::Object* this_object,
                                                const mirror::ArtMethod* referrer,
                                                bool access_check, InvokeType type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  bool is_direct = type == kStatic || type == kDirect;
  if (UNLIKELY(this_object == NULL && !is_direct)) {
    return NULL;
  }
  mirror::ArtMethod* resolved_method =
      referrer->GetDeclaringClass()->GetDexCache()->GetResolvedMethod(method_idx);
  if (UNLIKELY(resolved_method == NULL)) {
    return NULL;
  }
  if (access_check) {
    // Check for incompatible class change errors and access.
    bool icce = resolved_method->CheckIncompatibleClassChange(type);
    if (UNLIKELY(icce)) {
      return NULL;
    }
    mirror::Class* methods_class = resolved_method->GetDeclaringClass();
    mirror::Class* referring_class = referrer->GetDeclaringClass();
    if (UNLIKELY(!referring_class->CanAccess(methods_class) ||
                 !referring_class->CanAccessMember(methods_class,
                                                   resolved_method->GetAccessFlags()))) {
      // Potential illegal access, may need to refine the method's class.
      return NULL;
    }
  }
  if (type == kInterface) {  // Most common form of slow path dispatch.
    return this_object->GetClass()->FindVirtualMethodForInterface(resolved_method);
  } else if (is_direct) {
    return resolved_method;
  } else if (type == kSuper) {
    return referrer->GetDeclaringClass()->GetSuperClass()->GetVTable()->
        Get(resolved_method->GetMethodIndex());
  } else {
    DCHECK(type == kVirtual);
    return this_object->GetClass()->GetVTable()->Get(resolved_method->GetMethodIndex());
  }
}

extern mirror::ArtMethod* FindMethodFromCode(uint32_t method_idx, mirror::Object* this_object,
                                             mirror::ArtMethod* referrer,
                                             Thread* self, bool access_check, InvokeType type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

static inline mirror::Class* ResolveVerifyAndClinit(uint32_t type_idx,
                                                    const mirror::ArtMethod* referrer,
                                                    Thread* self, bool can_run_clinit,
                                                    bool verify_access)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  mirror::Class* klass = class_linker->ResolveType(type_idx, referrer);
  if (UNLIKELY(klass == NULL)) {
    CHECK(self->IsExceptionPending());
    return NULL;  // Failure - Indicate to caller to deliver exception
  }
  // Perform access check if necessary.
  mirror::Class* referring_class = referrer->GetDeclaringClass();
  if (verify_access && UNLIKELY(!referring_class->CanAccess(klass))) {
    ThrowIllegalAccessErrorClass(referring_class, klass);
    return NULL;  // Failure - Indicate to caller to deliver exception
  }
  // If we're just implementing const-class, we shouldn't call <clinit>.
  if (!can_run_clinit) {
    return klass;
  }
  // If we are the <clinit> of this class, just return our storage.
  //
  // Do not set the DexCache InitializedStaticStorage, since that implies <clinit> has finished
  // running.
  if (klass == referring_class && MethodHelper(referrer).IsClassInitializer()) {
    return klass;
  }
  if (!class_linker->EnsureInitialized(klass, true, true)) {
    CHECK(self->IsExceptionPending());
    return NULL;  // Failure - Indicate to caller to deliver exception
  }
  referrer->GetDexCacheInitializedStaticStorage()->Set(type_idx, klass);
  return klass;
}

extern void ThrowStackOverflowError(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

static inline mirror::String* ResolveStringFromCode(const mirror::ArtMethod* referrer,
                                                    uint32_t string_idx)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  return class_linker->ResolveString(string_idx, referrer);
}

static inline void UnlockJniSynchronizedMethod(jobject locked, Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
    UNLOCK_FUNCTION(monitor_lock_) {
  // Save any pending exception over monitor exit call.
  mirror::Throwable* saved_exception = NULL;
  ThrowLocation saved_throw_location;
  if (UNLIKELY(self->IsExceptionPending())) {
    saved_exception = self->GetException(&saved_throw_location);
    self->ClearException();
  }
  // Decode locked object and unlock, before popping local references.
  self->DecodeJObject(locked)->MonitorExit(self);
  if (UNLIKELY(self->IsExceptionPending())) {
    LOG(FATAL) << "Synchronized JNI code returning with an exception:\n"
        << saved_exception->Dump()
        << "\nEncountered second exception during implicit MonitorExit:\n"
        << self->GetException(NULL)->Dump();
  }
  // Restore pending exception.
  if (saved_exception != NULL) {
    self->SetException(saved_throw_location, saved_exception);
  }
}

static inline void CheckReferenceResult(mirror::Object* o, Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (o == NULL) {
    return;
  }
  mirror::ArtMethod* m = self->GetCurrentMethod(NULL);
  if (o == kInvalidIndirectRefObject) {
    JniAbortF(NULL, "invalid reference returned from %s", PrettyMethod(m).c_str());
  }
  // Make sure that the result is an instance of the type this method was expected to return.
  mirror::Class* return_type = MethodHelper(m).GetReturnType();

  if (!o->InstanceOf(return_type)) {
    JniAbortF(NULL, "attempt to return an instance of %s from %s",
              PrettyTypeOf(o).c_str(), PrettyMethod(m).c_str());
  }
}

static inline void CheckSuspend(Thread* thread) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  for (;;) {
    if (thread->ReadFlag(kCheckpointRequest)) {
      thread->RunCheckpointFunction();
      thread->AtomicClearFlag(kCheckpointRequest);
    } else if (thread->ReadFlag(kSuspendRequest)) {
      thread->FullSuspendCheck();
    } else {
      break;
    }
  }
}

JValue InvokeProxyInvocationHandler(ScopedObjectAccessUnchecked& soa, const char* shorty,
                                    jobject rcvr_jobj, jobject interface_art_method_jobj,
                                    std::vector<jvalue>& args)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Entry point for deoptimization.
extern "C" void art_quick_deoptimize();
static inline uintptr_t GetQuickDeoptimizationEntryPoint() {
  return reinterpret_cast<uintptr_t>(art_quick_deoptimize);
}

// Return address of instrumentation stub.
extern "C" void art_quick_instrumentation_entry(void*);
static inline void* GetQuickInstrumentationEntryPoint() {
  return reinterpret_cast<void*>(art_quick_instrumentation_entry);
}

// The return_pc of instrumentation exit stub.
extern "C" void art_quick_instrumentation_exit();
static inline uintptr_t GetQuickInstrumentationExitPc() {
  return reinterpret_cast<uintptr_t>(art_quick_instrumentation_exit);
}

extern "C" void art_portable_to_interpreter_bridge(mirror::ArtMethod*);
static inline const void* GetPortableToInterpreterBridge() {
  return reinterpret_cast<void*>(art_portable_to_interpreter_bridge);
}

extern "C" void art_quick_to_interpreter_bridge(mirror::ArtMethod*);
static inline const void* GetQuickToInterpreterBridge() {
  return reinterpret_cast<void*>(art_quick_to_interpreter_bridge);
}

// Return address of interpreter stub.
static inline const void* GetCompiledCodeToInterpreterBridge() {
#if defined(ART_USE_PORTABLE_COMPILER)
  return GetPortableToInterpreterBridge();
#else
  return GetQuickToInterpreterBridge();
#endif
}


static inline const void* GetPortableResolutionTrampoline(ClassLinker* class_linker) {
  return class_linker->GetPortableResolutionTrampoline();
}

static inline const void* GetQuickResolutionTrampoline(ClassLinker* class_linker) {
  return class_linker->GetQuickResolutionTrampoline();
}

// Return address of resolution trampoline stub for defined compiler.
static inline const void* GetResolutionTrampoline(ClassLinker* class_linker) {
#if defined(ART_USE_PORTABLE_COMPILER)
  return GetPortableResolutionTrampoline(class_linker);
#else
  return GetQuickResolutionTrampoline(class_linker);
#endif
}

extern "C" void art_portable_proxy_invoke_handler();
static inline const void* GetPortableProxyInvokeHandler() {
  return reinterpret_cast<void*>(art_portable_proxy_invoke_handler);
}

extern "C" void art_quick_proxy_invoke_handler();
static inline const void* GetQuickProxyInvokeHandler() {
  return reinterpret_cast<void*>(art_quick_proxy_invoke_handler);
}

static inline const void* GetProxyInvokeHandler() {
#if defined(ART_USE_PORTABLE_COMPILER)
  return GetPortableProxyInvokeHandler();
#else
  return GetQuickProxyInvokeHandler();
#endif
}

extern "C" void* art_jni_dlsym_lookup_stub(JNIEnv*, jobject);
static inline void* GetJniDlsymLookupStub() {
  return reinterpret_cast<void*>(art_jni_dlsym_lookup_stub);
}

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_
