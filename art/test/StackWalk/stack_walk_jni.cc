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
#include "gc_map.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "jni.h"
#include "scoped_thread_state_change.h"

namespace art {

#define REG(mh, reg_bitmap, reg) \
    (((reg) < mh.GetCodeItem()->registers_size_) && \
     ((*((reg_bitmap) + (reg)/8) >> ((reg) % 8) ) & 0x01))

#define CHECK_REGS(...) if (!IsShadowFrame()) { \
    int t[] = {__VA_ARGS__}; \
    int t_size = sizeof(t) / sizeof(*t); \
    for (int i = 0; i < t_size; ++i) \
      CHECK(REG(mh, reg_bitmap, t[i])) << "Error: Reg " << i << " is not in RegisterMap"; \
  }

static int gJava_StackWalk_refmap_calls = 0;

struct TestReferenceMapVisitor : public StackVisitor {
  explicit TestReferenceMapVisitor(Thread* thread)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, NULL) {
  }

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* m = GetMethod();
    CHECK(m != NULL);
    LOG(INFO) << "At " << PrettyMethod(m, false);

    if (m->IsCalleeSaveMethod() || m->IsNative()) {
      LOG(WARNING) << "no PC for " << PrettyMethod(m);
      CHECK_EQ(GetDexPc(), DexFile::kDexNoIndex);
      return true;
    }
    const uint8_t* reg_bitmap = NULL;
    if (!IsShadowFrame()) {
      NativePcOffsetToReferenceMap map(m->GetNativeGcMap());
      reg_bitmap = map.FindBitMap(GetNativePcOffset());
    }
    MethodHelper mh(m);
    StringPiece m_name(mh.GetName());

    // Given the method name and the number of times the method has been called,
    // we know the Dex registers with live reference values. Assert that what we
    // find is what is expected.
    if (m_name == "f") {
      if (gJava_StackWalk_refmap_calls == 1) {
        CHECK_EQ(1U, GetDexPc());
        CHECK_REGS(1);
      } else {
        CHECK_EQ(gJava_StackWalk_refmap_calls, 2);
        CHECK_EQ(5U, GetDexPc());
        CHECK_REGS(1);
      }
    } else if (m_name == "g") {
      if (gJava_StackWalk_refmap_calls == 1) {
        CHECK_EQ(0xcU, GetDexPc());
        CHECK_REGS(0, 2);  // Note that v1 is not in the minimal root set
      } else {
        CHECK_EQ(gJava_StackWalk_refmap_calls, 2);
        CHECK_EQ(0xcU, GetDexPc());
        CHECK_REGS(0, 2);
      }
    } else if (m_name == "shlemiel") {
      if (gJava_StackWalk_refmap_calls == 1) {
        CHECK_EQ(0x380U, GetDexPc());
        CHECK_REGS(2, 4, 5, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 21, 25);
      } else {
        CHECK_EQ(gJava_StackWalk_refmap_calls, 2);
        CHECK_EQ(0x380U, GetDexPc());
        CHECK_REGS(2, 4, 5, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 21, 25);
      }
    }
    LOG(INFO) << reinterpret_cast<const void*>(reg_bitmap);

    return true;
  }
};

extern "C" JNIEXPORT jint JNICALL Java_StackWalk_refmap(JNIEnv*, jobject, jint count) {
  ScopedObjectAccess soa(Thread::Current());
  CHECK_EQ(count, 0);
  gJava_StackWalk_refmap_calls++;

  // Visitor
  TestReferenceMapVisitor mapper(soa.Self());
  mapper.WalkStack();

  return count + 1;
}

extern "C" JNIEXPORT jint JNICALL Java_StackWalk2_refmap2(JNIEnv*, jobject, jint count) {
  ScopedObjectAccess soa(Thread::Current());
  gJava_StackWalk_refmap_calls++;

  // Visitor
  TestReferenceMapVisitor mapper(soa.Self());
  mapper.WalkStack();

  return count + 1;
}

}  // namespace art
