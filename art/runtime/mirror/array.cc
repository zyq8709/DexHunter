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

#include "array.h"

#include "class.h"
#include "class-inl.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "object-inl.h"
#include "object_array.h"
#include "object_array-inl.h"
#include "object_utils.h"
#include "sirt_ref.h"
#include "thread.h"
#include "utils.h"

namespace art {
namespace mirror {

Array* Array::Alloc(Thread* self, Class* array_class, int32_t component_count,
                    size_t component_size) {
  DCHECK(array_class != NULL);
  DCHECK_GE(component_count, 0);
  DCHECK(array_class->IsArrayClass());

  size_t header_size = sizeof(Object) + (component_size == sizeof(int64_t) ? 8 : 4);
  size_t data_size = component_count * component_size;
  size_t size = header_size + data_size;

  // Check for overflow and throw OutOfMemoryError if this was an unreasonable request.
  size_t component_shift = sizeof(size_t) * 8 - 1 - CLZ(component_size);
  if (UNLIKELY(data_size >> component_shift != size_t(component_count) || size < data_size)) {
    self->ThrowOutOfMemoryError(StringPrintf("%s of length %d would overflow",
                                             PrettyDescriptor(array_class).c_str(),
                                             component_count).c_str());
    return NULL;
  }

  gc::Heap* heap = Runtime::Current()->GetHeap();
  Array* array = down_cast<Array*>(heap->AllocObject(self, array_class, size));
  if (array != NULL) {
    DCHECK(array->IsArrayInstance());
    array->SetLength(component_count);
  }
  return array;
}

Array* Array::Alloc(Thread* self, Class* array_class, int32_t component_count) {
  DCHECK(array_class->IsArrayClass());
  return Alloc(self, array_class, component_count, array_class->GetComponentSize());
}

// Create a multi-dimensional array of Objects or primitive types.
//
// We have to generate the names for X[], X[][], X[][][], and so on.  The
// easiest way to deal with that is to create the full name once and then
// subtract pieces off.  Besides, we want to start with the outermost
// piece and work our way in.
// Recursively create an array with multiple dimensions.  Elements may be
// Objects or primitive types.
static Array* RecursiveCreateMultiArray(Thread* self, Class* array_class, int current_dimension,
                                        IntArray* dimensions)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  int32_t array_length = dimensions->Get(current_dimension);
  SirtRef<Array> new_array(self, Array::Alloc(self, array_class, array_length));
  if (UNLIKELY(new_array.get() == NULL)) {
    CHECK(self->IsExceptionPending());
    return NULL;
  }
  if ((current_dimension + 1) < dimensions->GetLength()) {
    // Create a new sub-array in every element of the array.
    for (int32_t i = 0; i < array_length; i++) {
      Array* sub_array = RecursiveCreateMultiArray(self, array_class->GetComponentType(),
                                                   current_dimension + 1, dimensions);
      if (UNLIKELY(sub_array == NULL)) {
        CHECK(self->IsExceptionPending());
        return NULL;
      }
      new_array->AsObjectArray<Array>()->Set(i, sub_array);
    }
  }
  return new_array.get();
}

Array* Array::CreateMultiArray(Thread* self, Class* element_class, IntArray* dimensions) {
  // Verify dimensions.
  //
  // The caller is responsible for verifying that "dimArray" is non-null
  // and has a length > 0 and <= 255.
  int num_dimensions = dimensions->GetLength();
  DCHECK_GT(num_dimensions, 0);
  DCHECK_LE(num_dimensions, 255);

  for (int i = 0; i < num_dimensions; i++) {
    int dimension = dimensions->Get(i);
    if (UNLIKELY(dimension < 0)) {
      ThrowNegativeArraySizeException(StringPrintf("Dimension %d: %d", i, dimension).c_str());
      return NULL;
    }
  }

  // Generate the full name of the array class.
  std::string descriptor(num_dimensions, '[');
  descriptor += ClassHelper(element_class).GetDescriptor();

  // Find/generate the array class.
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Class* array_class = class_linker->FindClass(descriptor.c_str(), element_class->GetClassLoader());
  if (UNLIKELY(array_class == NULL)) {
    CHECK(self->IsExceptionPending());
    return NULL;
  }
  // create the array
  Array* new_array = RecursiveCreateMultiArray(self, array_class, 0, dimensions);
  if (UNLIKELY(new_array == NULL)) {
    CHECK(self->IsExceptionPending());
    return NULL;
  }
  return new_array;
}

void Array::ThrowArrayIndexOutOfBoundsException(int32_t index) const {
  art::ThrowArrayIndexOutOfBoundsException(index, GetLength());
}

void Array::ThrowArrayStoreException(Object* object) const {
  art::ThrowArrayStoreException(object->GetClass(), this->GetClass());
}

template<typename T>
PrimitiveArray<T>* PrimitiveArray<T>::Alloc(Thread* self, size_t length) {
  DCHECK(array_class_ != NULL);
  Array* raw_array = Array::Alloc(self, array_class_, length, sizeof(T));
  return down_cast<PrimitiveArray<T>*>(raw_array);
}

template <typename T> Class* PrimitiveArray<T>::array_class_ = NULL;

// Explicitly instantiate all the primitive array types.
template class PrimitiveArray<uint8_t>;   // BooleanArray
template class PrimitiveArray<int8_t>;    // ByteArray
template class PrimitiveArray<uint16_t>;  // CharArray
template class PrimitiveArray<double>;    // DoubleArray
template class PrimitiveArray<float>;     // FloatArray
template class PrimitiveArray<int32_t>;   // IntArray
template class PrimitiveArray<int64_t>;   // LongArray
template class PrimitiveArray<int16_t>;   // ShortArray

}  // namespace mirror
}  // namespace art
