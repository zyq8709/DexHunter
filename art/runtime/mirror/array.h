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

#ifndef ART_RUNTIME_MIRROR_ARRAY_H_
#define ART_RUNTIME_MIRROR_ARRAY_H_

#include "object.h"

namespace art {
namespace mirror {

class MANAGED Array : public Object {
 public:
  // A convenience for code that doesn't know the component size,
  // and doesn't want to have to work it out itself.
  static Array* Alloc(Thread* self, Class* array_class, int32_t component_count)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static Array* Alloc(Thread* self, Class* array_class, int32_t component_count,
                      size_t component_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static Array* CreateMultiArray(Thread* self, Class* element_class, IntArray* dimensions)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t SizeOf() const;

  int32_t GetLength() const {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Array, length_), false);
  }

  void SetLength(int32_t length) {
    CHECK_GE(length, 0);
    SetField32(OFFSET_OF_OBJECT_MEMBER(Array, length_), length, false);
  }

  static MemberOffset LengthOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Array, length_);
  }

  static MemberOffset DataOffset(size_t component_size) {
    if (component_size != sizeof(int64_t)) {
      return OFFSET_OF_OBJECT_MEMBER(Array, first_element_);
    } else {
      // Align longs and doubles.
      return MemberOffset(OFFSETOF_MEMBER(Array, first_element_) + 4);
    }
  }

  void* GetRawData(size_t component_size) {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset(component_size).Int32Value();
    return reinterpret_cast<void*>(data);
  }

  const void* GetRawData(size_t component_size) const {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset(component_size).Int32Value();
    return reinterpret_cast<const void*>(data);
  }

  bool IsValidIndex(int32_t index) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (UNLIKELY(static_cast<uint32_t>(index) >= static_cast<uint32_t>(GetLength()))) {
      ThrowArrayIndexOutOfBoundsException(index);
      return false;
    }
    return true;
  }

 protected:
  void ThrowArrayIndexOutOfBoundsException(int32_t index) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void ThrowArrayStoreException(Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // The number of array elements.
  int32_t length_;
  // Marker for the data (used by generated code)
  uint32_t first_element_[0];

  DISALLOW_IMPLICIT_CONSTRUCTORS(Array);
};

template<class T>
class MANAGED PrimitiveArray : public Array {
 public:
  typedef T ElementType;

  static PrimitiveArray<T>* Alloc(Thread* self, size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const T* GetData() const {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset(sizeof(T)).Int32Value();
    return reinterpret_cast<T*>(data);
  }

  T* GetData() {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset(sizeof(T)).Int32Value();
    return reinterpret_cast<T*>(data);
  }

  T Get(int32_t i) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!IsValidIndex(i)) {
      return T(0);
    }
    return GetData()[i];
  }

  void Set(int32_t i, T value) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (IsValidIndex(i)) {
      GetData()[i] = value;
    }
  }

  static void SetArrayClass(Class* array_class) {
    CHECK(array_class_ == NULL);
    CHECK(array_class != NULL);
    array_class_ = array_class;
  }

  static void ResetArrayClass() {
    CHECK(array_class_ != NULL);
    array_class_ = NULL;
  }

 private:
  static Class* array_class_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PrimitiveArray);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ARRAY_H_
