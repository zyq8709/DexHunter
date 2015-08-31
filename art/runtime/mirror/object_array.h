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

#ifndef ART_RUNTIME_MIRROR_OBJECT_ARRAY_H_
#define ART_RUNTIME_MIRROR_OBJECT_ARRAY_H_

#include "array.h"

namespace art {
namespace mirror {

template<class T>
class MANAGED ObjectArray : public Array {
 public:
  static ObjectArray<T>* Alloc(Thread* self, Class* object_array_class, int32_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  T* Get(int32_t i) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns true if the object can be stored into the array. If not, throws
  // an ArrayStoreException and returns false.
  bool CheckAssignable(T* object) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Set(int32_t i, T* object) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Set element without bound and element type checks, to be used in limited
  // circumstances, such as during boot image writing
  void SetWithoutChecks(int32_t i, T* object) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Set element without bound and element type checks, to be used in limited circumstances, such
  // as during boot image writing. Does not do write barrier.
  void SetPtrWithoutChecks(int32_t i, T* object) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  T* GetWithoutChecks(int32_t i) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void Copy(const ObjectArray<T>* src, int src_pos,
                   ObjectArray<T>* dst, int dst_pos,
                   size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ObjectArray<T>* CopyOf(Thread* self, int32_t new_length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ObjectArray);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_ARRAY_H_
