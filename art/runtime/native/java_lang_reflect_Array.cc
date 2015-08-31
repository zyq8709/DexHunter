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
#include "common_throws.h"
#include "dex_file-inl.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "scoped_thread_state_change.h"
#include "sirt_ref.h"

namespace art {

static jobject Array_createMultiArray(JNIEnv* env, jclass, jclass javaElementClass, jobject javaDimArray) {
  ScopedObjectAccess soa(env);
  DCHECK(javaElementClass != NULL);
  mirror::Class* element_class = soa.Decode<mirror::Class*>(javaElementClass);
  DCHECK(element_class->IsClass());
  DCHECK(javaDimArray != NULL);
  mirror::Object* dimensions_obj = soa.Decode<mirror::Object*>(javaDimArray);
  DCHECK(dimensions_obj->IsArrayInstance());
  DCHECK_STREQ(ClassHelper(dimensions_obj->GetClass()).GetDescriptor(), "[I");
  mirror::IntArray* dimensions_array = down_cast<mirror::IntArray*>(dimensions_obj);
  mirror::Array* new_array = mirror::Array::CreateMultiArray(soa.Self(), element_class, dimensions_array);
  return soa.AddLocalReference<jobject>(new_array);
}

static jobject Array_createObjectArray(JNIEnv* env, jclass, jclass javaElementClass, jint length) {
  ScopedObjectAccess soa(env);
  DCHECK(javaElementClass != NULL);
  mirror::Class* element_class = soa.Decode<mirror::Class*>(javaElementClass);
  if (UNLIKELY(length < 0)) {
    ThrowNegativeArraySizeException(length);
    return NULL;
  }
  std::string descriptor("[");
  descriptor += ClassHelper(element_class).GetDescriptor();

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  mirror::Class* array_class = class_linker->FindClass(descriptor.c_str(), element_class->GetClassLoader());
  if (UNLIKELY(array_class == NULL)) {
    CHECK(soa.Self()->IsExceptionPending());
    return NULL;
  }
  DCHECK(array_class->IsArrayClass());
  mirror::Array* new_array = mirror::Array::Alloc(soa.Self(), array_class, length);
  return soa.AddLocalReference<jobject>(new_array);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Array, createMultiArray, "(Ljava/lang/Class;[I)Ljava/lang/Object;"),
  NATIVE_METHOD(Array, createObjectArray, "(Ljava/lang/Class;I)Ljava/lang/Object;"),
};

void register_java_lang_reflect_Array(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/reflect/Array");
}

}  // namespace art
