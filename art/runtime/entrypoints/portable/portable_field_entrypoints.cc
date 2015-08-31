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
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" int32_t art_portable_set32_static_from_code(uint32_t field_idx,
                                                       mirror::ArtMethod* referrer,
                                                       int32_t new_value)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx,
                               referrer,
                               StaticPrimitiveWrite,
                               sizeof(uint32_t));
  if (LIKELY(field != NULL)) {
    field->Set32(field->GetDeclaringClass(), new_value);
    return 0;
  }
  field = FindFieldFromCode(field_idx,
                            referrer,
                            Thread::Current(),
                            StaticPrimitiveWrite,
                            sizeof(uint32_t),
                            true);
  if (LIKELY(field != NULL)) {
    field->Set32(field->GetDeclaringClass(), new_value);
    return 0;
  }
  return -1;
}

extern "C" int32_t art_portable_set64_static_from_code(uint32_t field_idx,
                                                       mirror::ArtMethod* referrer,
                                                       int64_t new_value)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, StaticPrimitiveWrite, sizeof(uint64_t));
  if (LIKELY(field != NULL)) {
    field->Set64(field->GetDeclaringClass(), new_value);
    return 0;
  }
  field = FindFieldFromCode(field_idx,
                            referrer,
                            Thread::Current(),
                            StaticPrimitiveWrite,
                            sizeof(uint64_t),
                            true);
  if (LIKELY(field != NULL)) {
    field->Set64(field->GetDeclaringClass(), new_value);
    return 0;
  }
  return -1;
}

extern "C" int32_t art_portable_set_obj_static_from_code(uint32_t field_idx,
                                                         mirror::ArtMethod* referrer,
                                                         mirror::Object* new_value)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, StaticObjectWrite,
                                          sizeof(mirror::Object*));
  if (LIKELY(field != NULL)) {
    field->SetObj(field->GetDeclaringClass(), new_value);
    return 0;
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            StaticObjectWrite, sizeof(mirror::Object*), true);
  if (LIKELY(field != NULL)) {
    field->SetObj(field->GetDeclaringClass(), new_value);
    return 0;
  }
  return -1;
}

extern "C" int32_t art_portable_get32_static_from_code(uint32_t field_idx,
                                                       mirror::ArtMethod* referrer)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, StaticPrimitiveRead, sizeof(uint32_t));
  if (LIKELY(field != NULL)) {
    return field->Get32(field->GetDeclaringClass());
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            StaticPrimitiveRead, sizeof(uint32_t), true);
  if (LIKELY(field != NULL)) {
    return field->Get32(field->GetDeclaringClass());
  }
  return 0;
}

extern "C" int64_t art_portable_get64_static_from_code(uint32_t field_idx,
                                                       mirror::ArtMethod* referrer)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, StaticPrimitiveRead, sizeof(uint64_t));
  if (LIKELY(field != NULL)) {
    return field->Get64(field->GetDeclaringClass());
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            StaticPrimitiveRead, sizeof(uint64_t), true);
  if (LIKELY(field != NULL)) {
    return field->Get64(field->GetDeclaringClass());
  }
  return 0;
}

extern "C" mirror::Object* art_portable_get_obj_static_from_code(uint32_t field_idx,
                                                                 mirror::ArtMethod* referrer)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, StaticObjectRead,
                                          sizeof(mirror::Object*));
  if (LIKELY(field != NULL)) {
    return field->GetObj(field->GetDeclaringClass());
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            StaticObjectRead, sizeof(mirror::Object*), true);
  if (LIKELY(field != NULL)) {
    return field->GetObj(field->GetDeclaringClass());
  }
  return 0;
}

extern "C" int32_t art_portable_set32_instance_from_code(uint32_t field_idx,
                                                         mirror::ArtMethod* referrer,
                                                         mirror::Object* obj, uint32_t new_value)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, InstancePrimitiveWrite, sizeof(uint32_t));
  if (LIKELY(field != NULL)) {
    field->Set32(obj, new_value);
    return 0;
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            InstancePrimitiveWrite, sizeof(uint32_t), true);
  if (LIKELY(field != NULL)) {
    field->Set32(obj, new_value);
    return 0;
  }
  return -1;
}

extern "C" int32_t art_portable_set64_instance_from_code(uint32_t field_idx,
                                                         mirror::ArtMethod* referrer,
                                                         mirror::Object* obj, int64_t new_value)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, InstancePrimitiveWrite, sizeof(uint64_t));
  if (LIKELY(field != NULL)) {
    field->Set64(obj, new_value);
    return 0;
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            InstancePrimitiveWrite, sizeof(uint64_t), true);
  if (LIKELY(field != NULL)) {
    field->Set64(obj, new_value);
    return 0;
  }
  return -1;
}

extern "C" int32_t art_portable_set_obj_instance_from_code(uint32_t field_idx,
                                                           mirror::ArtMethod* referrer,
                                                           mirror::Object* obj,
                                                           mirror::Object* new_value)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, InstanceObjectWrite,
                                          sizeof(mirror::Object*));
  if (LIKELY(field != NULL)) {
    field->SetObj(obj, new_value);
    return 0;
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            InstanceObjectWrite, sizeof(mirror::Object*), true);
  if (LIKELY(field != NULL)) {
    field->SetObj(obj, new_value);
    return 0;
  }
  return -1;
}

extern "C" int32_t art_portable_get32_instance_from_code(uint32_t field_idx,
                                                         mirror::ArtMethod* referrer,
                                                         mirror::Object* obj)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, InstancePrimitiveRead, sizeof(uint32_t));
  if (LIKELY(field != NULL)) {
    return field->Get32(obj);
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            InstancePrimitiveRead, sizeof(uint32_t), true);
  if (LIKELY(field != NULL)) {
    return field->Get32(obj);
  }
  return 0;
}

extern "C" int64_t art_portable_get64_instance_from_code(uint32_t field_idx,
                                                         mirror::ArtMethod* referrer,
                                                         mirror::Object* obj)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, InstancePrimitiveRead, sizeof(uint64_t));
  if (LIKELY(field != NULL)) {
    return field->Get64(obj);
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            InstancePrimitiveRead, sizeof(uint64_t), true);
  if (LIKELY(field != NULL)) {
    return field->Get64(obj);
  }
  return 0;
}

extern "C" mirror::Object* art_portable_get_obj_instance_from_code(uint32_t field_idx,
                                                                   mirror::ArtMethod* referrer,
                                                                   mirror::Object* obj)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtField* field = FindFieldFast(field_idx, referrer, InstanceObjectRead,
                                          sizeof(mirror::Object*));
  if (LIKELY(field != NULL)) {
    return field->GetObj(obj);
  }
  field = FindFieldFromCode(field_idx, referrer, Thread::Current(),
                            InstanceObjectRead, sizeof(mirror::Object*), true);
  if (LIKELY(field != NULL)) {
    return field->GetObj(obj);
  }
  return 0;
}

}  // namespace art
