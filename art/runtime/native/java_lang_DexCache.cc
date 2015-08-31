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

#include "dex_file.h"
#include "mirror/dex_cache.h"
#include "mirror/object-inl.h"
#include "scoped_thread_state_change.h"
#include "well_known_classes.h"

namespace art {

static jobject DexCache_getDexNative(JNIEnv* env, jobject javaDexCache) {
  ScopedObjectAccess soa(env);
  mirror::DexCache* dex_cache = soa.Decode<mirror::DexCache*>(javaDexCache);
  // Should only be called while holding the lock on the dex cache.
  DCHECK_EQ(dex_cache->GetThinLockId(), soa.Self()->GetThinLockId());
  const DexFile* dex_file = dex_cache->GetDexFile();
  if (dex_file == NULL) {
    return NULL;
  }
  void* address = const_cast<void*>(reinterpret_cast<const void*>(dex_file->Begin()));
  jobject byte_buffer = env->NewDirectByteBuffer(address, dex_file->Size());
  if (byte_buffer == NULL) {
    DCHECK(soa.Self()->IsExceptionPending());
    return NULL;
  }

  jvalue args[1];
  args[0].l = byte_buffer;
  return env->CallStaticObjectMethodA(WellKnownClasses::com_android_dex_Dex,
                                      WellKnownClasses::com_android_dex_Dex_create,
                                      args);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(DexCache, getDexNative, "()Lcom/android/dex/Dex;"),
};

void register_java_lang_DexCache(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/DexCache");
}

}  // namespace art
