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

#include <stdio.h>

#include "UniquePtr.h"
#include "class_linker.h"
#include "dex_file-inl.h"
#include "gc_map.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "scoped_thread_state_change.h"
#include "thread.h"
#include "jni.h"
#include "verifier/method_verifier.h"

namespace art {

#define IS_IN_REF_BITMAP(mh, ref_bitmap, reg) \
    (((reg) < mh.GetCodeItem()->registers_size_) && \
     ((*((ref_bitmap) + (reg)/8) >> ((reg) % 8) ) & 0x01))

#define CHECK_REGS_CONTAIN_REFS(...)     \
  do {                                   \
    int t[] = {__VA_ARGS__};             \
    int t_size = sizeof(t) / sizeof(*t);      \
    for (int i = 0; i < t_size; ++i)          \
      CHECK(IS_IN_REF_BITMAP(mh, ref_bitmap, t[i])) \
          << "Error: Reg @ " << i << "-th argument is not in GC map"; \
  } while (false)

struct ReferenceMap2Visitor : public StackVisitor {
  explicit ReferenceMap2Visitor(Thread* thread)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, NULL) {
  }

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* m = GetMethod();
    if (!m || m->IsNative() || m->IsRuntimeMethod() || IsShadowFrame()) {
      return true;
    }
    LOG(INFO) << "At " << PrettyMethod(m, false);

    NativePcOffsetToReferenceMap map(m->GetNativeGcMap());

    if (m->IsCalleeSaveMethod()) {
      LOG(WARNING) << "no PC for " << PrettyMethod(m);
      return true;
    }

    const uint8_t* ref_bitmap = NULL;
    MethodHelper mh(m);
    std::string m_name(mh.GetName());

    // Given the method name and the number of times the method has been called,
    // we know the Dex registers with live reference values. Assert that what we
    // find is what is expected.
    if (m_name.compare("f") == 0) {
      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x03U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8);  // v8: this

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x06U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 1);  // v8: this, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x08U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 3, 1);  // v8: this, v3: y, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x0cU)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 3, 1);  // v8: this, v3: y, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x0eU)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 3, 1);  // v8: this, v3: y, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x10U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 3, 1);  // v8: this, v3: y, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x13U)));
      CHECK(ref_bitmap);
      // v2 is added because of the instruction at DexPC 0024. Object merges with 0 is Object. See:
      //   0024: move-object v3, v2
      //   0025: goto 0013
      // Detaled dex instructions for ReferenceMap.java are at the end of this function.
      // CHECK_REGS_CONTAIN_REFS(8, 3, 2, 1);  // v8: this, v3: y, v2: y, v1: x
      // We eliminate the non-live registers at a return, so only v3 is live:
      CHECK_REGS_CONTAIN_REFS(3);  // v3: y

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x18U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 2, 1, 0);  // v8: this, v2: y, v1: x, v0: ex

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x1aU)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 5, 2, 1, 0);  // v8: this, v5: x[1], v2: y, v1: x, v0: ex

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x1dU)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 5, 2, 1, 0);  // v8: this, v5: x[1], v2: y, v1: x, v0: ex

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x1fU)));
      CHECK(ref_bitmap);
      // v5 is removed from the root set because there is a "merge" operation.
      // See 0015: if-nez v2, 001f.
      CHECK_REGS_CONTAIN_REFS(8, 2, 1, 0);  // v8: this, v2: y, v1: x, v0: ex

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x21U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 2, 1, 0);  // v8: this, v2: y, v1: x, v0: ex

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x27U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 4, 2, 1);  // v8: this, v4: ex, v2: y, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x29U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 4, 2, 1);  // v8: this, v4: ex, v2: y, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x2cU)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 4, 2, 1);  // v8: this, v4: ex, v2: y, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x2fU)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 4, 3, 2, 1);  // v8: this, v4: ex, v3: y, v2: y, v1: x

      ref_bitmap = map.FindBitMap(m->NativePcOffset(m->ToNativePc(0x32U)));
      CHECK(ref_bitmap);
      CHECK_REGS_CONTAIN_REFS(8, 3, 2, 1, 0);  // v8: this, v3: y, v2: y, v1: x, v0: ex
    }

    return true;
  }
};

// Dex instructions for the function 'f' in ReferenceMap.java
// Virtual methods   -
//    #0              : (in LReferenceMap;)
//      name          : 'f'
//      type          : '()Ljava/lang/Object;'
//      access        : 0x0000 ()
//      code          -
//      registers     : 9
//      ins           : 1
//      outs          : 2
//      insns size    : 51 16-bit code units
//      |[0001e8] ReferenceMap.f:()Ljava/lang/Object;
//      |0000: const/4 v4, #int 2 // #2
//      |0001: const/4 v7, #int 0 // #0
//      |0002: const/4 v6, #int 1 // #1
//
// 0:[Unknown],1:[Unknown],2:[Unknown],3:[Unknown],4:[32-bit Constant: 2],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0003: new-array v1, v4, [Ljava/lang/Object;  // type@0007
//      |0005: const/4 v2, #int 0 // #0

// 0:[Unknown],1:[Reference: java.lang.Object[]],2:[Zero],3:[Unknown],4:[32-bit Constant: 2],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0006: new-instance v3, Ljava/lang/Object;  // type@0003

// [Unknown],1:[Reference: java.lang.Object[]],2:[Zero],3:[Uninitialized Reference: java.lang.Object],4:[32-bit Constant: 2],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0008: +invoke-object-init/range {}, Ljava/lang/Object;.<init>:()V // method@0005
//      |000b: const/4 v4, #int 2 // #2

// 0:[Unknown],1:[Reference: java.lang.Object[]],2:[Zero],3:[Reference: java.lang.Object],4:[32-bit Constant: 2],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |000c: aput-object v3, v1, v4

// 0:[Unknown],1:[Reference: java.lang.Object[]],2:[Zero],3:[Reference: java.lang.Object],4:[32-bit Constant: 2],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |000e: aput-object v3, v1, v6

// 0:[Unknown],1:[Reference: java.lang.Object[]],2:[Zero],3:[Reference: java.lang.Object],4:[32-bit Constant: 2],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0010: +invoke-virtual-quick {v8, v7}, [000c] // vtable #000c

// 0:[Conflict],1:[Conflict],2:[Conflict],3:[Reference: java.lang.Object],4:[Conflict],5:[Conflict],6:[Conflict],7:[Conflict],8:[Conflict],
//      |0013: return-object v3
//      |0014: move-exception v0

// 0:[Reference: java.lang.Exception],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[32-bit Constant: 2],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0015: if-nez v2, 001f // +000a
//      |0017: const/4 v4, #int 1 // #1

// 0:[Reference: java.lang.Exception],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[32-bit Constant: 1],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0018: new-instance v5, Ljava/lang/Object;  // type@0003

// 0:[Reference: java.lang.Exception],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[32-bit Constant: 1],5:[Uninitialized Reference: java.lang.Object],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |001a: +invoke-object-init/range {}, Ljava/lang/Object;.<init>:()V // method@0005

// 0:[Reference: java.lang.Exception],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[32-bit Constant: 1],5:[Reference: java.lang.Object],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |001d: aput-object v5, v1, v4

// 0:[Reference: java.lang.Exception],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[32-bit Constant: 2],5:[Conflict],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |001f: aput-object v2, v1, v6

// 0:[Reference: java.lang.Exception],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[32-bit Constant: 2],5:[Conflict],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0021: +invoke-virtual-quick {v8, v7}, [000c] // vtable #000c
//      |0024: move-object v3, v2

// 0:[Reference: java.lang.Exception],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Reference: java.lang.Object],4:[32-bit Constant: 2],5:[Conflict],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0025: goto 0013 // -0012
//      |0026: move-exception v4

// 0:[Conflict],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[Reference: java.lang.Throwable],5:[Conflict],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0027: aput-object v2, v1, v6

// 0:[Conflict],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[Reference: java.lang.Throwable],5:[Conflict],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0029: +invoke-virtual-quick {v8, v7}, [000c] // vtable #000c

// 0:[Conflict],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Conflict],4:[Reference: java.lang.Throwable],5:[Conflict],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |002c: throw v4
//      |002d: move-exception v4
//      |002e: move-object v2, v3

// 0:[Unknown],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Reference: java.lang.Object],4:[Reference: java.lang.Throwable],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |002f: goto 0027 // -0008
//      |0030: move-exception v0
//      |0031: move-object v2, v3

// 0:[Reference: java.lang.Exception],1:[Reference: java.lang.Object[]],2:[Reference: java.lang.Object],3:[Reference: java.lang.Object],4:[32-bit Constant: 2],5:[Unknown],6:[32-bit Constant: 1],7:[Zero],8:[Reference: ReferenceMap],
//      |0032: goto 0015 // -001d
//      catches       : 3
//        0x0006 - 0x000b
//          Ljava/lang/Exception; -> 0x0014
//          <any> -> 0x0026
//        0x000c - 0x000e
//          Ljava/lang/Exception; -> 0x0030
//          <any> -> 0x002d
//        0x0018 - 0x001f
//          <any> -> 0x0026
//      positions     :
//        0x0003 line=8
//        0x0005 line=9
//        0x0006 line=11
//        0x000b line=12
//        0x000e line=18
//        0x0010 line=19
//        0x0013 line=21
//        0x0014 line=13
//        0x0015 line=14
//        0x0017 line=15
//        0x001f line=18
//        0x0021 line=19
//        0x0025 line=20
//        0x0026 line=18
//        0x0029 line=19
//        0x002d line=18
//        0x0030 line=13
//      locals        :
//        0x0006 - 0x000b reg=2 y Ljava/lang/Object;
//        0x000b - 0x0013 reg=3 y Ljava/lang/Object;
//        0x0014 - 0x0015 reg=2 y Ljava/lang/Object;
//        0x0015 - 0x0026 reg=0 ex Ljava/lang/Exception;
//        0x002d - 0x0032 reg=3 y Ljava/lang/Object;
//        0x0005 - 0x0033 reg=1 x [Ljava/lang/Object;
//        0x0032 - 0x0033 reg=2 y Ljava/lang/Object;
//        0x0000 - 0x0033 reg=8 this LReferenceMap;

extern "C" JNIEXPORT jint JNICALL Java_ReferenceMap_refmap(JNIEnv*, jobject, jint count) {
  // Visitor
  ScopedObjectAccess soa(Thread::Current());
  ReferenceMap2Visitor mapper(soa.Self());
  mapper.WalkStack();

  return count + 1;
}

}  // namespace art
