/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method.h"
#include "mirror/object-inl.h"
#include "verifier/dex_gc_map.h"
#include "stack.h"

namespace art {

class ShadowFrameCopyVisitor : public StackVisitor {
 public:
  explicit ShadowFrameCopyVisitor(Thread* self) : StackVisitor(self, NULL), prev_frame_(NULL),
      top_frame_(NULL) {}

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (IsShadowFrame()) {
      ShadowFrame* cur_frame = GetCurrentShadowFrame();
      size_t num_regs = cur_frame->NumberOfVRegs();
      mirror::ArtMethod* method = cur_frame->GetMethod();
      uint32_t dex_pc = cur_frame->GetDexPC();
      ShadowFrame* new_frame = ShadowFrame::Create(num_regs, NULL, method, dex_pc);

      const uint8_t* gc_map = method->GetNativeGcMap();
      uint32_t gc_map_length = static_cast<uint32_t>((gc_map[0] << 24) |
                                                     (gc_map[1] << 16) |
                                                     (gc_map[2] << 8) |
                                                     (gc_map[3] << 0));
      verifier::DexPcToReferenceMap dex_gc_map(gc_map + 4, gc_map_length);
      const uint8_t* reg_bitmap = dex_gc_map.FindBitMap(dex_pc);
      for (size_t reg = 0; reg < num_regs; ++reg) {
        if (TestBitmap(reg, reg_bitmap)) {
          new_frame->SetVRegReference(reg, cur_frame->GetVRegReference(reg));
        } else {
          new_frame->SetVReg(reg, cur_frame->GetVReg(reg));
        }
      }

      if (prev_frame_ != NULL) {
        prev_frame_->SetLink(new_frame);
      } else {
        top_frame_ = new_frame;
      }
      prev_frame_ = new_frame;
    }
    return true;
  }

  ShadowFrame* GetShadowFrameCopy() {
    return top_frame_;
  }

 private:
  static bool TestBitmap(int reg, const uint8_t* reg_vector) {
    return ((reg_vector[reg / 8] >> (reg % 8)) & 0x01) != 0;
  }

  ShadowFrame* prev_frame_;
  ShadowFrame* top_frame_;
};

extern "C" void art_portable_test_suspend_from_code(Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  CheckSuspend(self);
  if (Runtime::Current()->GetInstrumentation()->ShouldPortableCodeDeoptimize()) {
    // Save out the shadow frame to the heap
    ShadowFrameCopyVisitor visitor(self);
    visitor.WalkStack(true);
    self->SetDeoptimizationShadowFrame(visitor.GetShadowFrameCopy());
    self->SetDeoptimizationReturnValue(JValue());
    self->SetException(ThrowLocation(), reinterpret_cast<mirror::Throwable*>(-1));
  }
}

extern "C" ShadowFrame* art_portable_push_shadow_frame_from_code(Thread* thread,
                                                                 ShadowFrame* new_shadow_frame,
                                                                 mirror::ArtMethod* method,
                                                                 uint32_t num_vregs) {
  ShadowFrame* old_frame = thread->PushShadowFrame(new_shadow_frame);
  new_shadow_frame->SetMethod(method);
  new_shadow_frame->SetNumberOfVRegs(num_vregs);
  return old_frame;
}

}  // namespace art
