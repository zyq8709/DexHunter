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

#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"

namespace art {

static mirror::ArtMethod* FindMethodHelper(uint32_t method_idx,
                                                mirror::Object* this_object,
                                                mirror::ArtMethod* caller_method,
                                                bool access_check,
                                                InvokeType type,
                                                Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtMethod* method = FindMethodFast(method_idx,
                                                  this_object,
                                                  caller_method,
                                                  access_check,
                                                  type);
  if (UNLIKELY(method == NULL)) {
    method = FindMethodFromCode(method_idx, this_object, caller_method,
                                thread, access_check, type);
    if (UNLIKELY(method == NULL)) {
      CHECK(thread->IsExceptionPending());
      return 0;  // failure
    }
  }
  DCHECK(!thread->IsExceptionPending());
  const void* code = method->GetEntryPointFromCompiledCode();

  // When we return, the caller will branch to this address, so it had better not be 0!
  if (UNLIKELY(code == NULL)) {
      MethodHelper mh(method);
      LOG(FATAL) << "Code was NULL in method: " << PrettyMethod(method)
                 << " location: " << mh.GetDexFile().GetLocation();
  }
  return method;
}

extern "C" mirror::Object* art_portable_find_static_method_from_code_with_access_check(uint32_t method_idx,
                                                                                       mirror::Object* this_object,
                                                                                       mirror::ArtMethod* referrer,
                                                                                       Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return FindMethodHelper(method_idx, this_object, referrer, true, kStatic, thread);
}

extern "C" mirror::Object* art_portable_find_direct_method_from_code_with_access_check(uint32_t method_idx,
                                                                                       mirror::Object* this_object,
                                                                                       mirror::ArtMethod* referrer,
                                                                                       Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return FindMethodHelper(method_idx, this_object, referrer, true, kDirect, thread);
}

extern "C" mirror::Object* art_portable_find_virtual_method_from_code_with_access_check(uint32_t method_idx,
                                                                                        mirror::Object* this_object,
                                                                                        mirror::ArtMethod* referrer,
                                                                                        Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return FindMethodHelper(method_idx, this_object, referrer, true, kVirtual, thread);
}

extern "C" mirror::Object* art_portable_find_super_method_from_code_with_access_check(uint32_t method_idx,
                                                                                      mirror::Object* this_object,
                                                                                      mirror::ArtMethod* referrer,
                                                                                      Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return FindMethodHelper(method_idx, this_object, referrer, true, kSuper, thread);
}

extern "C" mirror::Object* art_portable_find_interface_method_from_code_with_access_check(uint32_t method_idx,
                                                                                          mirror::Object* this_object,
                                                                                          mirror::ArtMethod* referrer,
                                                                                          Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return FindMethodHelper(method_idx, this_object, referrer, true, kInterface, thread);
}

extern "C" mirror::Object* art_portable_find_interface_method_from_code(uint32_t method_idx,
                                                                        mirror::Object* this_object,
                                                                        mirror::ArtMethod* referrer,
                                                                        Thread* thread)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return FindMethodHelper(method_idx, this_object, referrer, false, kInterface, thread);
}

}  // namespace art
