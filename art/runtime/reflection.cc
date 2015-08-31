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

#include "reflection.h"

#include "class_linker.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "invoke_arg_array_builder.h"
#include "jni_internal.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/object_array.h"
#include "mirror/object_array-inl.h"
#include "object_utils.h"
#include "scoped_thread_state_change.h"
#include "well_known_classes.h"

namespace art {

jobject InvokeMethod(const ScopedObjectAccess& soa, jobject javaMethod, jobject javaReceiver,
                     jobject javaArgs) {
  jmethodID mid = soa.Env()->FromReflectedMethod(javaMethod);
  mirror::ArtMethod* m = soa.DecodeMethod(mid);

  mirror::Class* declaring_class = m->GetDeclaringClass();
  if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(declaring_class, true, true)) {
    return NULL;
  }

  mirror::Object* receiver = NULL;
  if (!m->IsStatic()) {
    // Check that the receiver is non-null and an instance of the field's declaring class.
    receiver = soa.Decode<mirror::Object*>(javaReceiver);
    if (!VerifyObjectInClass(receiver, declaring_class)) {
      return NULL;
    }

    // Find the actual implementation of the virtual method.
    m = receiver->GetClass()->FindVirtualMethodForVirtualOrInterface(m);
    mid = soa.EncodeMethod(m);
  }

  // Get our arrays of arguments and their types, and check they're the same size.
  mirror::ObjectArray<mirror::Object>* objects =
      soa.Decode<mirror::ObjectArray<mirror::Object>*>(javaArgs);
  MethodHelper mh(m);
  const DexFile::TypeList* classes = mh.GetParameterTypeList();
  uint32_t classes_size = classes == NULL ? 0 : classes->Size();
  uint32_t arg_count = (objects != NULL) ? objects->GetLength() : 0;
  if (arg_count != classes_size) {
    ThrowIllegalArgumentException(NULL,
                                  StringPrintf("Wrong number of arguments; expected %d, got %d",
                                               classes_size, arg_count).c_str());
    return NULL;
  }

  // Translate javaArgs to a jvalue[].
  UniquePtr<jvalue[]> args(new jvalue[arg_count]);
  JValue* decoded_args = reinterpret_cast<JValue*>(args.get());
  for (uint32_t i = 0; i < arg_count; ++i) {
    mirror::Object* arg = objects->Get(i);
    mirror::Class* dst_class = mh.GetClassFromTypeIdx(classes->GetTypeItem(i).type_idx_);
    if (!UnboxPrimitiveForArgument(arg, dst_class, decoded_args[i], m, i)) {
      return NULL;
    }
    if (!dst_class->IsPrimitive()) {
      args[i].l = soa.AddLocalReference<jobject>(arg);
    }
  }

  // Invoke the method.
  JValue value(InvokeWithJValues(soa, javaReceiver, mid, args.get()));

  // Wrap any exception with "Ljava/lang/reflect/InvocationTargetException;" and return early.
  if (soa.Self()->IsExceptionPending()) {
    jthrowable th = soa.Env()->ExceptionOccurred();
    soa.Env()->ExceptionClear();
    jclass exception_class = soa.Env()->FindClass("java/lang/reflect/InvocationTargetException");
    jmethodID mid = soa.Env()->GetMethodID(exception_class, "<init>", "(Ljava/lang/Throwable;)V");
    jobject exception_instance = soa.Env()->NewObject(exception_class, mid, th);
    soa.Env()->Throw(reinterpret_cast<jthrowable>(exception_instance));
    return NULL;
  }

  // Box if necessary and return.
  return soa.AddLocalReference<jobject>(BoxPrimitive(mh.GetReturnType()->GetPrimitiveType(), value));
}

bool VerifyObjectInClass(mirror::Object* o, mirror::Class* c) {
  if (o == NULL) {
    ThrowNullPointerException(NULL, "null receiver");
    return false;
  } else if (!o->InstanceOf(c)) {
    std::string expected_class_name(PrettyDescriptor(c));
    std::string actual_class_name(PrettyTypeOf(o));
    ThrowIllegalArgumentException(NULL,
                                  StringPrintf("Expected receiver of type %s, but got %s",
                                               expected_class_name.c_str(),
                                               actual_class_name.c_str()).c_str());
    return false;
  }
  return true;
}

bool ConvertPrimitiveValue(const ThrowLocation* throw_location, bool unbox_for_result,
                           Primitive::Type srcType, Primitive::Type dstType,
                           const JValue& src, JValue& dst) {
  CHECK(srcType != Primitive::kPrimNot && dstType != Primitive::kPrimNot);
  switch (dstType) {
  case Primitive::kPrimBoolean:
    if (srcType == Primitive::kPrimBoolean) {
      dst.SetZ(src.GetZ());
      return true;
    }
    break;
  case Primitive::kPrimChar:
    if (srcType == Primitive::kPrimChar) {
      dst.SetC(src.GetC());
      return true;
    }
    break;
  case Primitive::kPrimByte:
    if (srcType == Primitive::kPrimByte) {
      dst.SetB(src.GetB());
      return true;
    }
    break;
  case Primitive::kPrimShort:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimShort) {
      dst.SetS(src.GetI());
      return true;
    }
    break;
  case Primitive::kPrimInt:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimChar ||
        srcType == Primitive::kPrimShort || srcType == Primitive::kPrimInt) {
      dst.SetI(src.GetI());
      return true;
    }
    break;
  case Primitive::kPrimLong:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimChar ||
        srcType == Primitive::kPrimShort || srcType == Primitive::kPrimInt) {
      dst.SetJ(src.GetI());
      return true;
    } else if (srcType == Primitive::kPrimLong) {
      dst.SetJ(src.GetJ());
      return true;
    }
    break;
  case Primitive::kPrimFloat:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimChar ||
        srcType == Primitive::kPrimShort || srcType == Primitive::kPrimInt) {
      dst.SetF(src.GetI());
      return true;
    } else if (srcType == Primitive::kPrimLong) {
      dst.SetF(src.GetJ());
      return true;
    } else if (srcType == Primitive::kPrimFloat) {
      dst.SetF(src.GetF());
      return true;
    }
    break;
  case Primitive::kPrimDouble:
    if (srcType == Primitive::kPrimByte || srcType == Primitive::kPrimChar ||
        srcType == Primitive::kPrimShort || srcType == Primitive::kPrimInt) {
      dst.SetD(src.GetI());
      return true;
    } else if (srcType == Primitive::kPrimLong) {
      dst.SetD(src.GetJ());
      return true;
    } else if (srcType == Primitive::kPrimFloat) {
      dst.SetD(src.GetF());
      return true;
    } else if (srcType == Primitive::kPrimDouble) {
      dst.SetJ(src.GetJ());
      return true;
    }
    break;
  default:
    break;
  }
  if (!unbox_for_result) {
    ThrowIllegalArgumentException(throw_location,
                                  StringPrintf("Invalid primitive conversion from %s to %s",
                                               PrettyDescriptor(srcType).c_str(),
                                               PrettyDescriptor(dstType).c_str()).c_str());
  } else {
    ThrowClassCastException(throw_location,
                            StringPrintf("Couldn't convert result of type %s to %s",
                                         PrettyDescriptor(srcType).c_str(),
                                         PrettyDescriptor(dstType).c_str()).c_str());
  }
  return false;
}

mirror::Object* BoxPrimitive(Primitive::Type src_class, const JValue& value) {
  if (src_class == Primitive::kPrimNot) {
    return value.GetL();
  }

  jmethodID m = NULL;
  switch (src_class) {
  case Primitive::kPrimBoolean:
    m = WellKnownClasses::java_lang_Boolean_valueOf;
    break;
  case Primitive::kPrimByte:
    m = WellKnownClasses::java_lang_Byte_valueOf;
    break;
  case Primitive::kPrimChar:
    m = WellKnownClasses::java_lang_Character_valueOf;
    break;
  case Primitive::kPrimDouble:
    m = WellKnownClasses::java_lang_Double_valueOf;
    break;
  case Primitive::kPrimFloat:
    m = WellKnownClasses::java_lang_Float_valueOf;
    break;
  case Primitive::kPrimInt:
    m = WellKnownClasses::java_lang_Integer_valueOf;
    break;
  case Primitive::kPrimLong:
    m = WellKnownClasses::java_lang_Long_valueOf;
    break;
  case Primitive::kPrimShort:
    m = WellKnownClasses::java_lang_Short_valueOf;
    break;
  case Primitive::kPrimVoid:
    // There's no such thing as a void field, and void methods invoked via reflection return null.
    return NULL;
  default:
    LOG(FATAL) << static_cast<int>(src_class);
  }

  ScopedObjectAccessUnchecked soa(Thread::Current());
  if (kIsDebugBuild) {
    CHECK_EQ(soa.Self()->GetState(), kRunnable);
  }

  ArgArray arg_array(NULL, 0);
  JValue result;
  if (src_class == Primitive::kPrimDouble || src_class == Primitive::kPrimLong) {
    arg_array.AppendWide(value.GetJ());
  } else {
    arg_array.Append(value.GetI());
  }

  soa.DecodeMethod(m)->Invoke(soa.Self(), arg_array.GetArray(), arg_array.GetNumBytes(),
                              &result, 'L');
  return result.GetL();
}

static std::string UnboxingFailureKind(mirror::ArtMethod* m, int index, mirror::ArtField* f)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (m != NULL && index != -1) {
    ++index;  // Humans count from 1.
    return StringPrintf("method %s argument %d", PrettyMethod(m, false).c_str(), index);
  }
  if (f != NULL) {
    return "field " + PrettyField(f, false);
  }
  return "result";
}

static bool UnboxPrimitive(const ThrowLocation* throw_location, mirror::Object* o,
                           mirror::Class* dst_class, JValue& unboxed_value,
                           mirror::ArtMethod* m, int index, mirror::ArtField* f)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  bool unbox_for_result = (f == NULL) && (index == -1);
  if (!dst_class->IsPrimitive()) {
    if (UNLIKELY(o != NULL && !o->InstanceOf(dst_class))) {
      if (!unbox_for_result) {
        ThrowIllegalArgumentException(throw_location,
                                      StringPrintf("%s has type %s, got %s",
                                                   UnboxingFailureKind(m, index, f).c_str(),
                                                   PrettyDescriptor(dst_class).c_str(),
                                                   PrettyTypeOf(o).c_str()).c_str());
      } else {
        ThrowClassCastException(throw_location,
                                StringPrintf("Couldn't convert result of type %s to %s",
                                             PrettyTypeOf(o).c_str(),
                                             PrettyDescriptor(dst_class).c_str()).c_str());
      }
      return false;
    }
    unboxed_value.SetL(o);
    return true;
  }
  if (UNLIKELY(dst_class->GetPrimitiveType() == Primitive::kPrimVoid)) {
    ThrowIllegalArgumentException(throw_location,
                                  StringPrintf("Can't unbox %s to void",
                                               UnboxingFailureKind(m, index, f).c_str()).c_str());
    return false;
  }
  if (UNLIKELY(o == NULL)) {
    if (!unbox_for_result) {
      ThrowIllegalArgumentException(throw_location,
                                    StringPrintf("%s has type %s, got null",
                                                 UnboxingFailureKind(m, index, f).c_str(),
                                                 PrettyDescriptor(dst_class).c_str()).c_str());
    } else {
      ThrowNullPointerException(throw_location,
                                StringPrintf("Expected to unbox a '%s' primitive type but was returned null",
                                             PrettyDescriptor(dst_class).c_str()).c_str());
    }
    return false;
  }

  JValue boxed_value;
  std::string src_descriptor(ClassHelper(o->GetClass()).GetDescriptor());
  mirror::Class* src_class = NULL;
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  mirror::ArtField* primitive_field = o->GetClass()->GetIFields()->Get(0);
  if (src_descriptor == "Ljava/lang/Boolean;") {
    src_class = class_linker->FindPrimitiveClass('Z');
    boxed_value.SetZ(primitive_field->GetBoolean(o));
  } else if (src_descriptor == "Ljava/lang/Byte;") {
    src_class = class_linker->FindPrimitiveClass('B');
    boxed_value.SetB(primitive_field->GetByte(o));
  } else if (src_descriptor == "Ljava/lang/Character;") {
    src_class = class_linker->FindPrimitiveClass('C');
    boxed_value.SetC(primitive_field->GetChar(o));
  } else if (src_descriptor == "Ljava/lang/Float;") {
    src_class = class_linker->FindPrimitiveClass('F');
    boxed_value.SetF(primitive_field->GetFloat(o));
  } else if (src_descriptor == "Ljava/lang/Double;") {
    src_class = class_linker->FindPrimitiveClass('D');
    boxed_value.SetD(primitive_field->GetDouble(o));
  } else if (src_descriptor == "Ljava/lang/Integer;") {
    src_class = class_linker->FindPrimitiveClass('I');
    boxed_value.SetI(primitive_field->GetInt(o));
  } else if (src_descriptor == "Ljava/lang/Long;") {
    src_class = class_linker->FindPrimitiveClass('J');
    boxed_value.SetJ(primitive_field->GetLong(o));
  } else if (src_descriptor == "Ljava/lang/Short;") {
    src_class = class_linker->FindPrimitiveClass('S');
    boxed_value.SetS(primitive_field->GetShort(o));
  } else {
    ThrowIllegalArgumentException(throw_location,
                                  StringPrintf("%s has type %s, got %s",
                                               UnboxingFailureKind(m, index, f).c_str(),
                                               PrettyDescriptor(dst_class).c_str(),
                                               PrettyDescriptor(src_descriptor.c_str()).c_str()).c_str());
    return false;
  }

  return ConvertPrimitiveValue(throw_location, unbox_for_result,
                               src_class->GetPrimitiveType(), dst_class->GetPrimitiveType(),
                               boxed_value, unboxed_value);
}

bool UnboxPrimitiveForArgument(mirror::Object* o, mirror::Class* dst_class, JValue& unboxed_value,
                               mirror::ArtMethod* m, size_t index) {
  CHECK(m != NULL);
  return UnboxPrimitive(NULL, o, dst_class, unboxed_value, m, index, NULL);
}

bool UnboxPrimitiveForField(mirror::Object* o, mirror::Class* dst_class, JValue& unboxed_value,
                            mirror::ArtField* f) {
  CHECK(f != NULL);
  return UnboxPrimitive(NULL, o, dst_class, unboxed_value, NULL, -1, f);
}

bool UnboxPrimitiveForResult(const ThrowLocation& throw_location, mirror::Object* o,
                             mirror::Class* dst_class, JValue& unboxed_value) {
  return UnboxPrimitive(&throw_location, o, dst_class, unboxed_value, NULL, -1, NULL);
}

}  // namespace art
