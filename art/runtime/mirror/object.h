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

#ifndef ART_RUNTIME_MIRROR_OBJECT_H_
#define ART_RUNTIME_MIRROR_OBJECT_H_

#include "base/casts.h"
#include "base/logging.h"
#include "base/macros.h"
#include "cutils/atomic-inline.h"
#include "offsets.h"

namespace art {

class ImageWriter;
struct ObjectOffsets;
class Thread;

namespace mirror {

class ArtField;
class ArtMethod;
class Array;
class Class;
template<class T> class ObjectArray;
template<class T> class PrimitiveArray;
typedef PrimitiveArray<uint8_t> BooleanArray;
typedef PrimitiveArray<int8_t> ByteArray;
typedef PrimitiveArray<uint16_t> CharArray;
typedef PrimitiveArray<double> DoubleArray;
typedef PrimitiveArray<float> FloatArray;
typedef PrimitiveArray<int32_t> IntArray;
typedef PrimitiveArray<int64_t> LongArray;
typedef PrimitiveArray<int16_t> ShortArray;
class String;
class Throwable;

// Classes shared with the managed side of the world need to be packed so that they don't have
// extra platform specific padding.
#define MANAGED PACKED(4)

// Fields within mirror objects aren't accessed directly so that the appropriate amount of
// handshaking is done with GC (for example, read and write barriers). This macro is used to
// compute an offset for the Set/Get methods defined in Object that can safely access fields.
#define OFFSET_OF_OBJECT_MEMBER(type, field) \
    MemberOffset(OFFSETOF_MEMBER(type, field))

const bool kCheckFieldAssignments = false;

// C++ mirror of java.lang.Object
class MANAGED Object {
 public:
  static MemberOffset ClassOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Object, klass_);
  }

  Class* GetClass() const;

  void SetClass(Class* new_klass);

  // The verifier treats all interfaces as java.lang.Object and relies on runtime checks in
  // invoke-interface to detect incompatible interface types.
  bool VerifierInstanceOf(const Class* klass) const
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool InstanceOf(const Class* klass) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t SizeOf() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Object* Clone(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t IdentityHashCode() const {
#ifdef MOVING_GARBAGE_COLLECTOR
    // TODO: we'll need to use the Object's internal concept of identity
    UNIMPLEMENTED(FATAL);
#endif
    return reinterpret_cast<int32_t>(this);
  }

  static MemberOffset MonitorOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Object, monitor_);
  }

  volatile int32_t* GetRawLockWordAddress() {
    byte* raw_addr = reinterpret_cast<byte*>(this) +
        OFFSET_OF_OBJECT_MEMBER(Object, monitor_).Int32Value();
    int32_t* word_addr = reinterpret_cast<int32_t*>(raw_addr);
    return const_cast<volatile int32_t*>(word_addr);
  }

  uint32_t GetThinLockId();

  void MonitorEnter(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCK_FUNCTION(monitor_lock_);

  bool MonitorExit(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      UNLOCK_FUNCTION(monitor_lock_);

  void Notify(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void NotifyAll(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Wait(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Wait(Thread* self, int64_t timeout, int32_t nanos) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsClass() const;

  Class* AsClass();

  const Class* AsClass() const;

  bool IsObjectArray() const;

  template<class T>
  ObjectArray<T>* AsObjectArray();

  template<class T>
  const ObjectArray<T>* AsObjectArray() const;

  bool IsArrayInstance() const;

  Array* AsArray();

  const Array* AsArray() const;

  BooleanArray* AsBooleanArray();
  ByteArray* AsByteArray();
  CharArray* AsCharArray();
  ShortArray* AsShortArray();
  IntArray* AsIntArray();
  LongArray* AsLongArray();

  String* AsString();

  Throwable* AsThrowable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsArtMethod() const;

  ArtMethod* AsArtMethod();

  const ArtMethod* AsArtMethod() const;

  bool IsArtField() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* AsArtField() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const ArtField* AsArtField() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsReferenceInstance() const;

  bool IsWeakReferenceInstance() const;

  bool IsSoftReferenceInstance() const;

  bool IsFinalizerReferenceInstance() const;

  bool IsPhantomReferenceInstance() const;

  // Accessors for Java type fields
  template<class T>
  T GetFieldObject(MemberOffset field_offset, bool is_volatile) const {
    T result = reinterpret_cast<T>(GetField32(field_offset, is_volatile));
    VerifyObject(result);
    return result;
  }

  void SetFieldObject(MemberOffset field_offset, const Object* new_value, bool is_volatile,
                      bool this_is_valid = true) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    VerifyObject(new_value);
    SetField32(field_offset, reinterpret_cast<uint32_t>(new_value), is_volatile, this_is_valid);
    if (new_value != NULL) {
      CheckFieldAssignment(field_offset, new_value);
      WriteBarrierField(this, field_offset, new_value);
    }
  }

  uint32_t GetField32(MemberOffset field_offset, bool is_volatile) const {
    VerifyObject(this);
    const byte* raw_addr = reinterpret_cast<const byte*>(this) + field_offset.Int32Value();
    const int32_t* word_addr = reinterpret_cast<const int32_t*>(raw_addr);
    if (UNLIKELY(is_volatile)) {
      return android_atomic_acquire_load(word_addr);
    } else {
      return *word_addr;
    }
  }

  void SetField32(MemberOffset field_offset, uint32_t new_value, bool is_volatile,
                  bool this_is_valid = true) {
    if (this_is_valid) {
      VerifyObject(this);
    }
    byte* raw_addr = reinterpret_cast<byte*>(this) + field_offset.Int32Value();
    uint32_t* word_addr = reinterpret_cast<uint32_t*>(raw_addr);
    if (UNLIKELY(is_volatile)) {
      /*
       * TODO: add an android_atomic_synchronization_store() function and
       * use it in the 32-bit volatile set handlers.  On some platforms we
       * can use a fast atomic instruction and avoid the barriers.
       */
      ANDROID_MEMBAR_STORE();
      *word_addr = new_value;
      ANDROID_MEMBAR_FULL();
    } else {
      *word_addr = new_value;
    }
  }

  uint64_t GetField64(MemberOffset field_offset, bool is_volatile) const;

  void SetField64(MemberOffset field_offset, uint64_t new_value, bool is_volatile);

 protected:
  // Accessors for non-Java type fields
  template<class T>
  T GetFieldPtr(MemberOffset field_offset, bool is_volatile) const {
    return reinterpret_cast<T>(GetField32(field_offset, is_volatile));
  }

  template<typename T>
  void SetFieldPtr(MemberOffset field_offset, T new_value, bool is_volatile, bool this_is_valid = true) {
    SetField32(field_offset, reinterpret_cast<uint32_t>(new_value), is_volatile, this_is_valid);
  }

 private:
  static void VerifyObject(const Object* obj) ALWAYS_INLINE;

  // Verify the type correctness of stores to fields.
  void CheckFieldAssignmentImpl(MemberOffset field_offset, const Object* new_value)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void CheckFieldAssignment(MemberOffset field_offset, const Object* new_value)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (kCheckFieldAssignments) {
      CheckFieldAssignmentImpl(field_offset, new_value);
    }
  }

  // Write barrier called post update to a reference bearing field.
  static void WriteBarrierField(const Object* dst, MemberOffset offset, const Object* new_value);

  Class* klass_;

  uint32_t monitor_;

  friend class art::ImageWriter;
  friend struct art::ObjectOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Object);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_H_
