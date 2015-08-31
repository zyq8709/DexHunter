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

#include "class_linker.h"
#include "jni_internal.h"
#include "mirror/class_loader.h"
#include "mirror/object_array.h"
#include "mirror/string.h"
#include "scoped_thread_state_change.h"

namespace art {

static jclass Proxy_generateProxy(JNIEnv* env, jclass, jstring javaName,
                                  jobjectArray javaInterfaces, jobject javaLoader,
                                  jobjectArray javaMethods, jobjectArray javaThrows) {
  ScopedObjectAccess soa(env);
  mirror::String* name = soa.Decode<mirror::String*>(javaName);
  mirror::ObjectArray<mirror::Class>* interfaces =
      soa.Decode<mirror::ObjectArray<mirror::Class>*>(javaInterfaces);
  mirror::ClassLoader* loader = soa.Decode<mirror::ClassLoader*>(javaLoader);
  mirror::ObjectArray<mirror::ArtMethod>* methods =
      soa.Decode<mirror::ObjectArray<mirror::ArtMethod>*>(javaMethods);
  mirror::ObjectArray<mirror::ObjectArray<mirror::Class> >* throws =
      soa.Decode<mirror::ObjectArray<mirror::ObjectArray<mirror::Class> >*>(javaThrows);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  mirror::Class* result = class_linker->CreateProxyClass(name, interfaces, loader, methods, throws);
  return soa.AddLocalReference<jclass>(result);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Proxy, generateProxy, "(Ljava/lang/String;[Ljava/lang/Class;Ljava/lang/ClassLoader;[Ljava/lang/reflect/ArtMethod;[[Ljava/lang/Class;)Ljava/lang/Class;"),
};

void register_java_lang_reflect_Proxy(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/reflect/Proxy");
}

}  // namespace art
