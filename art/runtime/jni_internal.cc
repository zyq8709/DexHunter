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

#include "jni_internal.h"

#include <dlfcn.h>

#include <cstdarg>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/stl_util.h"
#include "base/stringpiece.h"
#include "class_linker.h"
#include "dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "interpreter/interpreter.h"
#include "invoke_arg_array_builder.h"
#include "jni.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/throwable.h"
#include "object_utils.h"
#include "runtime.h"
#include "safe_map.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "thread.h"
#include "utf.h"
#include "UniquePtr.h"
#include "well_known_classes.h"

using ::art::mirror::ArtField;
using ::art::mirror::ArtMethod;
using ::art::mirror::Array;
using ::art::mirror::BooleanArray;
using ::art::mirror::ByteArray;
using ::art::mirror::CharArray;
using ::art::mirror::Class;
using ::art::mirror::ClassLoader;
using ::art::mirror::DoubleArray;
using ::art::mirror::FloatArray;
using ::art::mirror::IntArray;
using ::art::mirror::LongArray;
using ::art::mirror::Object;
using ::art::mirror::ObjectArray;
using ::art::mirror::ShortArray;
using ::art::mirror::String;
using ::art::mirror::Throwable;

namespace art {

static const size_t kMonitorsInitial = 32;  // Arbitrary.
static const size_t kMonitorsMax = 4096;  // Arbitrary sanity check.

static const size_t kLocalsInitial = 64;  // Arbitrary.
static const size_t kLocalsMax = 512;  // Arbitrary sanity check.

static const size_t kPinTableInitial = 16;  // Arbitrary.
static const size_t kPinTableMax = 1024;  // Arbitrary sanity check.

static size_t gGlobalsInitial = 512;  // Arbitrary.
static size_t gGlobalsMax = 51200;  // Arbitrary sanity check. (Must fit in 16 bits.)

static const size_t kWeakGlobalsInitial = 16;  // Arbitrary.
static const size_t kWeakGlobalsMax = 51200;  // Arbitrary sanity check. (Must fit in 16 bits.)

static jweak AddWeakGlobalReference(ScopedObjectAccess& soa, Object* obj)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return soa.Vm()->AddWeakGlobalReference(soa.Self(), obj);
}

static bool IsBadJniVersion(int version) {
  // We don't support JNI_VERSION_1_1. These are the only other valid versions.
  return version != JNI_VERSION_1_2 && version != JNI_VERSION_1_4 && version != JNI_VERSION_1_6;
}

static void CheckMethodArguments(ArtMethod* m, uint32_t* args)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  MethodHelper mh(m);
  const DexFile::TypeList* params = mh.GetParameterTypeList();
  if (params == NULL) {
    return;  // No arguments so nothing to check.
  }
  uint32_t offset = 0;
  uint32_t num_params = params->Size();
  size_t error_count = 0;
  if (!m->IsStatic()) {
    offset = 1;
  }
  for (uint32_t i = 0; i < num_params; i++) {
    uint16_t type_idx = params->GetTypeItem(i).type_idx_;
    Class* param_type = mh.GetClassFromTypeIdx(type_idx);
    if (param_type == NULL) {
      Thread* self = Thread::Current();
      CHECK(self->IsExceptionPending());
      LOG(ERROR) << "Internal error: unresolvable type for argument type in JNI invoke: "
          << mh.GetTypeDescriptorFromTypeIdx(type_idx) << "\n"
          << self->GetException(NULL)->Dump();
      self->ClearException();
      ++error_count;
    } else if (!param_type->IsPrimitive()) {
      // TODO: check primitives are in range.
      Object* argument = reinterpret_cast<Object*>(args[i + offset]);
      if (argument != NULL && !argument->InstanceOf(param_type)) {
        LOG(ERROR) << "JNI ERROR (app bug): attempt to pass an instance of "
                   << PrettyTypeOf(argument) << " as argument " << (i + 1) << " to " << PrettyMethod(m);
        ++error_count;
      }
    } else if (param_type->IsPrimitiveLong() || param_type->IsPrimitiveDouble()) {
      offset++;
    }
  }
  if (error_count > 0) {
    // TODO: pass the JNI function name (such as "CallVoidMethodV") through so we can call JniAbort
    // with an argument.
    JniAbortF(NULL, "bad arguments passed to %s (see above for details)", PrettyMethod(m).c_str());
  }
}

void InvokeWithArgArray(const ScopedObjectAccess& soa, ArtMethod* method,
                        ArgArray* arg_array, JValue* result, char result_type)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t* args = arg_array->GetArray();
  if (UNLIKELY(soa.Env()->check_jni)) {
    CheckMethodArguments(method, args);
  }
  method->Invoke(soa.Self(), args, arg_array->GetNumBytes(), result, result_type);
}

static JValue InvokeWithVarArgs(const ScopedObjectAccess& soa, jobject obj,
                                jmethodID mid, va_list args)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ArtMethod* method = soa.DecodeMethod(mid);
  Object* receiver = method->IsStatic() ? NULL : soa.Decode<Object*>(obj);
  MethodHelper mh(method);
  JValue result;
  ArgArray arg_array(mh.GetShorty(), mh.GetShortyLength());
  arg_array.BuildArgArray(soa, receiver, args);
  InvokeWithArgArray(soa, method, &arg_array, &result, mh.GetShorty()[0]);
  return result;
}

static ArtMethod* FindVirtualMethod(Object* receiver, ArtMethod* method)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return receiver->GetClass()->FindVirtualMethodForVirtualOrInterface(method);
}

static JValue InvokeVirtualOrInterfaceWithJValues(const ScopedObjectAccess& soa,
                                                  jobject obj, jmethodID mid, jvalue* args)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Object* receiver = soa.Decode<Object*>(obj);
  ArtMethod* method = FindVirtualMethod(receiver, soa.DecodeMethod(mid));
  MethodHelper mh(method);
  JValue result;
  ArgArray arg_array(mh.GetShorty(), mh.GetShortyLength());
  arg_array.BuildArgArray(soa, receiver, args);
  InvokeWithArgArray(soa, method, &arg_array, &result, mh.GetShorty()[0]);
  return result;
}

static JValue InvokeVirtualOrInterfaceWithVarArgs(const ScopedObjectAccess& soa,
                                                  jobject obj, jmethodID mid, va_list args)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Object* receiver = soa.Decode<Object*>(obj);
  ArtMethod* method = FindVirtualMethod(receiver, soa.DecodeMethod(mid));
  MethodHelper mh(method);
  JValue result;
  ArgArray arg_array(mh.GetShorty(), mh.GetShortyLength());
  arg_array.BuildArgArray(soa, receiver, args);
  InvokeWithArgArray(soa, method, &arg_array, &result, mh.GetShorty()[0]);
  return result;
}

// Section 12.3.2 of the JNI spec describes JNI class descriptors. They're
// separated with slashes but aren't wrapped with "L;" like regular descriptors
// (i.e. "a/b/C" rather than "La/b/C;"). Arrays of reference types are an
// exception; there the "L;" must be present ("[La/b/C;"). Historically we've
// supported names with dots too (such as "a.b.C").
static std::string NormalizeJniClassDescriptor(const char* name) {
  std::string result;
  // Add the missing "L;" if necessary.
  if (name[0] == '[') {
    result = name;
  } else {
    result += 'L';
    result += name;
    result += ';';
  }
  // Rewrite '.' as '/' for backwards compatibility.
  if (result.find('.') != std::string::npos) {
    LOG(WARNING) << "Call to JNI FindClass with dots in name: "
                 << "\"" << name << "\"";
    std::replace(result.begin(), result.end(), '.', '/');
  }
  return result;
}

static void ThrowNoSuchMethodError(ScopedObjectAccess& soa, Class* c,
                                   const char* name, const char* sig, const char* kind)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
  soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/NoSuchMethodError;",
                                 "no %s method \"%s.%s%s\"",
                                 kind, ClassHelper(c).GetDescriptor(), name, sig);
}

static jmethodID FindMethodID(ScopedObjectAccess& soa, jclass jni_class,
                              const char* name, const char* sig, bool is_static)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Class* c = soa.Decode<Class*>(jni_class);
  if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(c, true, true)) {
    return NULL;
  }

  ArtMethod* method = NULL;
  if (is_static) {
    method = c->FindDirectMethod(name, sig);
  } else {
    method = c->FindVirtualMethod(name, sig);
    if (method == NULL) {
      // No virtual method matching the signature.  Search declared
      // private methods and constructors.
      method = c->FindDeclaredDirectMethod(name, sig);
    }
  }

  if (method == NULL || method->IsStatic() != is_static) {
    ThrowNoSuchMethodError(soa, c, name, sig, is_static ? "static" : "non-static");
    return NULL;
  }

  return soa.EncodeMethod(method);
}

static ClassLoader* GetClassLoader(const ScopedObjectAccess& soa)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ArtMethod* method = soa.Self()->GetCurrentMethod(NULL);
  // If we are running Runtime.nativeLoad, use the overriding ClassLoader it set.
  if (method == soa.DecodeMethod(WellKnownClasses::java_lang_Runtime_nativeLoad)) {
    return soa.Self()->GetClassLoaderOverride();
  }
  // If we have a method, use its ClassLoader for context.
  if (method != NULL) {
    return method->GetDeclaringClass()->GetClassLoader();
  }
  // We don't have a method, so try to use the system ClassLoader.
  ClassLoader* class_loader = soa.Decode<ClassLoader*>(Runtime::Current()->GetSystemClassLoader());
  if (class_loader != NULL) {
    return class_loader;
  }
  // See if the override ClassLoader is set for gtests.
  class_loader = soa.Self()->GetClassLoaderOverride();
  if (class_loader != NULL) {
    // If so, CommonTest should have set UseCompileTimeClassPath.
    CHECK(Runtime::Current()->UseCompileTimeClassPath());
    return class_loader;
  }
  // Use the BOOTCLASSPATH.
  return NULL;
}

static jfieldID FindFieldID(const ScopedObjectAccess& soa, jclass jni_class, const char* name,
                            const char* sig, bool is_static)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Class* c = soa.Decode<Class*>(jni_class);
  if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(c, true, true)) {
    return NULL;
  }

  ArtField* field = NULL;
  Class* field_type;
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  if (sig[1] != '\0') {
    SirtRef<mirror::ClassLoader> class_loader(soa.Self(), c->GetClassLoader());
    field_type = class_linker->FindClass(sig, class_loader.get());
  } else {
    field_type = class_linker->FindPrimitiveClass(*sig);
  }
  if (field_type == NULL) {
    // Failed to find type from the signature of the field.
    DCHECK(soa.Self()->IsExceptionPending());
    ThrowLocation throw_location;
    SirtRef<Throwable> cause(soa.Self(), soa.Self()->GetException(&throw_location));
    soa.Self()->ClearException();
    soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/NoSuchFieldError;",
                                   "no type \"%s\" found and so no field \"%s\" could be found in class "
                                   "\"%s\" or its superclasses", sig, name,
                                   ClassHelper(c).GetDescriptor());
    soa.Self()->GetException(NULL)->SetCause(cause.get());
    return NULL;
  }
  if (is_static) {
    field = c->FindStaticField(name, ClassHelper(field_type).GetDescriptor());
  } else {
    field = c->FindInstanceField(name, ClassHelper(field_type).GetDescriptor());
  }
  if (field == NULL) {
    ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
    soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/NoSuchFieldError;",
                                   "no \"%s\" field \"%s\" in class \"%s\" or its superclasses",
                                   sig, name, ClassHelper(c).GetDescriptor());
    return NULL;
  }
  return soa.EncodeField(field);
}

static void PinPrimitiveArray(const ScopedObjectAccess& soa, const Array* array)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  JavaVMExt* vm = soa.Vm();
  MutexLock mu(soa.Self(), vm->pins_lock);
  vm->pin_table.Add(array);
}

static void UnpinPrimitiveArray(const ScopedObjectAccess& soa, const Array* array)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  JavaVMExt* vm = soa.Vm();
  MutexLock mu(soa.Self(), vm->pins_lock);
  vm->pin_table.Remove(array);
}

static void ThrowAIOOBE(ScopedObjectAccess& soa, Array* array, jsize start,
                        jsize length, const char* identifier)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::string type(PrettyTypeOf(array));
  ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
  soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/ArrayIndexOutOfBoundsException;",
                                 "%s offset=%d length=%d %s.length=%d",
                                 type.c_str(), start, length, identifier, array->GetLength());
}

static void ThrowSIOOBE(ScopedObjectAccess& soa, jsize start, jsize length,
                        jsize array_length)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
  soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/StringIndexOutOfBoundsException;",
                                 "offset=%d length=%d string.length()=%d", start, length,
                                 array_length);
}

int ThrowNewException(JNIEnv* env, jclass exception_class, const char* msg, jobject cause)
    LOCKS_EXCLUDED(Locks::mutator_lock_) {
  // Turn the const char* into a java.lang.String.
  ScopedLocalRef<jstring> s(env, env->NewStringUTF(msg));
  if (msg != NULL && s.get() == NULL) {
    return JNI_ERR;
  }

  // Choose an appropriate constructor and set up the arguments.
  jvalue args[2];
  const char* signature;
  if (msg == NULL && cause == NULL) {
    signature = "()V";
  } else if (msg != NULL && cause == NULL) {
    signature = "(Ljava/lang/String;)V";
    args[0].l = s.get();
  } else if (msg == NULL && cause != NULL) {
    signature = "(Ljava/lang/Throwable;)V";
    args[0].l = cause;
  } else {
    signature = "(Ljava/lang/String;Ljava/lang/Throwable;)V";
    args[0].l = s.get();
    args[1].l = cause;
  }
  jmethodID mid = env->GetMethodID(exception_class, "<init>", signature);
  if (mid == NULL) {
    ScopedObjectAccess soa(env);
    LOG(ERROR) << "No <init>" << signature << " in "
        << PrettyClass(soa.Decode<Class*>(exception_class));
    return JNI_ERR;
  }

  ScopedLocalRef<jthrowable> exception(env, reinterpret_cast<jthrowable>(env->NewObjectA(exception_class, mid, args)));
  if (exception.get() == NULL) {
    return JNI_ERR;
  }
  ScopedObjectAccess soa(env);
  ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
  soa.Self()->SetException(throw_location, soa.Decode<Throwable*>(exception.get()));
  return JNI_OK;
}

static jint JII_AttachCurrentThread(JavaVM* vm, JNIEnv** p_env, void* raw_args, bool as_daemon) {
  if (vm == NULL || p_env == NULL) {
    return JNI_ERR;
  }

  // Return immediately if we're already attached.
  Thread* self = Thread::Current();
  if (self != NULL) {
    *p_env = self->GetJniEnv();
    return JNI_OK;
  }

  Runtime* runtime = reinterpret_cast<JavaVMExt*>(vm)->runtime;

  // No threads allowed in zygote mode.
  if (runtime->IsZygote()) {
    LOG(ERROR) << "Attempt to attach a thread in the zygote";
    return JNI_ERR;
  }

  JavaVMAttachArgs* args = static_cast<JavaVMAttachArgs*>(raw_args);
  const char* thread_name = NULL;
  jobject thread_group = NULL;
  if (args != NULL) {
    if (IsBadJniVersion(args->version)) {
      LOG(ERROR) << "Bad JNI version passed to "
                 << (as_daemon ? "AttachCurrentThreadAsDaemon" : "AttachCurrentThread") << ": "
                 << args->version;
      return JNI_EVERSION;
    }
    thread_name = args->name;
    thread_group = args->group;
  }

  if (!runtime->AttachCurrentThread(thread_name, as_daemon, thread_group, !runtime->IsCompiler())) {
    *p_env = NULL;
    return JNI_ERR;
  } else {
    *p_env = Thread::Current()->GetJniEnv();
    return JNI_OK;
  }
}

class SharedLibrary {
 public:
  SharedLibrary(const std::string& path, void* handle, Object* class_loader)
      : path_(path),
        handle_(handle),
        class_loader_(class_loader),
        jni_on_load_lock_("JNI_OnLoad lock"),
        jni_on_load_cond_("JNI_OnLoad condition variable", jni_on_load_lock_),
        jni_on_load_thread_id_(Thread::Current()->GetThinLockId()),
        jni_on_load_result_(kPending) {
  }

  Object* GetClassLoader() {
    return class_loader_;
  }

  std::string GetPath() {
    return path_;
  }

  /*
   * Check the result of an earlier call to JNI_OnLoad on this library.
   * If the call has not yet finished in another thread, wait for it.
   */
  bool CheckOnLoadResult()
      LOCKS_EXCLUDED(jni_on_load_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Thread* self = Thread::Current();
    self->TransitionFromRunnableToSuspended(kWaitingForJniOnLoad);
    bool okay;
    {
      MutexLock mu(self, jni_on_load_lock_);

      if (jni_on_load_thread_id_ == self->GetThinLockId()) {
        // Check this so we don't end up waiting for ourselves.  We need to return "true" so the
        // caller can continue.
        LOG(INFO) << *self << " recursive attempt to load library " << "\"" << path_ << "\"";
        okay = true;
      } else {
        while (jni_on_load_result_ == kPending) {
          VLOG(jni) << "[" << *self << " waiting for \"" << path_ << "\" " << "JNI_OnLoad...]";
          jni_on_load_cond_.Wait(self);
        }

        okay = (jni_on_load_result_ == kOkay);
        VLOG(jni) << "[Earlier JNI_OnLoad for \"" << path_ << "\" "
            << (okay ? "succeeded" : "failed") << "]";
      }
    }
    self->TransitionFromSuspendedToRunnable();
    return okay;
  }

  void SetResult(bool result) LOCKS_EXCLUDED(jni_on_load_lock_) {
    Thread* self = Thread::Current();
    MutexLock mu(self, jni_on_load_lock_);

    jni_on_load_result_ = result ? kOkay : kFailed;
    jni_on_load_thread_id_ = 0;

    // Broadcast a wakeup to anybody sleeping on the condition variable.
    jni_on_load_cond_.Broadcast(self);
  }

  void* FindSymbol(const std::string& symbol_name) {
    return dlsym(handle_, symbol_name.c_str());
  }

 private:
  enum JNI_OnLoadState {
    kPending,
    kFailed,
    kOkay,
  };

  // Path to library "/system/lib/libjni.so".
  std::string path_;

  // The void* returned by dlopen(3).
  void* handle_;

  // The ClassLoader this library is associated with.
  Object* class_loader_;

  // Guards remaining items.
  Mutex jni_on_load_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  // Wait for JNI_OnLoad in other thread.
  ConditionVariable jni_on_load_cond_ GUARDED_BY(jni_on_load_lock_);
  // Recursive invocation guard.
  uint32_t jni_on_load_thread_id_ GUARDED_BY(jni_on_load_lock_);
  // Result of earlier JNI_OnLoad call.
  JNI_OnLoadState jni_on_load_result_ GUARDED_BY(jni_on_load_lock_);
};

// This exists mainly to keep implementation details out of the header file.
class Libraries {
 public:
  Libraries() {
  }

  ~Libraries() {
    STLDeleteValues(&libraries_);
  }

  void Dump(std::ostream& os) const {
    bool first = true;
    for (const auto& library : libraries_) {
      if (!first) {
        os << ' ';
      }
      first = false;
      os << library.first;
    }
  }

  size_t size() const {
    return libraries_.size();
  }

  SharedLibrary* Get(const std::string& path) {
    auto it = libraries_.find(path);
    return (it == libraries_.end()) ? NULL : it->second;
  }

  void Put(const std::string& path, SharedLibrary* library) {
    libraries_.Put(path, library);
  }

  // See section 11.3 "Linking Native Methods" of the JNI spec.
  void* FindNativeMethod(const ArtMethod* m, std::string& detail)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    std::string jni_short_name(JniShortName(m));
    std::string jni_long_name(JniLongName(m));
    const ClassLoader* declaring_class_loader = m->GetDeclaringClass()->GetClassLoader();
    for (const auto& lib : libraries_) {
      SharedLibrary* library = lib.second;
      if (library->GetClassLoader() != declaring_class_loader) {
        // We only search libraries loaded by the appropriate ClassLoader.
        continue;
      }
      // Try the short name then the long name...
      void* fn = library->FindSymbol(jni_short_name);
      if (fn == NULL) {
        fn = library->FindSymbol(jni_long_name);
      }
      if (fn != NULL) {
        VLOG(jni) << "[Found native code for " << PrettyMethod(m)
                  << " in \"" << library->GetPath() << "\"]";
        return fn;
      }
    }
    detail += "No implementation found for ";
    detail += PrettyMethod(m);
    detail += " (tried " + jni_short_name + " and " + jni_long_name + ")";
    LOG(ERROR) << detail;
    return NULL;
  }

 private:
  SafeMap<std::string, SharedLibrary*> libraries_;
};

JValue InvokeWithJValues(const ScopedObjectAccess& soa, jobject obj, jmethodID mid,
                         jvalue* args) {
  ArtMethod* method = soa.DecodeMethod(mid);
  Object* receiver = method->IsStatic() ? NULL : soa.Decode<Object*>(obj);
  MethodHelper mh(method);
  JValue result;
  ArgArray arg_array(mh.GetShorty(), mh.GetShortyLength());
  arg_array.BuildArgArray(soa, receiver, args);
  InvokeWithArgArray(soa, method, &arg_array, &result, mh.GetShorty()[0]);
  return result;
}

#define CHECK_NON_NULL_ARGUMENT(fn, value) \
  if (UNLIKELY(value == NULL)) { \
    JniAbortF(#fn, #value " == null"); \
  }

#define CHECK_NON_NULL_MEMCPY_ARGUMENT(fn, length, value) \
  if (UNLIKELY(length != 0 && value == NULL)) { \
    JniAbortF(#fn, #value " == null"); \
  }

class JNI {
 public:
  static jint GetVersion(JNIEnv*) {
    return JNI_VERSION_1_6;
  }

  static jclass DefineClass(JNIEnv*, const char*, jobject, const jbyte*, jsize) {
    LOG(WARNING) << "JNI DefineClass is not supported";
    return NULL;
  }

  static jclass FindClass(JNIEnv* env, const char* name) {
    CHECK_NON_NULL_ARGUMENT(FindClass, name);
    Runtime* runtime = Runtime::Current();
    ClassLinker* class_linker = runtime->GetClassLinker();
    std::string descriptor(NormalizeJniClassDescriptor(name));
    ScopedObjectAccess soa(env);
    Class* c = NULL;
    if (runtime->IsStarted()) {
      ClassLoader* cl = GetClassLoader(soa);
      c = class_linker->FindClass(descriptor.c_str(), cl);
    } else {
      c = class_linker->FindSystemClass(descriptor.c_str());
    }
    return soa.AddLocalReference<jclass>(c);
  }

  static jmethodID FromReflectedMethod(JNIEnv* env, jobject java_method) {
    CHECK_NON_NULL_ARGUMENT(FromReflectedMethod, java_method);
    ScopedObjectAccess soa(env);
    jobject art_method = env->GetObjectField(java_method,
                                             WellKnownClasses::java_lang_reflect_AbstractMethod_artMethod);
    ArtMethod* method = soa.Decode<ArtMethod*>(art_method);
    DCHECK(method != NULL);
    return soa.EncodeMethod(method);
  }

  static jfieldID FromReflectedField(JNIEnv* env, jobject java_field) {
    CHECK_NON_NULL_ARGUMENT(FromReflectedField, java_field);
    ScopedObjectAccess soa(env);
    jobject art_field = env->GetObjectField(java_field,
                                            WellKnownClasses::java_lang_reflect_Field_artField);
    ArtField* field = soa.Decode<ArtField*>(art_field);
    DCHECK(field != NULL);
    return soa.EncodeField(field);
  }

  static jobject ToReflectedMethod(JNIEnv* env, jclass, jmethodID mid, jboolean) {
    CHECK_NON_NULL_ARGUMENT(ToReflectedMethod, mid);
    ScopedObjectAccess soa(env);
    ArtMethod* m = soa.DecodeMethod(mid);
    jobject art_method = soa.AddLocalReference<jobject>(m);
    jobject reflect_method = env->AllocObject(WellKnownClasses::java_lang_reflect_Method);
    if (env->ExceptionCheck()) {
      return NULL;
    }
    SetObjectField(env,
                   reflect_method,
                   WellKnownClasses::java_lang_reflect_AbstractMethod_artMethod,
                   art_method);
    return reflect_method;
  }

  static jobject ToReflectedField(JNIEnv* env, jclass, jfieldID fid, jboolean) {
    CHECK_NON_NULL_ARGUMENT(ToReflectedField, fid);
    ScopedObjectAccess soa(env);
    ArtField* f = soa.DecodeField(fid);
    jobject art_field = soa.AddLocalReference<jobject>(f);
    jobject reflect_field = env->AllocObject(WellKnownClasses::java_lang_reflect_Field);
    if (env->ExceptionCheck()) {
      return NULL;
    }
    SetObjectField(env,
                   reflect_field,
                   WellKnownClasses::java_lang_reflect_Field_artField,
                   art_field);
    return reflect_field;
  }

  static jclass GetObjectClass(JNIEnv* env, jobject java_object) {
    CHECK_NON_NULL_ARGUMENT(GetObjectClass, java_object);
    ScopedObjectAccess soa(env);
    Object* o = soa.Decode<Object*>(java_object);
    return soa.AddLocalReference<jclass>(o->GetClass());
  }

  static jclass GetSuperclass(JNIEnv* env, jclass java_class) {
    CHECK_NON_NULL_ARGUMENT(GetSuperclass, java_class);
    ScopedObjectAccess soa(env);
    Class* c = soa.Decode<Class*>(java_class);
    return soa.AddLocalReference<jclass>(c->GetSuperClass());
  }

  static jboolean IsAssignableFrom(JNIEnv* env, jclass java_class1, jclass java_class2) {
    CHECK_NON_NULL_ARGUMENT(IsAssignableFrom, java_class1);
    CHECK_NON_NULL_ARGUMENT(IsAssignableFrom, java_class2);
    ScopedObjectAccess soa(env);
    Class* c1 = soa.Decode<Class*>(java_class1);
    Class* c2 = soa.Decode<Class*>(java_class2);
    return c1->IsAssignableFrom(c2) ? JNI_TRUE : JNI_FALSE;
  }

  static jboolean IsInstanceOf(JNIEnv* env, jobject jobj, jclass java_class) {
    CHECK_NON_NULL_ARGUMENT(IsInstanceOf, java_class);
    if (jobj == NULL) {
      // Note: JNI is different from regular Java instanceof in this respect
      return JNI_TRUE;
    } else {
      ScopedObjectAccess soa(env);
      Object* obj = soa.Decode<Object*>(jobj);
      Class* c = soa.Decode<Class*>(java_class);
      return obj->InstanceOf(c) ? JNI_TRUE : JNI_FALSE;
    }
  }

  static jint Throw(JNIEnv* env, jthrowable java_exception) {
    ScopedObjectAccess soa(env);
    Throwable* exception = soa.Decode<Throwable*>(java_exception);
    if (exception == NULL) {
      return JNI_ERR;
    }
    ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
    soa.Self()->SetException(throw_location, exception);
    return JNI_OK;
  }

  static jint ThrowNew(JNIEnv* env, jclass c, const char* msg) {
    CHECK_NON_NULL_ARGUMENT(ThrowNew, c);
    return ThrowNewException(env, c, msg, NULL);
  }

  static jboolean ExceptionCheck(JNIEnv* env) {
    return static_cast<JNIEnvExt*>(env)->self->IsExceptionPending() ? JNI_TRUE : JNI_FALSE;
  }

  static void ExceptionClear(JNIEnv* env) {
    static_cast<JNIEnvExt*>(env)->self->ClearException();
  }

  static void ExceptionDescribe(JNIEnv* env) {
    ScopedObjectAccess soa(env);

    SirtRef<Object> old_throw_this_object(soa.Self(), NULL);
    SirtRef<ArtMethod> old_throw_method(soa.Self(), NULL);
    SirtRef<Throwable> old_exception(soa.Self(), NULL);
    uint32_t old_throw_dex_pc;
    {
      ThrowLocation old_throw_location;
      Throwable* old_exception_obj = soa.Self()->GetException(&old_throw_location);
      old_throw_this_object.reset(old_throw_location.GetThis());
      old_throw_method.reset(old_throw_location.GetMethod());
      old_exception.reset(old_exception_obj);
      old_throw_dex_pc = old_throw_location.GetDexPc();
      soa.Self()->ClearException();
    }
    ScopedLocalRef<jthrowable> exception(env, soa.AddLocalReference<jthrowable>(old_exception.get()));
    ScopedLocalRef<jclass> exception_class(env, env->GetObjectClass(exception.get()));
    jmethodID mid = env->GetMethodID(exception_class.get(), "printStackTrace", "()V");
    if (mid == NULL) {
      LOG(WARNING) << "JNI WARNING: no printStackTrace()V in "
                   << PrettyTypeOf(old_exception.get());
    } else {
      env->CallVoidMethod(exception.get(), mid);
      if (soa.Self()->IsExceptionPending()) {
        LOG(WARNING) << "JNI WARNING: " << PrettyTypeOf(soa.Self()->GetException(NULL))
                     << " thrown while calling printStackTrace";
        soa.Self()->ClearException();
      }
    }
    ThrowLocation gc_safe_throw_location(old_throw_this_object.get(), old_throw_method.get(),
                                         old_throw_dex_pc);

    soa.Self()->SetException(gc_safe_throw_location, old_exception.get());
  }

  static jthrowable ExceptionOccurred(JNIEnv* env) {
    ScopedObjectAccess soa(env);
    Object* exception = soa.Self()->GetException(NULL);
    return soa.AddLocalReference<jthrowable>(exception);
  }

  static void FatalError(JNIEnv*, const char* msg) {
    LOG(FATAL) << "JNI FatalError called: " << msg;
  }

  static jint PushLocalFrame(JNIEnv* env, jint capacity) {
    if (EnsureLocalCapacity(env, capacity, "PushLocalFrame") != JNI_OK) {
      return JNI_ERR;
    }
    static_cast<JNIEnvExt*>(env)->PushFrame(capacity);
    return JNI_OK;
  }

  static jobject PopLocalFrame(JNIEnv* env, jobject java_survivor) {
    ScopedObjectAccess soa(env);
    Object* survivor = soa.Decode<Object*>(java_survivor);
    soa.Env()->PopFrame();
    return soa.AddLocalReference<jobject>(survivor);
  }

  static jint EnsureLocalCapacity(JNIEnv* env, jint desired_capacity) {
    return EnsureLocalCapacity(env, desired_capacity, "EnsureLocalCapacity");
  }

  static jobject NewGlobalRef(JNIEnv* env, jobject obj) {
    ScopedObjectAccess soa(env);
    Object* decoded_obj = soa.Decode<Object*>(obj);
    // Check for null after decoding the object to handle cleared weak globals.
    if (decoded_obj == nullptr) {
      return nullptr;
    }
    JavaVMExt* vm = soa.Vm();
    IndirectReferenceTable& globals = vm->globals;
    WriterMutexLock mu(soa.Self(), vm->globals_lock);
    IndirectRef ref = globals.Add(IRT_FIRST_SEGMENT, decoded_obj);
    return reinterpret_cast<jobject>(ref);
  }

  static void DeleteGlobalRef(JNIEnv* env, jobject obj) {
    if (obj == NULL) {
      return;
    }
    JavaVMExt* vm = reinterpret_cast<JNIEnvExt*>(env)->vm;
    IndirectReferenceTable& globals = vm->globals;
    Thread* self = reinterpret_cast<JNIEnvExt*>(env)->self;
    WriterMutexLock mu(self, vm->globals_lock);

    if (!globals.Remove(IRT_FIRST_SEGMENT, obj)) {
      LOG(WARNING) << "JNI WARNING: DeleteGlobalRef(" << obj << ") "
                   << "failed to find entry";
    }
  }

  static jweak NewWeakGlobalRef(JNIEnv* env, jobject obj) {
    ScopedObjectAccess soa(env);
    return AddWeakGlobalReference(soa, soa.Decode<Object*>(obj));
  }

  static void DeleteWeakGlobalRef(JNIEnv* env, jweak obj) {
    if (obj != nullptr) {
      ScopedObjectAccess soa(env);
      soa.Vm()->DeleteWeakGlobalRef(soa.Self(), obj);
    }
  }

  static jobject NewLocalRef(JNIEnv* env, jobject obj) {
    ScopedObjectAccess soa(env);
    mirror::Object* decoded_obj = soa.Decode<Object*>(obj);
    // Check for null after decoding the object to handle cleared weak globals.
    if (decoded_obj == nullptr) {
      return nullptr;
    }
    return soa.AddLocalReference<jobject>(decoded_obj);
  }

  static void DeleteLocalRef(JNIEnv* env, jobject obj) {
    if (obj == NULL) {
      return;
    }
    IndirectReferenceTable& locals = reinterpret_cast<JNIEnvExt*>(env)->locals;

    uint32_t cookie = reinterpret_cast<JNIEnvExt*>(env)->local_ref_cookie;
    if (!locals.Remove(cookie, obj)) {
      // Attempting to delete a local reference that is not in the
      // topmost local reference frame is a no-op.  DeleteLocalRef returns
      // void and doesn't throw any exceptions, but we should probably
      // complain about it so the user will notice that things aren't
      // going quite the way they expect.
      LOG(WARNING) << "JNI WARNING: DeleteLocalRef(" << obj << ") "
                   << "failed to find entry";
    }
  }

  static jboolean IsSameObject(JNIEnv* env, jobject obj1, jobject obj2) {
    if (obj1 == obj2) {
      return JNI_TRUE;
    } else {
      ScopedObjectAccess soa(env);
      return (soa.Decode<Object*>(obj1) == soa.Decode<Object*>(obj2)) ? JNI_TRUE : JNI_FALSE;
    }
  }

  static jobject AllocObject(JNIEnv* env, jclass java_class) {
    CHECK_NON_NULL_ARGUMENT(AllocObject, java_class);
    ScopedObjectAccess soa(env);
    Class* c = soa.Decode<Class*>(java_class);
    if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(c, true, true)) {
      return NULL;
    }
    return soa.AddLocalReference<jobject>(c->AllocObject(soa.Self()));
  }

  static jobject NewObject(JNIEnv* env, jclass java_class, jmethodID mid, ...) {
    va_list args;
    va_start(args, mid);
    CHECK_NON_NULL_ARGUMENT(NewObject, java_class);
    CHECK_NON_NULL_ARGUMENT(NewObject, mid);
    jobject result = NewObjectV(env, java_class, mid, args);
    va_end(args);
    return result;
  }

  static jobject NewObjectV(JNIEnv* env, jclass java_class, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(NewObjectV, java_class);
    CHECK_NON_NULL_ARGUMENT(NewObjectV, mid);
    ScopedObjectAccess soa(env);
    Class* c = soa.Decode<Class*>(java_class);
    if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(c, true, true)) {
      return NULL;
    }
    Object* result = c->AllocObject(soa.Self());
    if (result == NULL) {
      return NULL;
    }
    jobject local_result = soa.AddLocalReference<jobject>(result);
    CallNonvirtualVoidMethodV(env, local_result, java_class, mid, args);
    if (!soa.Self()->IsExceptionPending()) {
      return local_result;
    } else {
      return NULL;
    }
  }

  static jobject NewObjectA(JNIEnv* env, jclass java_class, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(NewObjectA, java_class);
    CHECK_NON_NULL_ARGUMENT(NewObjectA, mid);
    ScopedObjectAccess soa(env);
    Class* c = soa.Decode<Class*>(java_class);
    if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(c, true, true)) {
      return NULL;
    }
    Object* result = c->AllocObject(soa.Self());
    if (result == NULL) {
      return NULL;
    }
    jobject local_result = soa.AddLocalReference<jobjectArray>(result);
    CallNonvirtualVoidMethodA(env, local_result, java_class, mid, args);
    if (!soa.Self()->IsExceptionPending()) {
      return local_result;
    } else {
      return NULL;
    }
  }

  static jmethodID GetMethodID(JNIEnv* env, jclass java_class, const char* name, const char* sig) {
    CHECK_NON_NULL_ARGUMENT(GetMethodID, java_class);
    CHECK_NON_NULL_ARGUMENT(GetMethodID, name);
    CHECK_NON_NULL_ARGUMENT(GetMethodID, sig);
    ScopedObjectAccess soa(env);
    return FindMethodID(soa, java_class, name, sig, false);
  }

  static jmethodID GetStaticMethodID(JNIEnv* env, jclass java_class, const char* name,
                                     const char* sig) {
    CHECK_NON_NULL_ARGUMENT(GetStaticMethodID, java_class);
    CHECK_NON_NULL_ARGUMENT(GetStaticMethodID, name);
    CHECK_NON_NULL_ARGUMENT(GetStaticMethodID, sig);
    ScopedObjectAccess soa(env);
    return FindMethodID(soa, java_class, name, sig, true);
  }

  static jobject CallObjectMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallObjectMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallObjectMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return soa.AddLocalReference<jobject>(result.GetL());
  }

  static jobject CallObjectMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallObjectMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallObjectMethodV, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args));
    return soa.AddLocalReference<jobject>(result.GetL());
  }

  static jobject CallObjectMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallObjectMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallObjectMethodA, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args));
    return soa.AddLocalReference<jobject>(result.GetL());
  }

  static jboolean CallBooleanMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallBooleanMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallBooleanMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetZ();
  }

  static jboolean CallBooleanMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallBooleanMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallBooleanMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args).GetZ();
  }

  static jboolean CallBooleanMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallBooleanMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallBooleanMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args).GetZ();
  }

  static jbyte CallByteMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallByteMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallByteMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetB();
  }

  static jbyte CallByteMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallByteMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallByteMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args).GetB();
  }

  static jbyte CallByteMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallByteMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallByteMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args).GetB();
  }

  static jchar CallCharMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallCharMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallCharMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetC();
  }

  static jchar CallCharMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallCharMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallCharMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args).GetC();
  }

  static jchar CallCharMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallCharMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallCharMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args).GetC();
  }

  static jdouble CallDoubleMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallDoubleMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallDoubleMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetD();
  }

  static jdouble CallDoubleMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallDoubleMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallDoubleMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args).GetD();
  }

  static jdouble CallDoubleMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallDoubleMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallDoubleMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args).GetD();
  }

  static jfloat CallFloatMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallFloatMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallFloatMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetF();
  }

  static jfloat CallFloatMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallFloatMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallFloatMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args).GetF();
  }

  static jfloat CallFloatMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallFloatMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallFloatMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args).GetF();
  }

  static jint CallIntMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallIntMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallIntMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetI();
  }

  static jint CallIntMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallIntMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallIntMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args).GetI();
  }

  static jint CallIntMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallIntMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallIntMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args).GetI();
  }

  static jlong CallLongMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallLongMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallLongMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetJ();
  }

  static jlong CallLongMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallLongMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallLongMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args).GetJ();
  }

  static jlong CallLongMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallLongMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallLongMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args).GetJ();
  }

  static jshort CallShortMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallShortMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallShortMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetS();
  }

  static jshort CallShortMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallShortMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallShortMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args).GetS();
  }

  static jshort CallShortMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallShortMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallShortMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args).GetS();
  }

  static void CallVoidMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallVoidMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallVoidMethod, mid);
    ScopedObjectAccess soa(env);
    InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, ap);
    va_end(ap);
  }

  static void CallVoidMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallVoidMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallVoidMethodV, mid);
    ScopedObjectAccess soa(env);
    InvokeVirtualOrInterfaceWithVarArgs(soa, obj, mid, args);
  }

  static void CallVoidMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallVoidMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallVoidMethodA, mid);
    ScopedObjectAccess soa(env);
    InvokeVirtualOrInterfaceWithJValues(soa, obj, mid, args);
  }

  static jobject CallNonvirtualObjectMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualObjectMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualObjectMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    jobject local_result = soa.AddLocalReference<jobject>(result.GetL());
    va_end(ap);
    return local_result;
  }

  static jobject CallNonvirtualObjectMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                             va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualObjectMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualObjectMethodV, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, args));
    return soa.AddLocalReference<jobject>(result.GetL());
  }

  static jobject CallNonvirtualObjectMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                             jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualObjectMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualObjectMethodA, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithJValues(soa, obj, mid, args));
    return soa.AddLocalReference<jobject>(result.GetL());
  }

  static jboolean CallNonvirtualBooleanMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                              ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualBooleanMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualBooleanMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetZ();
  }

  static jboolean CallNonvirtualBooleanMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                               va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualBooleanMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualBooleanMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, obj, mid, args).GetZ();
  }

  static jboolean CallNonvirtualBooleanMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                               jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualBooleanMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualBooleanMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, obj, mid, args).GetZ();
  }

  static jbyte CallNonvirtualByteMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualByteMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualByteMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetB();
  }

  static jbyte CallNonvirtualByteMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                         va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualByteMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualByteMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, obj, mid, args).GetB();
  }

  static jbyte CallNonvirtualByteMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                         jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualByteMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualByteMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, obj, mid, args).GetB();
  }

  static jchar CallNonvirtualCharMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualCharMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualCharMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetC();
  }

  static jchar CallNonvirtualCharMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                         va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualCharMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualCharMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, obj, mid, args).GetC();
  }

  static jchar CallNonvirtualCharMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                         jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualCharMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualCharMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, obj, mid, args).GetC();
  }

  static jshort CallNonvirtualShortMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualShortMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualShortMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetS();
  }

  static jshort CallNonvirtualShortMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                           va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualShortMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualShortMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, obj, mid, args).GetS();
  }

  static jshort CallNonvirtualShortMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                           jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualShortMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualShortMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, obj, mid, args).GetS();
  }

  static jint CallNonvirtualIntMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualIntMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualIntMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetI();
  }

  static jint CallNonvirtualIntMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                       va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualIntMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualIntMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, obj, mid, args).GetI();
  }

  static jint CallNonvirtualIntMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                       jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualIntMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualIntMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, obj, mid, args).GetI();
  }

  static jlong CallNonvirtualLongMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualLongMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualLongMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetJ();
  }

  static jlong CallNonvirtualLongMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                         va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualLongMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualLongMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, obj, mid, args).GetJ();
  }

  static jlong CallNonvirtualLongMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                         jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualLongMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualLongMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, obj, mid, args).GetJ();
  }

  static jfloat CallNonvirtualFloatMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualFloatMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualFloatMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetF();
  }

  static jfloat CallNonvirtualFloatMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                           va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualFloatMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualFloatMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, obj, mid, args).GetF();
  }

  static jfloat CallNonvirtualFloatMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                           jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualFloatMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualFloatMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, obj, mid, args).GetF();
  }

  static jdouble CallNonvirtualDoubleMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualDoubleMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualDoubleMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, obj, mid, ap));
    va_end(ap);
    return result.GetD();
  }

  static jdouble CallNonvirtualDoubleMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                             va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualDoubleMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualDoubleMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, obj, mid, args).GetD();
  }

  static jdouble CallNonvirtualDoubleMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                             jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualDoubleMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualDoubleMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, obj, mid, args).GetD();
  }

  static void CallNonvirtualVoidMethod(JNIEnv* env, jobject obj, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualVoidMethod, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualVoidMethod, mid);
    ScopedObjectAccess soa(env);
    InvokeWithVarArgs(soa, obj, mid, ap);
    va_end(ap);
  }

  static void CallNonvirtualVoidMethodV(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                        va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualVoidMethodV, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualVoidMethodV, mid);
    ScopedObjectAccess soa(env);
    InvokeWithVarArgs(soa, obj, mid, args);
  }

  static void CallNonvirtualVoidMethodA(JNIEnv* env, jobject obj, jclass, jmethodID mid,
                                        jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualVoidMethodA, obj);
    CHECK_NON_NULL_ARGUMENT(CallNonvirtualVoidMethodA, mid);
    ScopedObjectAccess soa(env);
    InvokeWithJValues(soa, obj, mid, args);
  }

  static jfieldID GetFieldID(JNIEnv* env, jclass java_class, const char* name, const char* sig) {
    CHECK_NON_NULL_ARGUMENT(GetFieldID, java_class);
    CHECK_NON_NULL_ARGUMENT(GetFieldID, name);
    CHECK_NON_NULL_ARGUMENT(GetFieldID, sig);
    ScopedObjectAccess soa(env);
    return FindFieldID(soa, java_class, name, sig, false);
  }

  static jfieldID GetStaticFieldID(JNIEnv* env, jclass java_class, const char* name,
                                   const char* sig) {
    CHECK_NON_NULL_ARGUMENT(GetStaticFieldID, java_class);
    CHECK_NON_NULL_ARGUMENT(GetStaticFieldID, name);
    CHECK_NON_NULL_ARGUMENT(GetFieldID, sig);
    ScopedObjectAccess soa(env);
    return FindFieldID(soa, java_class, name, sig, true);
  }

  static jobject GetObjectField(JNIEnv* env, jobject obj, jfieldID fid) {
    CHECK_NON_NULL_ARGUMENT(GetObjectField, obj);
    CHECK_NON_NULL_ARGUMENT(GetObjectField, fid);
    ScopedObjectAccess soa(env);
    Object* o = soa.Decode<Object*>(obj);
    ArtField* f = soa.DecodeField(fid);
    return soa.AddLocalReference<jobject>(f->GetObject(o));
  }

  static jobject GetStaticObjectField(JNIEnv* env, jclass, jfieldID fid) {
    CHECK_NON_NULL_ARGUMENT(GetStaticObjectField, fid);
    ScopedObjectAccess soa(env);
    ArtField* f = soa.DecodeField(fid);
    return soa.AddLocalReference<jobject>(f->GetObject(f->GetDeclaringClass()));
  }

  static void SetObjectField(JNIEnv* env, jobject java_object, jfieldID fid, jobject java_value) {
    CHECK_NON_NULL_ARGUMENT(SetObjectField, java_object);
    CHECK_NON_NULL_ARGUMENT(SetObjectField, fid);
    ScopedObjectAccess soa(env);
    Object* o = soa.Decode<Object*>(java_object);
    Object* v = soa.Decode<Object*>(java_value);
    ArtField* f = soa.DecodeField(fid);
    f->SetObject(o, v);
  }

  static void SetStaticObjectField(JNIEnv* env, jclass, jfieldID fid, jobject java_value) {
    CHECK_NON_NULL_ARGUMENT(SetStaticObjectField, fid);
    ScopedObjectAccess soa(env);
    Object* v = soa.Decode<Object*>(java_value);
    ArtField* f = soa.DecodeField(fid);
    f->SetObject(f->GetDeclaringClass(), v);
  }

#define GET_PRIMITIVE_FIELD(fn, instance) \
  CHECK_NON_NULL_ARGUMENT(Get #fn Field, instance); \
  CHECK_NON_NULL_ARGUMENT(Get #fn Field, fid); \
  ScopedObjectAccess soa(env); \
  Object* o = soa.Decode<Object*>(instance); \
  ArtField* f = soa.DecodeField(fid); \
  return f->Get ##fn (o)

#define GET_STATIC_PRIMITIVE_FIELD(fn) \
  CHECK_NON_NULL_ARGUMENT(GetStatic #fn Field, fid); \
  ScopedObjectAccess soa(env); \
  ArtField* f = soa.DecodeField(fid); \
  return f->Get ##fn (f->GetDeclaringClass())

#define SET_PRIMITIVE_FIELD(fn, instance, value) \
  CHECK_NON_NULL_ARGUMENT(Set #fn Field, instance); \
  CHECK_NON_NULL_ARGUMENT(Set #fn Field, fid); \
  ScopedObjectAccess soa(env); \
  Object* o = soa.Decode<Object*>(instance); \
  ArtField* f = soa.DecodeField(fid); \
  f->Set ##fn(o, value)

#define SET_STATIC_PRIMITIVE_FIELD(fn, value) \
  CHECK_NON_NULL_ARGUMENT(SetStatic #fn Field, fid); \
  ScopedObjectAccess soa(env); \
  ArtField* f = soa.DecodeField(fid); \
  f->Set ##fn(f->GetDeclaringClass(), value)

  static jboolean GetBooleanField(JNIEnv* env, jobject obj, jfieldID fid) {
    GET_PRIMITIVE_FIELD(Boolean, obj);
  }

  static jbyte GetByteField(JNIEnv* env, jobject obj, jfieldID fid) {
    GET_PRIMITIVE_FIELD(Byte, obj);
  }

  static jchar GetCharField(JNIEnv* env, jobject obj, jfieldID fid) {
    GET_PRIMITIVE_FIELD(Char, obj);
  }

  static jshort GetShortField(JNIEnv* env, jobject obj, jfieldID fid) {
    GET_PRIMITIVE_FIELD(Short, obj);
  }

  static jint GetIntField(JNIEnv* env, jobject obj, jfieldID fid) {
    GET_PRIMITIVE_FIELD(Int, obj);
  }

  static jlong GetLongField(JNIEnv* env, jobject obj, jfieldID fid) {
    GET_PRIMITIVE_FIELD(Long, obj);
  }

  static jfloat GetFloatField(JNIEnv* env, jobject obj, jfieldID fid) {
    GET_PRIMITIVE_FIELD(Float, obj);
  }

  static jdouble GetDoubleField(JNIEnv* env, jobject obj, jfieldID fid) {
    GET_PRIMITIVE_FIELD(Double, obj);
  }

  static jboolean GetStaticBooleanField(JNIEnv* env, jclass, jfieldID fid) {
    GET_STATIC_PRIMITIVE_FIELD(Boolean);
  }

  static jbyte GetStaticByteField(JNIEnv* env, jclass, jfieldID fid) {
    GET_STATIC_PRIMITIVE_FIELD(Byte);
  }

  static jchar GetStaticCharField(JNIEnv* env, jclass, jfieldID fid) {
    GET_STATIC_PRIMITIVE_FIELD(Char);
  }

  static jshort GetStaticShortField(JNIEnv* env, jclass, jfieldID fid) {
    GET_STATIC_PRIMITIVE_FIELD(Short);
  }

  static jint GetStaticIntField(JNIEnv* env, jclass, jfieldID fid) {
    GET_STATIC_PRIMITIVE_FIELD(Int);
  }

  static jlong GetStaticLongField(JNIEnv* env, jclass, jfieldID fid) {
    GET_STATIC_PRIMITIVE_FIELD(Long);
  }

  static jfloat GetStaticFloatField(JNIEnv* env, jclass, jfieldID fid) {
    GET_STATIC_PRIMITIVE_FIELD(Float);
  }

  static jdouble GetStaticDoubleField(JNIEnv* env, jclass, jfieldID fid) {
    GET_STATIC_PRIMITIVE_FIELD(Double);
  }

  static void SetBooleanField(JNIEnv* env, jobject obj, jfieldID fid, jboolean v) {
    SET_PRIMITIVE_FIELD(Boolean, obj, v);
  }

  static void SetByteField(JNIEnv* env, jobject obj, jfieldID fid, jbyte v) {
    SET_PRIMITIVE_FIELD(Byte, obj, v);
  }

  static void SetCharField(JNIEnv* env, jobject obj, jfieldID fid, jchar v) {
    SET_PRIMITIVE_FIELD(Char, obj, v);
  }

  static void SetFloatField(JNIEnv* env, jobject obj, jfieldID fid, jfloat v) {
    SET_PRIMITIVE_FIELD(Float, obj, v);
  }

  static void SetDoubleField(JNIEnv* env, jobject obj, jfieldID fid, jdouble v) {
    SET_PRIMITIVE_FIELD(Double, obj, v);
  }

  static void SetIntField(JNIEnv* env, jobject obj, jfieldID fid, jint v) {
    SET_PRIMITIVE_FIELD(Int, obj, v);
  }

  static void SetLongField(JNIEnv* env, jobject obj, jfieldID fid, jlong v) {
    SET_PRIMITIVE_FIELD(Long, obj, v);
  }

  static void SetShortField(JNIEnv* env, jobject obj, jfieldID fid, jshort v) {
    SET_PRIMITIVE_FIELD(Short, obj, v);
  }

  static void SetStaticBooleanField(JNIEnv* env, jclass, jfieldID fid, jboolean v) {
    SET_STATIC_PRIMITIVE_FIELD(Boolean, v);
  }

  static void SetStaticByteField(JNIEnv* env, jclass, jfieldID fid, jbyte v) {
    SET_STATIC_PRIMITIVE_FIELD(Byte, v);
  }

  static void SetStaticCharField(JNIEnv* env, jclass, jfieldID fid, jchar v) {
    SET_STATIC_PRIMITIVE_FIELD(Char, v);
  }

  static void SetStaticFloatField(JNIEnv* env, jclass, jfieldID fid, jfloat v) {
    SET_STATIC_PRIMITIVE_FIELD(Float, v);
  }

  static void SetStaticDoubleField(JNIEnv* env, jclass, jfieldID fid, jdouble v) {
    SET_STATIC_PRIMITIVE_FIELD(Double, v);
  }

  static void SetStaticIntField(JNIEnv* env, jclass, jfieldID fid, jint v) {
    SET_STATIC_PRIMITIVE_FIELD(Int, v);
  }

  static void SetStaticLongField(JNIEnv* env, jclass, jfieldID fid, jlong v) {
    SET_STATIC_PRIMITIVE_FIELD(Long, v);
  }

  static void SetStaticShortField(JNIEnv* env, jclass, jfieldID fid, jshort v) {
    SET_STATIC_PRIMITIVE_FIELD(Short, v);
  }

  static jobject CallStaticObjectMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticObjectMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    jobject local_result = soa.AddLocalReference<jobject>(result.GetL());
    va_end(ap);
    return local_result;
  }

  static jobject CallStaticObjectMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticObjectMethodV, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, args));
    return soa.AddLocalReference<jobject>(result.GetL());
  }

  static jobject CallStaticObjectMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticObjectMethodA, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithJValues(soa, NULL, mid, args));
    return soa.AddLocalReference<jobject>(result.GetL());
  }

  static jboolean CallStaticBooleanMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticBooleanMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    va_end(ap);
    return result.GetZ();
  }

  static jboolean CallStaticBooleanMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticBooleanMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, NULL, mid, args).GetZ();
  }

  static jboolean CallStaticBooleanMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticBooleanMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, NULL, mid, args).GetZ();
  }

  static jbyte CallStaticByteMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticByteMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    va_end(ap);
    return result.GetB();
  }

  static jbyte CallStaticByteMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticByteMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, NULL, mid, args).GetB();
  }

  static jbyte CallStaticByteMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticByteMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, NULL, mid, args).GetB();
  }

  static jchar CallStaticCharMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticCharMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    va_end(ap);
    return result.GetC();
  }

  static jchar CallStaticCharMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticCharMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, NULL, mid, args).GetC();
  }

  static jchar CallStaticCharMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticCharMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, NULL, mid, args).GetC();
  }

  static jshort CallStaticShortMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticShortMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    va_end(ap);
    return result.GetS();
  }

  static jshort CallStaticShortMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticShortMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, NULL, mid, args).GetS();
  }

  static jshort CallStaticShortMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticShortMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, NULL, mid, args).GetS();
  }

  static jint CallStaticIntMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticIntMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    va_end(ap);
    return result.GetI();
  }

  static jint CallStaticIntMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticIntMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, NULL, mid, args).GetI();
  }

  static jint CallStaticIntMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticIntMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, NULL, mid, args).GetI();
  }

  static jlong CallStaticLongMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticLongMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    va_end(ap);
    return result.GetJ();
  }

  static jlong CallStaticLongMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticLongMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, NULL, mid, args).GetJ();
  }

  static jlong CallStaticLongMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticLongMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, NULL, mid, args).GetJ();
  }

  static jfloat CallStaticFloatMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticFloatMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    va_end(ap);
    return result.GetF();
  }

  static jfloat CallStaticFloatMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticFloatMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, NULL, mid, args).GetF();
  }

  static jfloat CallStaticFloatMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticFloatMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, NULL, mid, args).GetF();
  }

  static jdouble CallStaticDoubleMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticDoubleMethod, mid);
    ScopedObjectAccess soa(env);
    JValue result(InvokeWithVarArgs(soa, NULL, mid, ap));
    va_end(ap);
    return result.GetD();
  }

  static jdouble CallStaticDoubleMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticDoubleMethodV, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithVarArgs(soa, NULL, mid, args).GetD();
  }

  static jdouble CallStaticDoubleMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticDoubleMethodA, mid);
    ScopedObjectAccess soa(env);
    return InvokeWithJValues(soa, NULL, mid, args).GetD();
  }

  static void CallStaticVoidMethod(JNIEnv* env, jclass, jmethodID mid, ...) {
    va_list ap;
    va_start(ap, mid);
    CHECK_NON_NULL_ARGUMENT(CallStaticVoidMethod, mid);
    ScopedObjectAccess soa(env);
    InvokeWithVarArgs(soa, NULL, mid, ap);
    va_end(ap);
  }

  static void CallStaticVoidMethodV(JNIEnv* env, jclass, jmethodID mid, va_list args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticVoidMethodV, mid);
    ScopedObjectAccess soa(env);
    InvokeWithVarArgs(soa, NULL, mid, args);
  }

  static void CallStaticVoidMethodA(JNIEnv* env, jclass, jmethodID mid, jvalue* args) {
    CHECK_NON_NULL_ARGUMENT(CallStaticVoidMethodA, mid);
    ScopedObjectAccess soa(env);
    InvokeWithJValues(soa, NULL, mid, args);
  }

  static jstring NewString(JNIEnv* env, const jchar* chars, jsize char_count) {
    if (UNLIKELY(chars == NULL && char_count > 0)) { \
      JniAbortF("NewString", "char == null && char_count > 0"); \
    }
    ScopedObjectAccess soa(env);
    String* result = String::AllocFromUtf16(soa.Self(), char_count, chars);
    return soa.AddLocalReference<jstring>(result);
  }

  static jstring NewStringUTF(JNIEnv* env, const char* utf) {
    if (utf == NULL) {
      return NULL;
    }
    ScopedObjectAccess soa(env);
    String* result = String::AllocFromModifiedUtf8(soa.Self(), utf);
    return soa.AddLocalReference<jstring>(result);
  }

  static jsize GetStringLength(JNIEnv* env, jstring java_string) {
    CHECK_NON_NULL_ARGUMENT(GetStringLength, java_string);
    ScopedObjectAccess soa(env);
    return soa.Decode<String*>(java_string)->GetLength();
  }

  static jsize GetStringUTFLength(JNIEnv* env, jstring java_string) {
    CHECK_NON_NULL_ARGUMENT(GetStringLength, java_string);
    ScopedObjectAccess soa(env);
    return soa.Decode<String*>(java_string)->GetUtfLength();
  }

  static void GetStringRegion(JNIEnv* env, jstring java_string, jsize start, jsize length,
                              jchar* buf) {
    CHECK_NON_NULL_ARGUMENT(GetStringRegion, java_string);
    ScopedObjectAccess soa(env);
    String* s = soa.Decode<String*>(java_string);
    if (start < 0 || length < 0 || start + length > s->GetLength()) {
      ThrowSIOOBE(soa, start, length, s->GetLength());
    } else {
      CHECK_NON_NULL_MEMCPY_ARGUMENT(GetStringRegion, length, buf);
      const jchar* chars = s->GetCharArray()->GetData() + s->GetOffset();
      memcpy(buf, chars + start, length * sizeof(jchar));
    }
  }

  static void GetStringUTFRegion(JNIEnv* env, jstring java_string, jsize start, jsize length,
                                 char* buf) {
    CHECK_NON_NULL_ARGUMENT(GetStringUTFRegion, java_string);
    ScopedObjectAccess soa(env);
    String* s = soa.Decode<String*>(java_string);
    if (start < 0 || length < 0 || start + length > s->GetLength()) {
      ThrowSIOOBE(soa, start, length, s->GetLength());
    } else {
      CHECK_NON_NULL_MEMCPY_ARGUMENT(GetStringUTFRegion, length, buf);
      const jchar* chars = s->GetCharArray()->GetData() + s->GetOffset();
      ConvertUtf16ToModifiedUtf8(buf, chars + start, length);
    }
  }

  static const jchar* GetStringChars(JNIEnv* env, jstring java_string, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetStringUTFRegion, java_string);
    ScopedObjectAccess soa(env);
    String* s = soa.Decode<String*>(java_string);
    const CharArray* chars = s->GetCharArray();
    PinPrimitiveArray(soa, chars);
    if (is_copy != NULL) {
      *is_copy = JNI_FALSE;
    }
    return chars->GetData() + s->GetOffset();
  }

  static void ReleaseStringChars(JNIEnv* env, jstring java_string, const jchar*) {
    CHECK_NON_NULL_ARGUMENT(GetStringUTFRegion, java_string);
    ScopedObjectAccess soa(env);
    UnpinPrimitiveArray(soa, soa.Decode<String*>(java_string)->GetCharArray());
  }

  static const jchar* GetStringCritical(JNIEnv* env, jstring java_string, jboolean* is_copy) {
    return GetStringChars(env, java_string, is_copy);
  }

  static void ReleaseStringCritical(JNIEnv* env, jstring java_string, const jchar* chars) {
    return ReleaseStringChars(env, java_string, chars);
  }

  static const char* GetStringUTFChars(JNIEnv* env, jstring java_string, jboolean* is_copy) {
    if (java_string == NULL) {
      return NULL;
    }
    if (is_copy != NULL) {
      *is_copy = JNI_TRUE;
    }
    ScopedObjectAccess soa(env);
    String* s = soa.Decode<String*>(java_string);
    size_t byte_count = s->GetUtfLength();
    char* bytes = new char[byte_count + 1];
    CHECK(bytes != NULL);  // bionic aborts anyway.
    const uint16_t* chars = s->GetCharArray()->GetData() + s->GetOffset();
    ConvertUtf16ToModifiedUtf8(bytes, chars, s->GetLength());
    bytes[byte_count] = '\0';
    return bytes;
  }

  static void ReleaseStringUTFChars(JNIEnv* env, jstring, const char* chars) {
    delete[] chars;
  }

  static jsize GetArrayLength(JNIEnv* env, jarray java_array) {
    CHECK_NON_NULL_ARGUMENT(GetArrayLength, java_array);
    ScopedObjectAccess soa(env);
    Object* obj = soa.Decode<Object*>(java_array);
    if (UNLIKELY(!obj->IsArrayInstance())) {
      JniAbortF("GetArrayLength", "not an array: %s", PrettyTypeOf(obj).c_str());
    }
    Array* array = obj->AsArray();
    return array->GetLength();
  }

  static jobject GetObjectArrayElement(JNIEnv* env, jobjectArray java_array, jsize index) {
    CHECK_NON_NULL_ARGUMENT(GetObjectArrayElement, java_array);
    ScopedObjectAccess soa(env);
    ObjectArray<Object>* array = soa.Decode<ObjectArray<Object>*>(java_array);
    return soa.AddLocalReference<jobject>(array->Get(index));
  }

  static void SetObjectArrayElement(JNIEnv* env, jobjectArray java_array, jsize index,
                                    jobject java_value) {
    CHECK_NON_NULL_ARGUMENT(SetObjectArrayElement, java_array);
    ScopedObjectAccess soa(env);
    ObjectArray<Object>* array = soa.Decode<ObjectArray<Object>*>(java_array);
    Object* value = soa.Decode<Object*>(java_value);
    array->Set(index, value);
  }

  static jbooleanArray NewBooleanArray(JNIEnv* env, jsize length) {
    ScopedObjectAccess soa(env);
    return NewPrimitiveArray<jbooleanArray, BooleanArray>(soa, length);
  }

  static jbyteArray NewByteArray(JNIEnv* env, jsize length) {
    ScopedObjectAccess soa(env);
    return NewPrimitiveArray<jbyteArray, ByteArray>(soa, length);
  }

  static jcharArray NewCharArray(JNIEnv* env, jsize length) {
    ScopedObjectAccess soa(env);
    return NewPrimitiveArray<jcharArray, CharArray>(soa, length);
  }

  static jdoubleArray NewDoubleArray(JNIEnv* env, jsize length) {
    ScopedObjectAccess soa(env);
    return NewPrimitiveArray<jdoubleArray, DoubleArray>(soa, length);
  }

  static jfloatArray NewFloatArray(JNIEnv* env, jsize length) {
    ScopedObjectAccess soa(env);
    return NewPrimitiveArray<jfloatArray, FloatArray>(soa, length);
  }

  static jintArray NewIntArray(JNIEnv* env, jsize length) {
    ScopedObjectAccess soa(env);
    return NewPrimitiveArray<jintArray, IntArray>(soa, length);
  }

  static jlongArray NewLongArray(JNIEnv* env, jsize length) {
    ScopedObjectAccess soa(env);
    return NewPrimitiveArray<jlongArray, LongArray>(soa, length);
  }

  static jobjectArray NewObjectArray(JNIEnv* env, jsize length, jclass element_jclass, jobject initial_element) {
    if (length < 0) {
      JniAbortF("NewObjectArray", "negative array length: %d", length);
    }

    // Compute the array class corresponding to the given element class.
    ScopedObjectAccess soa(env);
    Class* element_class = soa.Decode<Class*>(element_jclass);
    std::string descriptor;
    descriptor += "[";
    descriptor += ClassHelper(element_class).GetDescriptor();

    // Find the class.
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Class* array_class = class_linker->FindClass(descriptor.c_str(),
                                                 element_class->GetClassLoader());
    if (array_class == NULL) {
      return NULL;
    }

    // Allocate and initialize if necessary.
    ObjectArray<Object>* result = ObjectArray<Object>::Alloc(soa.Self(), array_class, length);
    if (initial_element != NULL) {
      Object* initial_object = soa.Decode<Object*>(initial_element);
      for (jsize i = 0; i < length; ++i) {
        result->Set(i, initial_object);
      }
    }
    return soa.AddLocalReference<jobjectArray>(result);
  }

  static jshortArray NewShortArray(JNIEnv* env, jsize length) {
    ScopedObjectAccess soa(env);
    return NewPrimitiveArray<jshortArray, ShortArray>(soa, length);
  }

  static void* GetPrimitiveArrayCritical(JNIEnv* env, jarray java_array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetPrimitiveArrayCritical, java_array);
    ScopedObjectAccess soa(env);
    Array* array = soa.Decode<Array*>(java_array);
    PinPrimitiveArray(soa, array);
    if (is_copy != NULL) {
      *is_copy = JNI_FALSE;
    }
    return array->GetRawData(array->GetClass()->GetComponentSize());
  }

  static void ReleasePrimitiveArrayCritical(JNIEnv* env, jarray array, void*, jint mode) {
    CHECK_NON_NULL_ARGUMENT(ReleasePrimitiveArrayCritical, array);
    ReleasePrimitiveArray(env, array, mode);
  }

  static jboolean* GetBooleanArrayElements(JNIEnv* env, jbooleanArray array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetBooleanArrayElements, array);
    ScopedObjectAccess soa(env);
    return GetPrimitiveArray<jbooleanArray, jboolean*, BooleanArray>(soa, array, is_copy);
  }

  static jbyte* GetByteArrayElements(JNIEnv* env, jbyteArray array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetByteArrayElements, array);
    ScopedObjectAccess soa(env);
    return GetPrimitiveArray<jbyteArray, jbyte*, ByteArray>(soa, array, is_copy);
  }

  static jchar* GetCharArrayElements(JNIEnv* env, jcharArray array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetCharArrayElements, array);
    ScopedObjectAccess soa(env);
    return GetPrimitiveArray<jcharArray, jchar*, CharArray>(soa, array, is_copy);
  }

  static jdouble* GetDoubleArrayElements(JNIEnv* env, jdoubleArray array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetDoubleArrayElements, array);
    ScopedObjectAccess soa(env);
    return GetPrimitiveArray<jdoubleArray, jdouble*, DoubleArray>(soa, array, is_copy);
  }

  static jfloat* GetFloatArrayElements(JNIEnv* env, jfloatArray array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetFloatArrayElements, array);
    ScopedObjectAccess soa(env);
    return GetPrimitiveArray<jfloatArray, jfloat*, FloatArray>(soa, array, is_copy);
  }

  static jint* GetIntArrayElements(JNIEnv* env, jintArray array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetIntArrayElements, array);
    ScopedObjectAccess soa(env);
    return GetPrimitiveArray<jintArray, jint*, IntArray>(soa, array, is_copy);
  }

  static jlong* GetLongArrayElements(JNIEnv* env, jlongArray array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetLongArrayElements, array);
    ScopedObjectAccess soa(env);
    return GetPrimitiveArray<jlongArray, jlong*, LongArray>(soa, array, is_copy);
  }

  static jshort* GetShortArrayElements(JNIEnv* env, jshortArray array, jboolean* is_copy) {
    CHECK_NON_NULL_ARGUMENT(GetShortArrayElements, array);
    ScopedObjectAccess soa(env);
    return GetPrimitiveArray<jshortArray, jshort*, ShortArray>(soa, array, is_copy);
  }

  static void ReleaseBooleanArrayElements(JNIEnv* env, jbooleanArray array, jboolean*, jint mode) {
    ReleasePrimitiveArray(env, array, mode);
  }

  static void ReleaseByteArrayElements(JNIEnv* env, jbyteArray array, jbyte*, jint mode) {
    ReleasePrimitiveArray(env, array, mode);
  }

  static void ReleaseCharArrayElements(JNIEnv* env, jcharArray array, jchar*, jint mode) {
    ReleasePrimitiveArray(env, array, mode);
  }

  static void ReleaseDoubleArrayElements(JNIEnv* env, jdoubleArray array, jdouble*, jint mode) {
    ReleasePrimitiveArray(env, array, mode);
  }

  static void ReleaseFloatArrayElements(JNIEnv* env, jfloatArray array, jfloat*, jint mode) {
    ReleasePrimitiveArray(env, array, mode);
  }

  static void ReleaseIntArrayElements(JNIEnv* env, jintArray array, jint*, jint mode) {
    ReleasePrimitiveArray(env, array, mode);
  }

  static void ReleaseLongArrayElements(JNIEnv* env, jlongArray array, jlong*, jint mode) {
    ReleasePrimitiveArray(env, array, mode);
  }

  static void ReleaseShortArrayElements(JNIEnv* env, jshortArray array, jshort*, jint mode) {
    ReleasePrimitiveArray(env, array, mode);
  }

  static void GetBooleanArrayRegion(JNIEnv* env, jbooleanArray array, jsize start, jsize length,
                                    jboolean* buf) {
    ScopedObjectAccess soa(env);
    GetPrimitiveArrayRegion<jbooleanArray, jboolean, BooleanArray>(soa, array, start, length, buf);
  }

  static void GetByteArrayRegion(JNIEnv* env, jbyteArray array, jsize start, jsize length,
                                 jbyte* buf) {
    ScopedObjectAccess soa(env);
    GetPrimitiveArrayRegion<jbyteArray, jbyte, ByteArray>(soa, array, start, length, buf);
  }

  static void GetCharArrayRegion(JNIEnv* env, jcharArray array, jsize start, jsize length,
                                 jchar* buf) {
    ScopedObjectAccess soa(env);
    GetPrimitiveArrayRegion<jcharArray, jchar, CharArray>(soa, array, start, length, buf);
  }

  static void GetDoubleArrayRegion(JNIEnv* env, jdoubleArray array, jsize start, jsize length,
                                   jdouble* buf) {
    ScopedObjectAccess soa(env);
    GetPrimitiveArrayRegion<jdoubleArray, jdouble, DoubleArray>(soa, array, start, length, buf);
  }

  static void GetFloatArrayRegion(JNIEnv* env, jfloatArray array, jsize start, jsize length,
                                  jfloat* buf) {
    ScopedObjectAccess soa(env);
    GetPrimitiveArrayRegion<jfloatArray, jfloat, FloatArray>(soa, array, start, length, buf);
  }

  static void GetIntArrayRegion(JNIEnv* env, jintArray array, jsize start, jsize length,
                                jint* buf) {
    ScopedObjectAccess soa(env);
    GetPrimitiveArrayRegion<jintArray, jint, IntArray>(soa, array, start, length, buf);
  }

  static void GetLongArrayRegion(JNIEnv* env, jlongArray array, jsize start, jsize length,
                                 jlong* buf) {
    ScopedObjectAccess soa(env);
    GetPrimitiveArrayRegion<jlongArray, jlong, LongArray>(soa, array, start, length, buf);
  }

  static void GetShortArrayRegion(JNIEnv* env, jshortArray array, jsize start, jsize length,
                                  jshort* buf) {
    ScopedObjectAccess soa(env);
    GetPrimitiveArrayRegion<jshortArray, jshort, ShortArray>(soa, array, start, length, buf);
  }

  static void SetBooleanArrayRegion(JNIEnv* env, jbooleanArray array, jsize start, jsize length,
                                    const jboolean* buf) {
    ScopedObjectAccess soa(env);
    SetPrimitiveArrayRegion<jbooleanArray, jboolean, BooleanArray>(soa, array, start, length, buf);
  }

  static void SetByteArrayRegion(JNIEnv* env, jbyteArray array, jsize start, jsize length,
                                 const jbyte* buf) {
    ScopedObjectAccess soa(env);
    SetPrimitiveArrayRegion<jbyteArray, jbyte, ByteArray>(soa, array, start, length, buf);
  }

  static void SetCharArrayRegion(JNIEnv* env, jcharArray array, jsize start, jsize length,
                                 const jchar* buf) {
    ScopedObjectAccess soa(env);
    SetPrimitiveArrayRegion<jcharArray, jchar, CharArray>(soa, array, start, length, buf);
  }

  static void SetDoubleArrayRegion(JNIEnv* env, jdoubleArray array, jsize start, jsize length,
                                   const jdouble* buf) {
    ScopedObjectAccess soa(env);
    SetPrimitiveArrayRegion<jdoubleArray, jdouble, DoubleArray>(soa, array, start, length, buf);
  }

  static void SetFloatArrayRegion(JNIEnv* env, jfloatArray array, jsize start, jsize length,
                                  const jfloat* buf) {
    ScopedObjectAccess soa(env);
    SetPrimitiveArrayRegion<jfloatArray, jfloat, FloatArray>(soa, array, start, length, buf);
  }

  static void SetIntArrayRegion(JNIEnv* env, jintArray array, jsize start, jsize length,
                                const jint* buf) {
    ScopedObjectAccess soa(env);
    SetPrimitiveArrayRegion<jintArray, jint, IntArray>(soa, array, start, length, buf);
  }

  static void SetLongArrayRegion(JNIEnv* env, jlongArray array, jsize start, jsize length,
                                 const jlong* buf) {
    ScopedObjectAccess soa(env);
    SetPrimitiveArrayRegion<jlongArray, jlong, LongArray>(soa, array, start, length, buf);
  }

  static void SetShortArrayRegion(JNIEnv* env, jshortArray array, jsize start, jsize length,
                                  const jshort* buf) {
    ScopedObjectAccess soa(env);
    SetPrimitiveArrayRegion<jshortArray, jshort, ShortArray>(soa, array, start, length, buf);
  }

  static jint RegisterNatives(JNIEnv* env, jclass java_class, const JNINativeMethod* methods,
                              jint method_count) {
    return RegisterNativeMethods(env, java_class, methods, method_count, true);
  }

  static jint RegisterNativeMethods(JNIEnv* env, jclass java_class, const JNINativeMethod* methods,
                                    jint method_count, bool return_errors) {
    if (UNLIKELY(method_count < 0)) {
      JniAbortF("RegisterNatives", "negative method count: %d", method_count);
      return JNI_ERR;  // Not reached.
    }
    CHECK_NON_NULL_ARGUMENT(RegisterNatives, java_class);
    ScopedObjectAccess soa(env);
    Class* c = soa.Decode<Class*>(java_class);
    if (UNLIKELY(method_count == 0)) {
      LOG(WARNING) << "JNI RegisterNativeMethods: attempt to register 0 native methods for "
          << PrettyDescriptor(c);
      return JNI_OK;
    }
    CHECK_NON_NULL_ARGUMENT(RegisterNatives, methods);
    for (jint i = 0; i < method_count; ++i) {
      const char* name = methods[i].name;
      const char* sig = methods[i].signature;

      if (*sig == '!') {
        // TODO: fast jni. it's too noisy to log all these.
        ++sig;
      }

      ArtMethod* m = c->FindDirectMethod(name, sig);
      if (m == NULL) {
        m = c->FindVirtualMethod(name, sig);
      }
      if (m == NULL) {
        LOG(return_errors ? ERROR : FATAL) << "Failed to register native method "
            << PrettyDescriptor(c) << "." << name << sig;
        ThrowNoSuchMethodError(soa, c, name, sig, "static or non-static");
        return JNI_ERR;
      } else if (!m->IsNative()) {
        LOG(return_errors ? ERROR : FATAL) << "Failed to register non-native method "
            << PrettyDescriptor(c) << "." << name << sig
            << " as native";
        ThrowNoSuchMethodError(soa, c, name, sig, "native");
        return JNI_ERR;
      }

      VLOG(jni) << "[Registering JNI native method " << PrettyMethod(m) << "]";

      m->RegisterNative(soa.Self(), methods[i].fnPtr);
    }
    return JNI_OK;
  }

  static jint UnregisterNatives(JNIEnv* env, jclass java_class) {
    CHECK_NON_NULL_ARGUMENT(UnregisterNatives, java_class);
    ScopedObjectAccess soa(env);
    Class* c = soa.Decode<Class*>(java_class);

    VLOG(jni) << "[Unregistering JNI native methods for " << PrettyClass(c) << "]";

    for (size_t i = 0; i < c->NumDirectMethods(); ++i) {
      ArtMethod* m = c->GetDirectMethod(i);
      if (m->IsNative()) {
        m->UnregisterNative(soa.Self());
      }
    }
    for (size_t i = 0; i < c->NumVirtualMethods(); ++i) {
      ArtMethod* m = c->GetVirtualMethod(i);
      if (m->IsNative()) {
        m->UnregisterNative(soa.Self());
      }
    }

    return JNI_OK;
  }

  static jint MonitorEnter(JNIEnv* env, jobject java_object)
      EXCLUSIVE_LOCK_FUNCTION(monitor_lock_) {
    CHECK_NON_NULL_ARGUMENT(MonitorEnter, java_object);
    ScopedObjectAccess soa(env);
    Object* o = soa.Decode<Object*>(java_object);
    o->MonitorEnter(soa.Self());
    if (soa.Self()->IsExceptionPending()) {
      return JNI_ERR;
    }
    soa.Env()->monitors.Add(o);
    return JNI_OK;
  }

  static jint MonitorExit(JNIEnv* env, jobject java_object)
      UNLOCK_FUNCTION(monitor_lock_) {
    CHECK_NON_NULL_ARGUMENT(MonitorExit, java_object);
    ScopedObjectAccess soa(env);
    Object* o = soa.Decode<Object*>(java_object);
    o->MonitorExit(soa.Self());
    if (soa.Self()->IsExceptionPending()) {
      return JNI_ERR;
    }
    soa.Env()->monitors.Remove(o);
    return JNI_OK;
  }

  static jint GetJavaVM(JNIEnv* env, JavaVM** vm) {
    CHECK_NON_NULL_ARGUMENT(GetJavaVM, vm);
    Runtime* runtime = Runtime::Current();
    if (runtime != NULL) {
      *vm = runtime->GetJavaVM();
    } else {
      *vm = NULL;
    }
    return (*vm != NULL) ? JNI_OK : JNI_ERR;
  }

  static jobject NewDirectByteBuffer(JNIEnv* env, void* address, jlong capacity) {
    if (capacity < 0) {
      JniAbortF("NewDirectByteBuffer", "negative buffer capacity: %lld", capacity);
    }
    if (address == NULL && capacity != 0) {
      JniAbortF("NewDirectByteBuffer", "non-zero capacity for NULL pointer: %lld", capacity);
    }

    // At the moment, the Java side is limited to 32 bits.
    CHECK_LE(reinterpret_cast<uintptr_t>(address), 0xffffffff);
    CHECK_LE(capacity, 0xffffffff);
    jlong address_arg = reinterpret_cast<jlong>(address);
    jint capacity_arg = static_cast<jint>(capacity);

    jobject result = env->NewObject(WellKnownClasses::java_nio_DirectByteBuffer,
                                    WellKnownClasses::java_nio_DirectByteBuffer_init,
                                    address_arg, capacity_arg);
    return static_cast<JNIEnvExt*>(env)->self->IsExceptionPending() ? NULL : result;
  }

  static void* GetDirectBufferAddress(JNIEnv* env, jobject java_buffer) {
    return reinterpret_cast<void*>(env->GetLongField(java_buffer, WellKnownClasses::java_nio_DirectByteBuffer_effectiveDirectAddress));
  }

  static jlong GetDirectBufferCapacity(JNIEnv* env, jobject java_buffer) {
    return static_cast<jlong>(env->GetIntField(java_buffer, WellKnownClasses::java_nio_DirectByteBuffer_capacity));
  }

  static jobjectRefType GetObjectRefType(JNIEnv* env, jobject java_object) {
    CHECK_NON_NULL_ARGUMENT(GetObjectRefType, java_object);

    // Do we definitely know what kind of reference this is?
    IndirectRef ref = reinterpret_cast<IndirectRef>(java_object);
    IndirectRefKind kind = GetIndirectRefKind(ref);
    switch (kind) {
    case kLocal:
      if (static_cast<JNIEnvExt*>(env)->locals.Get(ref) != kInvalidIndirectRefObject) {
        return JNILocalRefType;
      }
      return JNIInvalidRefType;
    case kGlobal:
      return JNIGlobalRefType;
    case kWeakGlobal:
      return JNIWeakGlobalRefType;
    case kSirtOrInvalid:
      // Is it in a stack IRT?
      if (static_cast<JNIEnvExt*>(env)->self->SirtContains(java_object)) {
        return JNILocalRefType;
      }

      if (!static_cast<JNIEnvExt*>(env)->vm->work_around_app_jni_bugs) {
        return JNIInvalidRefType;
      }

      // If we're handing out direct pointers, check whether it's a direct pointer
      // to a local reference.
      {
        ScopedObjectAccess soa(env);
        if (soa.Decode<Object*>(java_object) == reinterpret_cast<Object*>(java_object)) {
          if (soa.Env()->locals.ContainsDirectPointer(reinterpret_cast<Object*>(java_object))) {
            return JNILocalRefType;
          }
        }
      }
      return JNIInvalidRefType;
    }
    LOG(FATAL) << "IndirectRefKind[" << kind << "]";
    return JNIInvalidRefType;
  }

 private:
  static jint EnsureLocalCapacity(JNIEnv* env, jint desired_capacity,
                                  const char* caller) {
    // TODO: we should try to expand the table if necessary.
    if (desired_capacity < 0 || desired_capacity > static_cast<jint>(kLocalsMax)) {
      LOG(ERROR) << "Invalid capacity given to " << caller << ": " << desired_capacity;
      return JNI_ERR;
    }
    // TODO: this isn't quite right, since "capacity" includes holes.
    size_t capacity = static_cast<JNIEnvExt*>(env)->locals.Capacity();
    bool okay = (static_cast<jint>(kLocalsMax - capacity) >= desired_capacity);
    if (!okay) {
      ScopedObjectAccess soa(env);
      soa.Self()->ThrowOutOfMemoryError(caller);
    }
    return okay ? JNI_OK : JNI_ERR;
  }

  template<typename JniT, typename ArtT>
  static JniT NewPrimitiveArray(const ScopedObjectAccess& soa, jsize length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (length < 0) {
      JniAbortF("NewPrimitiveArray", "negative array length: %d", length);
    }
    ArtT* result = ArtT::Alloc(soa.Self(), length);
    return soa.AddLocalReference<JniT>(result);
  }

  template <typename ArrayT, typename CArrayT, typename ArtArrayT>
  static CArrayT GetPrimitiveArray(ScopedObjectAccess& soa, ArrayT java_array,
                                   jboolean* is_copy)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    ArtArrayT* array = soa.Decode<ArtArrayT*>(java_array);
    PinPrimitiveArray(soa, array);
    if (is_copy != NULL) {
      *is_copy = JNI_FALSE;
    }
    return array->GetData();
  }

  template <typename ArrayT>
  static void ReleasePrimitiveArray(JNIEnv* env, ArrayT java_array, jint mode) {
    if (mode != JNI_COMMIT) {
      ScopedObjectAccess soa(env);
      Array* array = soa.Decode<Array*>(java_array);
      UnpinPrimitiveArray(soa, array);
    }
  }

  template <typename JavaArrayT, typename JavaT, typename ArrayT>
  static void GetPrimitiveArrayRegion(ScopedObjectAccess& soa, JavaArrayT java_array,
                                      jsize start, jsize length, JavaT* buf)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    CHECK_NON_NULL_ARGUMENT(GetPrimitiveArrayRegion, java_array);
    ArrayT* array = soa.Decode<ArrayT*>(java_array);
    if (start < 0 || length < 0 || start + length > array->GetLength()) {
      ThrowAIOOBE(soa, array, start, length, "src");
    } else {
      CHECK_NON_NULL_MEMCPY_ARGUMENT(GetStringRegion, length, buf);
      JavaT* data = array->GetData();
      memcpy(buf, data + start, length * sizeof(JavaT));
    }
  }

  template <typename JavaArrayT, typename JavaT, typename ArrayT>
  static void SetPrimitiveArrayRegion(ScopedObjectAccess& soa, JavaArrayT java_array,
                                      jsize start, jsize length, const JavaT* buf)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    CHECK_NON_NULL_ARGUMENT(SetPrimitiveArrayRegion, java_array);
    ArrayT* array = soa.Decode<ArrayT*>(java_array);
    if (start < 0 || length < 0 || start + length > array->GetLength()) {
      ThrowAIOOBE(soa, array, start, length, "dst");
    } else {
      CHECK_NON_NULL_MEMCPY_ARGUMENT(GetStringRegion, length, buf);
      JavaT* data = array->GetData();
      memcpy(data + start, buf, length * sizeof(JavaT));
    }
  }
};

const JNINativeInterface gJniNativeInterface = {
  NULL,  // reserved0.
  NULL,  // reserved1.
  NULL,  // reserved2.
  NULL,  // reserved3.
  JNI::GetVersion,
  JNI::DefineClass,
  JNI::FindClass,
  JNI::FromReflectedMethod,
  JNI::FromReflectedField,
  JNI::ToReflectedMethod,
  JNI::GetSuperclass,
  JNI::IsAssignableFrom,
  JNI::ToReflectedField,
  JNI::Throw,
  JNI::ThrowNew,
  JNI::ExceptionOccurred,
  JNI::ExceptionDescribe,
  JNI::ExceptionClear,
  JNI::FatalError,
  JNI::PushLocalFrame,
  JNI::PopLocalFrame,
  JNI::NewGlobalRef,
  JNI::DeleteGlobalRef,
  JNI::DeleteLocalRef,
  JNI::IsSameObject,
  JNI::NewLocalRef,
  JNI::EnsureLocalCapacity,
  JNI::AllocObject,
  JNI::NewObject,
  JNI::NewObjectV,
  JNI::NewObjectA,
  JNI::GetObjectClass,
  JNI::IsInstanceOf,
  JNI::GetMethodID,
  JNI::CallObjectMethod,
  JNI::CallObjectMethodV,
  JNI::CallObjectMethodA,
  JNI::CallBooleanMethod,
  JNI::CallBooleanMethodV,
  JNI::CallBooleanMethodA,
  JNI::CallByteMethod,
  JNI::CallByteMethodV,
  JNI::CallByteMethodA,
  JNI::CallCharMethod,
  JNI::CallCharMethodV,
  JNI::CallCharMethodA,
  JNI::CallShortMethod,
  JNI::CallShortMethodV,
  JNI::CallShortMethodA,
  JNI::CallIntMethod,
  JNI::CallIntMethodV,
  JNI::CallIntMethodA,
  JNI::CallLongMethod,
  JNI::CallLongMethodV,
  JNI::CallLongMethodA,
  JNI::CallFloatMethod,
  JNI::CallFloatMethodV,
  JNI::CallFloatMethodA,
  JNI::CallDoubleMethod,
  JNI::CallDoubleMethodV,
  JNI::CallDoubleMethodA,
  JNI::CallVoidMethod,
  JNI::CallVoidMethodV,
  JNI::CallVoidMethodA,
  JNI::CallNonvirtualObjectMethod,
  JNI::CallNonvirtualObjectMethodV,
  JNI::CallNonvirtualObjectMethodA,
  JNI::CallNonvirtualBooleanMethod,
  JNI::CallNonvirtualBooleanMethodV,
  JNI::CallNonvirtualBooleanMethodA,
  JNI::CallNonvirtualByteMethod,
  JNI::CallNonvirtualByteMethodV,
  JNI::CallNonvirtualByteMethodA,
  JNI::CallNonvirtualCharMethod,
  JNI::CallNonvirtualCharMethodV,
  JNI::CallNonvirtualCharMethodA,
  JNI::CallNonvirtualShortMethod,
  JNI::CallNonvirtualShortMethodV,
  JNI::CallNonvirtualShortMethodA,
  JNI::CallNonvirtualIntMethod,
  JNI::CallNonvirtualIntMethodV,
  JNI::CallNonvirtualIntMethodA,
  JNI::CallNonvirtualLongMethod,
  JNI::CallNonvirtualLongMethodV,
  JNI::CallNonvirtualLongMethodA,
  JNI::CallNonvirtualFloatMethod,
  JNI::CallNonvirtualFloatMethodV,
  JNI::CallNonvirtualFloatMethodA,
  JNI::CallNonvirtualDoubleMethod,
  JNI::CallNonvirtualDoubleMethodV,
  JNI::CallNonvirtualDoubleMethodA,
  JNI::CallNonvirtualVoidMethod,
  JNI::CallNonvirtualVoidMethodV,
  JNI::CallNonvirtualVoidMethodA,
  JNI::GetFieldID,
  JNI::GetObjectField,
  JNI::GetBooleanField,
  JNI::GetByteField,
  JNI::GetCharField,
  JNI::GetShortField,
  JNI::GetIntField,
  JNI::GetLongField,
  JNI::GetFloatField,
  JNI::GetDoubleField,
  JNI::SetObjectField,
  JNI::SetBooleanField,
  JNI::SetByteField,
  JNI::SetCharField,
  JNI::SetShortField,
  JNI::SetIntField,
  JNI::SetLongField,
  JNI::SetFloatField,
  JNI::SetDoubleField,
  JNI::GetStaticMethodID,
  JNI::CallStaticObjectMethod,
  JNI::CallStaticObjectMethodV,
  JNI::CallStaticObjectMethodA,
  JNI::CallStaticBooleanMethod,
  JNI::CallStaticBooleanMethodV,
  JNI::CallStaticBooleanMethodA,
  JNI::CallStaticByteMethod,
  JNI::CallStaticByteMethodV,
  JNI::CallStaticByteMethodA,
  JNI::CallStaticCharMethod,
  JNI::CallStaticCharMethodV,
  JNI::CallStaticCharMethodA,
  JNI::CallStaticShortMethod,
  JNI::CallStaticShortMethodV,
  JNI::CallStaticShortMethodA,
  JNI::CallStaticIntMethod,
  JNI::CallStaticIntMethodV,
  JNI::CallStaticIntMethodA,
  JNI::CallStaticLongMethod,
  JNI::CallStaticLongMethodV,
  JNI::CallStaticLongMethodA,
  JNI::CallStaticFloatMethod,
  JNI::CallStaticFloatMethodV,
  JNI::CallStaticFloatMethodA,
  JNI::CallStaticDoubleMethod,
  JNI::CallStaticDoubleMethodV,
  JNI::CallStaticDoubleMethodA,
  JNI::CallStaticVoidMethod,
  JNI::CallStaticVoidMethodV,
  JNI::CallStaticVoidMethodA,
  JNI::GetStaticFieldID,
  JNI::GetStaticObjectField,
  JNI::GetStaticBooleanField,
  JNI::GetStaticByteField,
  JNI::GetStaticCharField,
  JNI::GetStaticShortField,
  JNI::GetStaticIntField,
  JNI::GetStaticLongField,
  JNI::GetStaticFloatField,
  JNI::GetStaticDoubleField,
  JNI::SetStaticObjectField,
  JNI::SetStaticBooleanField,
  JNI::SetStaticByteField,
  JNI::SetStaticCharField,
  JNI::SetStaticShortField,
  JNI::SetStaticIntField,
  JNI::SetStaticLongField,
  JNI::SetStaticFloatField,
  JNI::SetStaticDoubleField,
  JNI::NewString,
  JNI::GetStringLength,
  JNI::GetStringChars,
  JNI::ReleaseStringChars,
  JNI::NewStringUTF,
  JNI::GetStringUTFLength,
  JNI::GetStringUTFChars,
  JNI::ReleaseStringUTFChars,
  JNI::GetArrayLength,
  JNI::NewObjectArray,
  JNI::GetObjectArrayElement,
  JNI::SetObjectArrayElement,
  JNI::NewBooleanArray,
  JNI::NewByteArray,
  JNI::NewCharArray,
  JNI::NewShortArray,
  JNI::NewIntArray,
  JNI::NewLongArray,
  JNI::NewFloatArray,
  JNI::NewDoubleArray,
  JNI::GetBooleanArrayElements,
  JNI::GetByteArrayElements,
  JNI::GetCharArrayElements,
  JNI::GetShortArrayElements,
  JNI::GetIntArrayElements,
  JNI::GetLongArrayElements,
  JNI::GetFloatArrayElements,
  JNI::GetDoubleArrayElements,
  JNI::ReleaseBooleanArrayElements,
  JNI::ReleaseByteArrayElements,
  JNI::ReleaseCharArrayElements,
  JNI::ReleaseShortArrayElements,
  JNI::ReleaseIntArrayElements,
  JNI::ReleaseLongArrayElements,
  JNI::ReleaseFloatArrayElements,
  JNI::ReleaseDoubleArrayElements,
  JNI::GetBooleanArrayRegion,
  JNI::GetByteArrayRegion,
  JNI::GetCharArrayRegion,
  JNI::GetShortArrayRegion,
  JNI::GetIntArrayRegion,
  JNI::GetLongArrayRegion,
  JNI::GetFloatArrayRegion,
  JNI::GetDoubleArrayRegion,
  JNI::SetBooleanArrayRegion,
  JNI::SetByteArrayRegion,
  JNI::SetCharArrayRegion,
  JNI::SetShortArrayRegion,
  JNI::SetIntArrayRegion,
  JNI::SetLongArrayRegion,
  JNI::SetFloatArrayRegion,
  JNI::SetDoubleArrayRegion,
  JNI::RegisterNatives,
  JNI::UnregisterNatives,
  JNI::MonitorEnter,
  JNI::MonitorExit,
  JNI::GetJavaVM,
  JNI::GetStringRegion,
  JNI::GetStringUTFRegion,
  JNI::GetPrimitiveArrayCritical,
  JNI::ReleasePrimitiveArrayCritical,
  JNI::GetStringCritical,
  JNI::ReleaseStringCritical,
  JNI::NewWeakGlobalRef,
  JNI::DeleteWeakGlobalRef,
  JNI::ExceptionCheck,
  JNI::NewDirectByteBuffer,
  JNI::GetDirectBufferAddress,
  JNI::GetDirectBufferCapacity,
  JNI::GetObjectRefType,
};

JNIEnvExt::JNIEnvExt(Thread* self, JavaVMExt* vm)
    : self(self),
      vm(vm),
      local_ref_cookie(IRT_FIRST_SEGMENT),
      locals(kLocalsInitial, kLocalsMax, kLocal),
      check_jni(false),
      critical(false),
      monitors("monitors", kMonitorsInitial, kMonitorsMax) {
  functions = unchecked_functions = &gJniNativeInterface;
  if (vm->check_jni) {
    SetCheckJniEnabled(true);
  }
  // The JniEnv local reference values must be at a consistent offset or else cross-compilation
  // errors will ensue.
  CHECK_EQ(JNIEnvExt::LocalRefCookieOffset().Int32Value(), 12);
  CHECK_EQ(JNIEnvExt::SegmentStateOffset().Int32Value(), 16);
}

JNIEnvExt::~JNIEnvExt() {
}

void JNIEnvExt::SetCheckJniEnabled(bool enabled) {
  check_jni = enabled;
  functions = enabled ? GetCheckJniNativeInterface() : &gJniNativeInterface;
}

void JNIEnvExt::DumpReferenceTables(std::ostream& os) {
  locals.Dump(os);
  monitors.Dump(os);
}

void JNIEnvExt::PushFrame(int /*capacity*/) {
  // TODO: take 'capacity' into account.
  stacked_local_ref_cookies.push_back(local_ref_cookie);
  local_ref_cookie = locals.GetSegmentState();
}

void JNIEnvExt::PopFrame() {
  locals.SetSegmentState(local_ref_cookie);
  local_ref_cookie = stacked_local_ref_cookies.back();
  stacked_local_ref_cookies.pop_back();
}

Offset JNIEnvExt::SegmentStateOffset() {
  return Offset(OFFSETOF_MEMBER(JNIEnvExt, locals) +
                IndirectReferenceTable::SegmentStateOffset().Int32Value());
}

// JNI Invocation interface.

extern "C" jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args) {
  const JavaVMInitArgs* args = static_cast<JavaVMInitArgs*>(vm_args);
  if (IsBadJniVersion(args->version)) {
    LOG(ERROR) << "Bad JNI version passed to CreateJavaVM: " << args->version;
    return JNI_EVERSION;
  }
  Runtime::Options options;
  for (int i = 0; i < args->nOptions; ++i) {
    JavaVMOption* option = &args->options[i];
    options.push_back(std::make_pair(std::string(option->optionString), option->extraInfo));
  }
  bool ignore_unrecognized = args->ignoreUnrecognized;
  if (!Runtime::Create(options, ignore_unrecognized)) {
    return JNI_ERR;
  }
  Runtime* runtime = Runtime::Current();
  bool started = runtime->Start();
  if (!started) {
    delete Thread::Current()->GetJniEnv();
    delete runtime->GetJavaVM();
    LOG(WARNING) << "CreateJavaVM failed";
    return JNI_ERR;
  }
  *p_env = Thread::Current()->GetJniEnv();
  *p_vm = runtime->GetJavaVM();
  return JNI_OK;
}

extern "C" jint JNI_GetCreatedJavaVMs(JavaVM** vms, jsize, jsize* vm_count) {
  Runtime* runtime = Runtime::Current();
  if (runtime == NULL) {
    *vm_count = 0;
  } else {
    *vm_count = 1;
    vms[0] = runtime->GetJavaVM();
  }
  return JNI_OK;
}

// Historically unsupported.
extern "C" jint JNI_GetDefaultJavaVMInitArgs(void* /*vm_args*/) {
  return JNI_ERR;
}

class JII {
 public:
  static jint DestroyJavaVM(JavaVM* vm) {
    if (vm == NULL) {
      return JNI_ERR;
    }
    JavaVMExt* raw_vm = reinterpret_cast<JavaVMExt*>(vm);
    delete raw_vm->runtime;
    return JNI_OK;
  }

  static jint AttachCurrentThread(JavaVM* vm, JNIEnv** p_env, void* thr_args) {
    return JII_AttachCurrentThread(vm, p_env, thr_args, false);
  }

  static jint AttachCurrentThreadAsDaemon(JavaVM* vm, JNIEnv** p_env, void* thr_args) {
    return JII_AttachCurrentThread(vm, p_env, thr_args, true);
  }

  static jint DetachCurrentThread(JavaVM* vm) {
    if (vm == NULL || Thread::Current() == NULL) {
      return JNI_ERR;
    }
    JavaVMExt* raw_vm = reinterpret_cast<JavaVMExt*>(vm);
    Runtime* runtime = raw_vm->runtime;
    runtime->DetachCurrentThread();
    return JNI_OK;
  }

  static jint GetEnv(JavaVM* vm, void** env, jint version) {
    // GetEnv always returns a JNIEnv* for the most current supported JNI version,
    // and unlike other calls that take a JNI version doesn't care if you supply
    // JNI_VERSION_1_1, which we don't otherwise support.
    if (IsBadJniVersion(version) && version != JNI_VERSION_1_1) {
      LOG(ERROR) << "Bad JNI version passed to GetEnv: " << version;
      return JNI_EVERSION;
    }
    if (vm == NULL || env == NULL) {
      return JNI_ERR;
    }
    Thread* thread = Thread::Current();
    if (thread == NULL) {
      *env = NULL;
      return JNI_EDETACHED;
    }
    *env = thread->GetJniEnv();
    return JNI_OK;
  }
};

const JNIInvokeInterface gJniInvokeInterface = {
  NULL,  // reserved0
  NULL,  // reserved1
  NULL,  // reserved2
  JII::DestroyJavaVM,
  JII::AttachCurrentThread,
  JII::DetachCurrentThread,
  JII::GetEnv,
  JII::AttachCurrentThreadAsDaemon
};

JavaVMExt::JavaVMExt(Runtime* runtime, Runtime::ParsedOptions* options)
    : runtime(runtime),
      check_jni_abort_hook(NULL),
      check_jni_abort_hook_data(NULL),
      check_jni(false),
      force_copy(false),  // TODO: add a way to enable this
      trace(options->jni_trace_),
      work_around_app_jni_bugs(false),
      pins_lock("JNI pin table lock", kPinTableLock),
      pin_table("pin table", kPinTableInitial, kPinTableMax),
      globals_lock("JNI global reference table lock"),
      globals(gGlobalsInitial, gGlobalsMax, kGlobal),
      libraries_lock("JNI shared libraries map lock", kLoadLibraryLock),
      libraries(new Libraries),
      weak_globals_lock_("JNI weak global reference table lock"),
      weak_globals_(kWeakGlobalsInitial, kWeakGlobalsMax, kWeakGlobal),
      allow_new_weak_globals_(true),
      weak_globals_add_condition_("weak globals add condition", weak_globals_lock_) {
  functions = unchecked_functions = &gJniInvokeInterface;
  if (options->check_jni_) {
    SetCheckJniEnabled(true);
  }
}

JavaVMExt::~JavaVMExt() {
  delete libraries;
}

jweak JavaVMExt::AddWeakGlobalReference(Thread* self, mirror::Object* obj) {
  if (obj == nullptr) {
    return nullptr;
  }
  MutexLock mu(self, weak_globals_lock_);
  while (UNLIKELY(!allow_new_weak_globals_)) {
    weak_globals_add_condition_.WaitHoldingLocks(self);
  }
  IndirectRef ref = weak_globals_.Add(IRT_FIRST_SEGMENT, obj);
  return reinterpret_cast<jweak>(ref);
}

void JavaVMExt::DeleteWeakGlobalRef(Thread* self, jweak obj) {
  MutexLock mu(self, weak_globals_lock_);
  if (!weak_globals_.Remove(IRT_FIRST_SEGMENT, obj)) {
    LOG(WARNING) << "JNI WARNING: DeleteWeakGlobalRef(" << obj << ") "
                 << "failed to find entry";
  }
}

void JavaVMExt::SetCheckJniEnabled(bool enabled) {
  check_jni = enabled;
  functions = enabled ? GetCheckJniInvokeInterface() : &gJniInvokeInterface;
}

void JavaVMExt::DumpForSigQuit(std::ostream& os) {
  os << "JNI: CheckJNI is " << (check_jni ? "on" : "off");
  if (force_copy) {
    os << " (with forcecopy)";
  }
  os << "; workarounds are " << (work_around_app_jni_bugs ? "on" : "off");
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, pins_lock);
    os << "; pins=" << pin_table.Size();
  }
  {
    ReaderMutexLock mu(self, globals_lock);
    os << "; globals=" << globals.Capacity();
  }
  {
    MutexLock mu(self, weak_globals_lock_);
    if (weak_globals_.Capacity() > 0) {
      os << " (plus " << weak_globals_.Capacity() << " weak)";
    }
  }
  os << '\n';

  {
    MutexLock mu(self, libraries_lock);
    os << "Libraries: " << Dumpable<Libraries>(*libraries) << " (" << libraries->size() << ")\n";
  }
}

void JavaVMExt::DisallowNewWeakGlobals() {
  MutexLock mu(Thread::Current(), weak_globals_lock_);
  allow_new_weak_globals_ = false;
}

void JavaVMExt::AllowNewWeakGlobals() {
  Thread* self = Thread::Current();
  MutexLock mu(self, weak_globals_lock_);
  allow_new_weak_globals_ = true;
  weak_globals_add_condition_.Broadcast(self);
}

void JavaVMExt::SweepWeakGlobals(IsMarkedTester is_marked, void* arg) {
  MutexLock mu(Thread::Current(), weak_globals_lock_);
  for (const Object** entry : weak_globals_) {
    if (!is_marked(*entry, arg)) {
      *entry = kClearedJniWeakGlobal;
    }
  }
}

mirror::Object* JavaVMExt::DecodeWeakGlobal(Thread* self, IndirectRef ref) {
  MutexLock mu(self, weak_globals_lock_);
  while (UNLIKELY(!allow_new_weak_globals_)) {
    weak_globals_add_condition_.WaitHoldingLocks(self);
  }
  return const_cast<mirror::Object*>(weak_globals_.Get(ref));
}

void JavaVMExt::DumpReferenceTables(std::ostream& os) {
  Thread* self = Thread::Current();
  {
    ReaderMutexLock mu(self, globals_lock);
    globals.Dump(os);
  }
  {
    MutexLock mu(self, weak_globals_lock_);
    weak_globals_.Dump(os);
  }
  {
    MutexLock mu(self, pins_lock);
    pin_table.Dump(os);
  }
}

bool JavaVMExt::LoadNativeLibrary(const std::string& path, ClassLoader* class_loader,
                                  std::string& detail) {
  detail.clear();

  // See if we've already loaded this library.  If we have, and the class loader
  // matches, return successfully without doing anything.
  // TODO: for better results we should canonicalize the pathname (or even compare
  // inodes). This implementation is fine if everybody is using System.loadLibrary.
  SharedLibrary* library;
  Thread* self = Thread::Current();
  {
    // TODO: move the locking (and more of this logic) into Libraries.
    MutexLock mu(self, libraries_lock);
    library = libraries->Get(path);
  }
  if (library != NULL) {
    if (library->GetClassLoader() != class_loader) {
      // The library will be associated with class_loader. The JNI
      // spec says we can't load the same library into more than one
      // class loader.
      StringAppendF(&detail, "Shared library \"%s\" already opened by "
          "ClassLoader %p; can't open in ClassLoader %p",
          path.c_str(), library->GetClassLoader(), class_loader);
      LOG(WARNING) << detail;
      return false;
    }
    VLOG(jni) << "[Shared library \"" << path << "\" already loaded in "
              << "ClassLoader " << class_loader << "]";
    if (!library->CheckOnLoadResult()) {
      StringAppendF(&detail, "JNI_OnLoad failed on a previous attempt "
          "to load \"%s\"", path.c_str());
      return false;
    }
    return true;
  }

  // Open the shared library.  Because we're using a full path, the system
  // doesn't have to search through LD_LIBRARY_PATH.  (It may do so to
  // resolve this library's dependencies though.)

  // Failures here are expected when java.library.path has several entries
  // and we have to hunt for the lib.

  // Below we dlopen but there is no paired dlclose, this would be necessary if we supported
  // class unloading. Libraries will only be unloaded when the reference count (incremented by
  // dlopen) becomes zero from dlclose.

  // This can execute slowly for a large library on a busy system, so we
  // want to switch from kRunnable while it executes.  This allows the GC to ignore us.
  self->TransitionFromRunnableToSuspended(kWaitingForJniOnLoad);
  void* handle = dlopen(path.empty() ? NULL : path.c_str(), RTLD_LAZY);
  self->TransitionFromSuspendedToRunnable();

  VLOG(jni) << "[Call to dlopen(\"" << path << "\", RTLD_LAZY) returned " << handle << "]";

  if (handle == NULL) {
    detail = dlerror();
    LOG(ERROR) << "dlopen(\"" << path << "\", RTLD_LAZY) failed: " << detail;
    return false;
  }

  // Create a new entry.
  // TODO: move the locking (and more of this logic) into Libraries.
  bool created_library = false;
  {
    MutexLock mu(self, libraries_lock);
    library = libraries->Get(path);
    if (library == NULL) {  // We won race to get libraries_lock
      library = new SharedLibrary(path, handle, class_loader);
      libraries->Put(path, library);
      created_library = true;
    }
  }
  if (!created_library) {
    LOG(INFO) << "WOW: we lost a race to add shared library: "
        << "\"" << path << "\" ClassLoader=" << class_loader;
    return library->CheckOnLoadResult();
  }

  VLOG(jni) << "[Added shared library \"" << path << "\" for ClassLoader " << class_loader << "]";

  bool was_successful = false;
  void* sym = dlsym(handle, "JNI_OnLoad");
  if (sym == NULL) {
    VLOG(jni) << "[No JNI_OnLoad found in \"" << path << "\"]";
    was_successful = true;
  } else {
    // Call JNI_OnLoad.  We have to override the current class
    // loader, which will always be "null" since the stuff at the
    // top of the stack is around Runtime.loadLibrary().  (See
    // the comments in the JNI FindClass function.)
    typedef int (*JNI_OnLoadFn)(JavaVM*, void*);
    JNI_OnLoadFn jni_on_load = reinterpret_cast<JNI_OnLoadFn>(sym);
    ClassLoader* old_class_loader = self->GetClassLoaderOverride();
    self->SetClassLoaderOverride(class_loader);

    int version = 0;
    {
      ScopedThreadStateChange tsc(self, kNative);
      VLOG(jni) << "[Calling JNI_OnLoad in \"" << path << "\"]";
      version = (*jni_on_load)(this, NULL);
    }

    self->SetClassLoaderOverride(old_class_loader);

    if (version == JNI_ERR) {
      StringAppendF(&detail, "JNI_ERR returned from JNI_OnLoad in \"%s\"", path.c_str());
    } else if (IsBadJniVersion(version)) {
      StringAppendF(&detail, "Bad JNI version returned from JNI_OnLoad in \"%s\": %d",
                    path.c_str(), version);
      // It's unwise to call dlclose() here, but we can mark it
      // as bad and ensure that future load attempts will fail.
      // We don't know how far JNI_OnLoad got, so there could
      // be some partially-initialized stuff accessible through
      // newly-registered native method calls.  We could try to
      // unregister them, but that doesn't seem worthwhile.
    } else {
      was_successful = true;
    }
    VLOG(jni) << "[Returned " << (was_successful ? "successfully" : "failure")
              << " from JNI_OnLoad in \"" << path << "\"]";
  }

  library->SetResult(was_successful);
  return was_successful;
}

void* JavaVMExt::FindCodeForNativeMethod(ArtMethod* m) {
  CHECK(m->IsNative());

  Class* c = m->GetDeclaringClass();

  // If this is a static method, it could be called before the class
  // has been initialized.
  if (m->IsStatic()) {
    if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(c, true, true)) {
      return NULL;
    }
  } else {
    CHECK(c->IsInitializing()) << c->GetStatus() << " " << PrettyMethod(m);
  }

  std::string detail;
  void* native_method;
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, libraries_lock);
    native_method = libraries->FindNativeMethod(m, detail);
  }
  // Throwing can cause libraries_lock to be reacquired.
  if (native_method == NULL) {
    ThrowLocation throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewException(throw_location, "Ljava/lang/UnsatisfiedLinkError;", detail.c_str());
  }
  return native_method;
}

void JavaVMExt::VisitRoots(RootVisitor* visitor, void* arg) {
  Thread* self = Thread::Current();
  {
    ReaderMutexLock mu(self, globals_lock);
    globals.VisitRoots(visitor, arg);
  }
  {
    MutexLock mu(self, pins_lock);
    pin_table.VisitRoots(visitor, arg);
  }
  // The weak_globals table is visited by the GC itself (because it mutates the table).
}

void RegisterNativeMethods(JNIEnv* env, const char* jni_class_name, const JNINativeMethod* methods,
                           jint method_count) {
  ScopedLocalRef<jclass> c(env, env->FindClass(jni_class_name));
  if (c.get() == NULL) {
    LOG(FATAL) << "Couldn't find class: " << jni_class_name;
  }
  JNI::RegisterNativeMethods(env, c.get(), methods, method_count, false);
}

}  // namespace art

std::ostream& operator<<(std::ostream& os, const jobjectRefType& rhs) {
  switch (rhs) {
  case JNIInvalidRefType:
    os << "JNIInvalidRefType";
    return os;
  case JNILocalRefType:
    os << "JNILocalRefType";
    return os;
  case JNIGlobalRefType:
    os << "JNIGlobalRefType";
    return os;
  case JNIWeakGlobalRefType:
    os << "JNIWeakGlobalRefType";
    return os;
  default:
    LOG(FATAL) << "jobjectRefType[" << static_cast<int>(rhs) << "]";
    return os;
  }
}
