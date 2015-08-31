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

#ifndef ART_RUNTIME_MIRROR_OBJECT_ARRAY_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_ARRAY_INL_H_

#include "object_array.h"

#include "gc/heap.h"
#include "mirror/art_field.h"
#include "mirror/class.h"
#include "runtime.h"
#include "thread.h"

namespace art {
namespace mirror {

template<class T>
inline ObjectArray<T>* ObjectArray<T>::Alloc(Thread* self, Class* object_array_class, int32_t length) {
  Array* array = Array::Alloc(self, object_array_class, length, sizeof(Object*));
  if (UNLIKELY(array == NULL)) {
    return NULL;
  } else {
    return array->AsObjectArray<T>();
  }
}

template<class T>
inline T* ObjectArray<T>::Get(int32_t i) const {
  if (UNLIKELY(!IsValidIndex(i))) {
    return NULL;
  }
  MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
  return GetFieldObject<T*>(data_offset, false);
}

template<class T>
inline bool ObjectArray<T>::CheckAssignable(T* object) {
  if (object != NULL) {
    Class* element_class = GetClass()->GetComponentType();
    if (UNLIKELY(!object->InstanceOf(element_class))) {
      ThrowArrayStoreException(object);
      return false;
    }
  }
  return true;
}

template<class T>
inline void ObjectArray<T>::Set(int32_t i, T* object) {
  if (LIKELY(IsValidIndex(i) && CheckAssignable(object))) {
    MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
    SetFieldObject(data_offset, object, false);
  } else {
    DCHECK(Thread::Current()->IsExceptionPending());
  }
}

template<class T>
inline void ObjectArray<T>::SetWithoutChecks(int32_t i, T* object) {
  DCHECK(IsValidIndex(i));
  MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
  SetFieldObject(data_offset, object, false);
}

template<class T>
inline void ObjectArray<T>::SetPtrWithoutChecks(int32_t i, T* object) {
  DCHECK(IsValidIndex(i));
  MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
  SetFieldPtr(data_offset, object, false);
}

template<class T>
inline T* ObjectArray<T>::GetWithoutChecks(int32_t i) const {
  DCHECK(IsValidIndex(i));
  MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
  return GetFieldObject<T*>(data_offset, false);
}

template<class T>
inline void ObjectArray<T>::Copy(const ObjectArray<T>* src, int src_pos,
                                 ObjectArray<T>* dst, int dst_pos,
                                 size_t length) {
  if (src->IsValidIndex(src_pos) &&
      src->IsValidIndex(src_pos+length-1) &&
      dst->IsValidIndex(dst_pos) &&
      dst->IsValidIndex(dst_pos+length-1)) {
    MemberOffset src_offset(DataOffset(sizeof(Object*)).Int32Value() + src_pos * sizeof(Object*));
    MemberOffset dst_offset(DataOffset(sizeof(Object*)).Int32Value() + dst_pos * sizeof(Object*));
    Class* array_class = dst->GetClass();
    gc::Heap* heap = Runtime::Current()->GetHeap();
    if (array_class == src->GetClass()) {
      // No need for array store checks if arrays are of the same type
      for (size_t i = 0; i < length; i++) {
        Object* object = src->GetFieldObject<Object*>(src_offset, false);
        heap->VerifyObject(object);
        // directly set field, we do a bulk write barrier at the end
        dst->SetField32(dst_offset, reinterpret_cast<uint32_t>(object), false, true);
        src_offset = MemberOffset(src_offset.Uint32Value() + sizeof(Object*));
        dst_offset = MemberOffset(dst_offset.Uint32Value() + sizeof(Object*));
      }
    } else {
      Class* element_class = array_class->GetComponentType();
      CHECK(!element_class->IsPrimitive());
      for (size_t i = 0; i < length; i++) {
        Object* object = src->GetFieldObject<Object*>(src_offset, false);
        if (object != NULL && !object->InstanceOf(element_class)) {
          dst->ThrowArrayStoreException(object);
          return;
        }
        heap->VerifyObject(object);
        // directly set field, we do a bulk write barrier at the end
        dst->SetField32(dst_offset, reinterpret_cast<uint32_t>(object), false, true);
        src_offset = MemberOffset(src_offset.Uint32Value() + sizeof(Object*));
        dst_offset = MemberOffset(dst_offset.Uint32Value() + sizeof(Object*));
      }
    }
    heap->WriteBarrierArray(dst, dst_pos, length);
  }
}

template<class T>
inline ObjectArray<T>* ObjectArray<T>::CopyOf(Thread* self, int32_t new_length) {
  ObjectArray<T>* new_array = Alloc(self, GetClass(), new_length);
  if (LIKELY(new_array != NULL)) {
    Copy(this, 0, new_array, 0, std::min(GetLength(), new_length));
  }
  return new_array;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_ARRAY_INL_H_
