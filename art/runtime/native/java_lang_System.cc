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

#include "common_throws.h"
#include "gc/accounting/card_table-inl.h"
#include "jni_internal.h"
#include "mirror/array.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "scoped_thread_state_change.h"

/*
 * We make guarantees about the atomicity of accesses to primitive
 * variables.  These guarantees also apply to elements of arrays.
 * In particular, 8-bit, 16-bit, and 32-bit accesses must be atomic and
 * must not cause "word tearing".  Accesses to 64-bit array elements must
 * either be atomic or treated as two 32-bit operations.  References are
 * always read and written atomically, regardless of the number of bits
 * used to represent them.
 *
 * We can't rely on standard libc functions like memcpy(3) and memmove(3)
 * in our implementation of System.arraycopy, because they may copy
 * byte-by-byte (either for the full run or for "unaligned" parts at the
 * start or end).  We need to use functions that guarantee 16-bit or 32-bit
 * atomicity as appropriate.
 *
 * System.arraycopy() is heavily used, so having an efficient implementation
 * is important.  The bionic libc provides a platform-optimized memory move
 * function that should be used when possible.  If it's not available,
 * the trivial "reference implementation" versions below can be used until
 * a proper version can be written.
 *
 * For these functions, The caller must guarantee that dst/src are aligned
 * appropriately for the element type, and that n is a multiple of the
 * element size.
 */

/*
 * Works like memmove(), except:
 * - if all arguments are at least 32-bit aligned, we guarantee that we
 *   will use operations that preserve atomicity of 32-bit values
 * - if not, we guarantee atomicity of 16-bit values
 *
 * If all three arguments are not at least 16-bit aligned, the behavior
 * of this function is undefined.  (We could remove this restriction by
 * testing for unaligned values and punting to memmove(), but that's
 * not currently useful.)
 *
 * TODO: add loop for 64-bit alignment
 * TODO: use __builtin_prefetch
 * TODO: write ARM/MIPS/x86 optimized versions
 */
void MemmoveWords(void* dst, const void* src, size_t n) {
  DCHECK_EQ((((uintptr_t) dst | (uintptr_t) src | n) & 0x01), 0U);

  char* d = reinterpret_cast<char*>(dst);
  const char* s = reinterpret_cast<const char*>(src);
  size_t copyCount;

  // If the source and destination pointers are the same, this is
  // an expensive no-op.  Testing for an empty move now allows us
  // to skip a check later.
  if (n == 0 || d == s) {
    return;
  }

  // Determine if the source and destination buffers will overlap if
  // we copy data forward (i.e. *dst++ = *src++).
  //
  // It's okay if the destination buffer starts before the source and
  // there is some overlap, because the reader is always ahead of the
  // writer.
  if (LIKELY((d < s) || ((size_t)(d - s) >= n))) {
    // Copy forward.  We prefer 32-bit loads and stores even for 16-bit
    // data, so sort that out.
    if (((reinterpret_cast<uintptr_t>(d) | reinterpret_cast<uintptr_t>(s)) & 0x03) != 0) {
      // Not 32-bit aligned.  Two possibilities:
      // (1) Congruent, we can align to 32-bit by copying one 16-bit val
      // (2) Non-congruent, we can do one of:
      //   a. copy whole buffer as a series of 16-bit values
      //   b. load/store 32 bits, using shifts to ensure alignment
      //   c. just copy the as 32-bit values and assume the CPU
      //      will do a reasonable job
      //
      // We're currently using (a), which is suboptimal.
      if (((reinterpret_cast<uintptr_t>(d) ^ reinterpret_cast<uintptr_t>(s)) & 0x03) != 0) {
        copyCount = n;
      } else {
        copyCount = 2;
      }
      n -= copyCount;
      copyCount /= sizeof(uint16_t);

      while (copyCount--) {
        *reinterpret_cast<uint16_t*>(d) = *reinterpret_cast<const uint16_t*>(s);
        d += sizeof(uint16_t);
        s += sizeof(uint16_t);
      }
    }

    // Copy 32-bit aligned words.
    copyCount = n / sizeof(uint32_t);
    while (copyCount--) {
      *reinterpret_cast<uint32_t*>(d) = *reinterpret_cast<const uint32_t*>(s);
      d += sizeof(uint32_t);
      s += sizeof(uint32_t);
    }

    // Check for leftovers.  Either we finished exactly, or we have one remaining 16-bit chunk.
    if ((n & 0x02) != 0) {
      *reinterpret_cast<uint16_t*>(d) = *reinterpret_cast<const uint16_t*>(s);
    }
  } else {
    // Copy backward, starting at the end.
    d += n;
    s += n;

    if (((reinterpret_cast<uintptr_t>(d) | reinterpret_cast<uintptr_t>(s)) & 0x03) != 0) {
      // try for 32-bit alignment.
      if (((reinterpret_cast<uintptr_t>(d) ^ reinterpret_cast<uintptr_t>(s)) & 0x03) != 0) {
        copyCount = n;
      } else {
        copyCount = 2;
      }
      n -= copyCount;
      copyCount /= sizeof(uint16_t);

      while (copyCount--) {
        d -= sizeof(uint16_t);
        s -= sizeof(uint16_t);
        *reinterpret_cast<uint16_t*>(d) = *reinterpret_cast<const uint16_t*>(s);
      }
    }

    // Copy 32-bit aligned words.
    copyCount = n / sizeof(uint32_t);
    while (copyCount--) {
      d -= sizeof(uint32_t);
      s -= sizeof(uint32_t);
      *reinterpret_cast<uint32_t*>(d) = *reinterpret_cast<const uint32_t*>(s);
    }

    // Copy leftovers.
    if ((n & 0x02) != 0) {
      d -= sizeof(uint16_t);
      s -= sizeof(uint16_t);
      *reinterpret_cast<uint16_t*>(d) = *reinterpret_cast<const uint16_t*>(s);
    }
  }
}

#define move16 MemmoveWords
#define move32 MemmoveWords

namespace art {

static void ThrowArrayStoreException_NotAnArray(const char* identifier, mirror::Object* array)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::string actualType(PrettyTypeOf(array));
  Thread* self = Thread::Current();
  ThrowLocation throw_location = self->GetCurrentLocationForThrow();
  self->ThrowNewExceptionF(throw_location, "Ljava/lang/ArrayStoreException;",
                           "%s of type %s is not an array", identifier, actualType.c_str());
}

static void System_arraycopy(JNIEnv* env, jclass, jobject javaSrc, jint srcPos, jobject javaDst, jint dstPos, jint length) {
  ScopedObjectAccess soa(env);

  // Null pointer checks.
  if (UNLIKELY(javaSrc == NULL)) {
    ThrowNullPointerException(NULL, "src == null");
    return;
  }
  if (UNLIKELY(javaDst == NULL)) {
    ThrowNullPointerException(NULL, "dst == null");
    return;
  }

  // Make sure source and destination are both arrays.
  mirror::Object* srcObject = soa.Decode<mirror::Object*>(javaSrc);
  mirror::Object* dstObject = soa.Decode<mirror::Object*>(javaDst);
  if (UNLIKELY(!srcObject->IsArrayInstance())) {
    ThrowArrayStoreException_NotAnArray("source", srcObject);
    return;
  }
  if (UNLIKELY(!dstObject->IsArrayInstance())) {
    ThrowArrayStoreException_NotAnArray("destination", dstObject);
    return;
  }
  mirror::Array* srcArray = srcObject->AsArray();
  mirror::Array* dstArray = dstObject->AsArray();
  mirror::Class* srcComponentType = srcArray->GetClass()->GetComponentType();
  mirror::Class* dstComponentType = dstArray->GetClass()->GetComponentType();

  // Bounds checking.
  if (UNLIKELY(srcPos < 0 || dstPos < 0 || length < 0 || srcPos > srcArray->GetLength() - length || dstPos > dstArray->GetLength() - length)) {
    ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
    soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/ArrayIndexOutOfBoundsException;",
                                   "src.length=%d srcPos=%d dst.length=%d dstPos=%d length=%d",
                                   srcArray->GetLength(), srcPos, dstArray->GetLength(), dstPos, length);
    return;
  }

  // Handle primitive arrays.
  if (srcComponentType->IsPrimitive() || dstComponentType->IsPrimitive()) {
    // If one of the arrays holds a primitive type the other array must hold the exact same type.
    if (UNLIKELY(srcComponentType != dstComponentType)) {
      std::string srcType(PrettyTypeOf(srcArray));
      std::string dstType(PrettyTypeOf(dstArray));
      ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
      soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/ArrayStoreException;",
                                     "Incompatible types: src=%s, dst=%s",
                                     srcType.c_str(), dstType.c_str());
      return;
    }

    size_t width = srcArray->GetClass()->GetComponentSize();
    uint8_t* dstBytes = reinterpret_cast<uint8_t*>(dstArray->GetRawData(width));
    const uint8_t* srcBytes = reinterpret_cast<const uint8_t*>(srcArray->GetRawData(width));

    switch (width) {
    case 1:
      memmove(dstBytes + dstPos, srcBytes + srcPos, length);
      break;
    case 2:
      move16(dstBytes + dstPos * 2, srcBytes + srcPos * 2, length * 2);
      break;
    case 4:
      move32(dstBytes + dstPos * 4, srcBytes + srcPos * 4, length * 4);
      break;
    case 8:
      // We don't need to guarantee atomicity of the entire 64-bit word.
      move32(dstBytes + dstPos * 8, srcBytes + srcPos * 8, length * 8);
      break;
    default:
      LOG(FATAL) << "Unknown primitive array type: " << PrettyTypeOf(srcArray);
    }

    return;
  }

  // Neither class is primitive. Are the types trivially compatible?
  const size_t width = sizeof(mirror::Object*);
  uint8_t* dstBytes = reinterpret_cast<uint8_t*>(dstArray->GetRawData(width));
  const uint8_t* srcBytes = reinterpret_cast<const uint8_t*>(srcArray->GetRawData(width));
  if (dstArray == srcArray || dstComponentType->IsAssignableFrom(srcComponentType)) {
    // Yes. Bulk copy.
    COMPILE_ASSERT(sizeof(width) == sizeof(uint32_t), move32_assumes_Object_references_are_32_bit);
    move32(dstBytes + dstPos * width, srcBytes + srcPos * width, length * width);
    Runtime::Current()->GetHeap()->WriteBarrierArray(dstArray, dstPos, length);
    return;
  }

  // The arrays are not trivially compatible. However, we may still be able to copy some or all of
  // the elements if the source objects are compatible (for example, copying an Object[] to
  // String[], the Objects being copied might actually be Strings).
  // We can't do a bulk move because that would introduce a check-use race condition, so we copy
  // elements one by one.

  // We already dealt with overlapping copies, so we don't need to cope with that case below.
  CHECK_NE(dstArray, srcArray);

  mirror::Object* const * srcObjects =
      reinterpret_cast<mirror::Object* const *>(srcBytes + srcPos * width);
  mirror::Object** dstObjects = reinterpret_cast<mirror::Object**>(dstBytes + dstPos * width);
  mirror::Class* dstClass = dstArray->GetClass()->GetComponentType();

  // We want to avoid redundant IsAssignableFrom checks where possible, so we cache a class that
  // we know is assignable to the destination array's component type.
  mirror::Class* lastAssignableElementClass = dstClass;

  mirror::Object* o = NULL;
  int i = 0;
  for (; i < length; ++i) {
    o = srcObjects[i];
    if (o != NULL) {
      mirror::Class* oClass = o->GetClass();
      if (lastAssignableElementClass == oClass) {
        dstObjects[i] = o;
      } else if (dstClass->IsAssignableFrom(oClass)) {
        lastAssignableElementClass = oClass;
        dstObjects[i] = o;
      } else {
        // Can't put this element into the array.
        break;
      }
    } else {
      dstObjects[i] = NULL;
    }
  }

  Runtime::Current()->GetHeap()->WriteBarrierArray(dstArray, dstPos, length);
  if (UNLIKELY(i != length)) {
    std::string actualSrcType(PrettyTypeOf(o));
    std::string dstType(PrettyTypeOf(dstArray));
    ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
    soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/ArrayStoreException;",
                                   "source[%d] of type %s cannot be stored in destination array of type %s",
                                   srcPos + i, actualSrcType.c_str(), dstType.c_str());
    return;
  }
}

static jint System_identityHashCode(JNIEnv* env, jclass, jobject javaObject) {
  ScopedObjectAccess soa(env);
  mirror::Object* o = soa.Decode<mirror::Object*>(javaObject);
  return static_cast<jint>(o->IdentityHashCode());
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(System, arraycopy, "(Ljava/lang/Object;ILjava/lang/Object;II)V"),
  NATIVE_METHOD(System, identityHashCode, "(Ljava/lang/Object;)I"),
};

void register_java_lang_System(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/System");
}

}  // namespace art
