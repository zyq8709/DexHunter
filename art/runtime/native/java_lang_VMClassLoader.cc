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
#include "mirror/object-inl.h"
#include "scoped_thread_state_change.h"
#include "ScopedUtfChars.h"
#include "zip_archive.h"

namespace art {

static jclass VMClassLoader_findLoadedClass(JNIEnv* env, jclass, jobject javaLoader, jstring javaName) {
  ScopedObjectAccess soa(env);
  mirror::ClassLoader* loader = soa.Decode<mirror::ClassLoader*>(javaLoader);
  ScopedUtfChars name(env, javaName);
  if (name.c_str() == NULL) {
    return NULL;
  }

  std::string descriptor(DotToDescriptor(name.c_str()));
  mirror::Class* c = Runtime::Current()->GetClassLinker()->LookupClass(descriptor.c_str(), loader);
  if (c != NULL && c->IsResolved()) {
    return soa.AddLocalReference<jclass>(c);
  } else {
    // Class wasn't resolved so it may be erroneous or not yet ready, force the caller to go into
    // the regular loadClass code.
    return NULL;
  }
}

static jint VMClassLoader_getBootClassPathSize(JNIEnv*, jclass) {
  return Runtime::Current()->GetClassLinker()->GetBootClassPath().size();
}

/*
 * Returns a string URL for a resource with the specified 'javaName' in
 * entry 'index' of the boot class path.
 *
 * We return a newly-allocated String in the following form:
 *
 *   jar:file://path!/name
 *
 * Where "path" is the bootstrap class path entry and "name" is the string
 * passed into this method.  "path" needs to be an absolute path (starting
 * with '/'); if it's not we'd need to make it absolute as part of forming
 * the URL string.
 */
static jstring VMClassLoader_getBootClassPathResource(JNIEnv* env, jclass, jstring javaName, jint index) {
  ScopedUtfChars name(env, javaName);
  if (name.c_str() == NULL) {
    return NULL;
  }

  const std::vector<const DexFile*>& path = Runtime::Current()->GetClassLinker()->GetBootClassPath();
  if (index < 0 || size_t(index) >= path.size()) {
    return NULL;
  }
  const DexFile* dex_file = path[index];
  const std::string& location(dex_file->GetLocation());
  UniquePtr<ZipArchive> zip_archive(ZipArchive::Open(location));
  if (zip_archive.get() == NULL) {
    return NULL;
  }
  UniquePtr<ZipEntry> zip_entry(zip_archive->Find(name.c_str()));
  if (zip_entry.get() == NULL) {
    return NULL;
  }

  std::string url;
  StringAppendF(&url, "jar:file://%s!/%s", location.c_str(), name.c_str());
  return env->NewStringUTF(url.c_str());
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(VMClassLoader, findLoadedClass, "(Ljava/lang/ClassLoader;Ljava/lang/String;)Ljava/lang/Class;"),
  NATIVE_METHOD(VMClassLoader, getBootClassPathResource, "(Ljava/lang/String;I)Ljava/lang/String;"),
  NATIVE_METHOD(VMClassLoader, getBootClassPathSize, "()I"),
};

void register_java_lang_VMClassLoader(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/VMClassLoader");
}

}  // namespace art
