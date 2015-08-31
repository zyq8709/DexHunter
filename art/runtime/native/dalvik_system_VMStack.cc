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

#include "jni_internal.h"
#include "nth_caller_visitor.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "scoped_thread_state_change.h"
#include "thread_list.h"

namespace art {

static jobject GetThreadStack(JNIEnv* env, jobject peer) {
  {
    ScopedObjectAccess soa(env);
    if (soa.Decode<mirror::Object*>(peer) == soa.Self()->GetPeer()) {
      return soa.Self()->CreateInternalStackTrace(soa);
    }
  }
  // Suspend thread to build stack trace.
  bool timed_out;
  Thread* thread = Thread::SuspendForDebugger(peer, true, &timed_out);
  if (thread != NULL) {
    jobject trace;
    {
      ScopedObjectAccess soa(env);
      trace = thread->CreateInternalStackTrace(soa);
    }
    // Restart suspended thread.
    Runtime::Current()->GetThreadList()->Resume(thread, true);
    return trace;
  } else {
    if (timed_out) {
      LOG(ERROR) << "Trying to get thread's stack failed as the thread failed to suspend within a "
          "generous timeout.";
    }
    return NULL;
  }
}

static jint VMStack_fillStackTraceElements(JNIEnv* env, jclass, jobject javaThread,
                                           jobjectArray javaSteArray) {
  jobject trace = GetThreadStack(env, javaThread);
  if (trace == NULL) {
    return 0;
  }
  int32_t depth;
  Thread::InternalStackTraceToStackTraceElementArray(env, trace, javaSteArray, &depth);
  return depth;
}

// Returns the defining class loader of the caller's caller.
static jobject VMStack_getCallingClassLoader(JNIEnv* env, jclass) {
  ScopedObjectAccess soa(env);
  NthCallerVisitor visitor(soa.Self(), 2);
  visitor.WalkStack();
  return soa.AddLocalReference<jobject>(visitor.caller->GetDeclaringClass()->GetClassLoader());
}

static jobject VMStack_getClosestUserClassLoader(JNIEnv* env, jclass, jobject javaBootstrap,
                                                 jobject javaSystem) {
  struct ClosestUserClassLoaderVisitor : public StackVisitor {
    ClosestUserClassLoaderVisitor(Thread* thread, mirror::Object* bootstrap, mirror::Object* system)
      : StackVisitor(thread, NULL), bootstrap(bootstrap), system(system), class_loader(NULL) {}

    bool VisitFrame() {
      DCHECK(class_loader == NULL);
      mirror::Class* c = GetMethod()->GetDeclaringClass();
      mirror::Object* cl = c->GetClassLoader();
      if (cl != NULL && cl != bootstrap && cl != system) {
        class_loader = cl;
        return false;
      }
      return true;
    }

    mirror::Object* bootstrap;
    mirror::Object* system;
    mirror::Object* class_loader;
  };
  ScopedObjectAccess soa(env);
  mirror::Object* bootstrap = soa.Decode<mirror::Object*>(javaBootstrap);
  mirror::Object* system = soa.Decode<mirror::Object*>(javaSystem);
  ClosestUserClassLoaderVisitor visitor(soa.Self(), bootstrap, system);
  visitor.WalkStack();
  return soa.AddLocalReference<jobject>(visitor.class_loader);
}

// Returns the class of the caller's caller's caller.
static jclass VMStack_getStackClass2(JNIEnv* env, jclass) {
  ScopedObjectAccess soa(env);
  NthCallerVisitor visitor(soa.Self(), 3);
  visitor.WalkStack();
  return soa.AddLocalReference<jclass>(visitor.caller->GetDeclaringClass());
}

static jobjectArray VMStack_getThreadStackTrace(JNIEnv* env, jclass, jobject javaThread) {
  jobject trace = GetThreadStack(env, javaThread);
  if (trace == NULL) {
    return NULL;
  }
  return Thread::InternalStackTraceToStackTraceElementArray(env, trace);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(VMStack, fillStackTraceElements, "(Ljava/lang/Thread;[Ljava/lang/StackTraceElement;)I"),
  NATIVE_METHOD(VMStack, getCallingClassLoader, "()Ljava/lang/ClassLoader;"),
  NATIVE_METHOD(VMStack, getClosestUserClassLoader, "(Ljava/lang/ClassLoader;Ljava/lang/ClassLoader;)Ljava/lang/ClassLoader;"),
  NATIVE_METHOD(VMStack, getStackClass2, "()Ljava/lang/Class;"),
  NATIVE_METHOD(VMStack, getThreadStackTrace, "(Ljava/lang/Thread;)[Ljava/lang/StackTraceElement;"),
};

void register_dalvik_system_VMStack(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/VMStack");
}

}  // namespace art
