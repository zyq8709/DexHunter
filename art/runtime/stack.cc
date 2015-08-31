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

#include "stack.h"

#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "object_utils.h"
#include "thread_list.h"
#include "throw_location.h"
#include "vmap_table.h"

namespace art {

mirror::Object* ShadowFrame::GetThisObject() const {
  mirror::ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return NULL;
  } else if (m->IsNative()) {
    return GetVRegReference(0);
  } else {
    const DexFile::CodeItem* code_item = MethodHelper(m).GetCodeItem();
    CHECK(code_item != NULL) << PrettyMethod(m);
    uint16_t reg = code_item->registers_size_ - code_item->ins_size_;
    return GetVRegReference(reg);
  }
}

mirror::Object* ShadowFrame::GetThisObject(uint16_t num_ins) const {
  mirror::ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return NULL;
  } else {
    return GetVRegReference(NumberOfVRegs() - num_ins);
  }
}

ThrowLocation ShadowFrame::GetCurrentLocationForThrow() const {
  return ThrowLocation(GetThisObject(), GetMethod(), GetDexPC());
}

size_t ManagedStack::NumJniShadowFrameReferences() const {
  size_t count = 0;
  for (const ManagedStack* current_fragment = this; current_fragment != NULL;
       current_fragment = current_fragment->GetLink()) {
    for (ShadowFrame* current_frame = current_fragment->top_shadow_frame_; current_frame != NULL;
         current_frame = current_frame->GetLink()) {
      if (current_frame->GetMethod()->IsNative()) {
        // The JNI ShadowFrame only contains references. (For indirect reference.)
        count += current_frame->NumberOfVRegs();
      }
    }
  }
  return count;
}

bool ManagedStack::ShadowFramesContain(mirror::Object** shadow_frame_entry) const {
  for (const ManagedStack* current_fragment = this; current_fragment != NULL;
       current_fragment = current_fragment->GetLink()) {
    for (ShadowFrame* current_frame = current_fragment->top_shadow_frame_; current_frame != NULL;
         current_frame = current_frame->GetLink()) {
      if (current_frame->Contains(shadow_frame_entry)) {
        return true;
      }
    }
  }
  return false;
}

StackVisitor::StackVisitor(Thread* thread, Context* context)
    : thread_(thread), cur_shadow_frame_(NULL),
      cur_quick_frame_(NULL), cur_quick_frame_pc_(0), num_frames_(0), cur_depth_(0),
      context_(context) {
  DCHECK(thread == Thread::Current() || thread->IsSuspended()) << *thread;
}

uint32_t StackVisitor::GetDexPc() const {
  if (cur_shadow_frame_ != NULL) {
    return cur_shadow_frame_->GetDexPC();
  } else if (cur_quick_frame_ != NULL) {
    return GetMethod()->ToDexPc(cur_quick_frame_pc_);
  } else {
    return 0;
  }
}

mirror::Object* StackVisitor::GetThisObject() const {
  mirror::ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return NULL;
  } else if (m->IsNative()) {
    if (cur_quick_frame_ != NULL) {
      StackIndirectReferenceTable* sirt =
          reinterpret_cast<StackIndirectReferenceTable*>(
              reinterpret_cast<char*>(cur_quick_frame_) +
              m->GetSirtOffsetInBytes());
      return sirt->GetReference(0);
    } else {
      return cur_shadow_frame_->GetVRegReference(0);
    }
  } else {
    const DexFile::CodeItem* code_item = MethodHelper(m).GetCodeItem();
    if (code_item == NULL) {
      UNIMPLEMENTED(ERROR) << "Failed to determine this object of abstract or proxy method"
          << PrettyMethod(m);
      return NULL;
    } else {
      uint16_t reg = code_item->registers_size_ - code_item->ins_size_;
      return reinterpret_cast<mirror::Object*>(GetVReg(m, reg, kReferenceVReg));
    }
  }
}

size_t StackVisitor::GetNativePcOffset() const {
  DCHECK(!IsShadowFrame());
  return GetMethod()->NativePcOffset(cur_quick_frame_pc_);
}

uint32_t StackVisitor::GetVReg(mirror::ArtMethod* m, uint16_t vreg, VRegKind kind) const {
  if (cur_quick_frame_ != NULL) {
    DCHECK(context_ != NULL);  // You can't reliably read registers without a context.
    DCHECK(m == GetMethod());
    const VmapTable vmap_table(m->GetVmapTable());
    uint32_t vmap_offset;
    // TODO: IsInContext stops before spotting floating point registers.
    if (vmap_table.IsInContext(vreg, kind, &vmap_offset)) {
      bool is_float = (kind == kFloatVReg) || (kind == kDoubleLoVReg) || (kind == kDoubleHiVReg);
      uint32_t spill_mask = is_float ? m->GetFpSpillMask()
                                     : m->GetCoreSpillMask();
      return GetGPR(vmap_table.ComputeRegister(spill_mask, vmap_offset, kind));
    } else {
      const DexFile::CodeItem* code_item = MethodHelper(m).GetCodeItem();
      DCHECK(code_item != NULL) << PrettyMethod(m);  // Can't be NULL or how would we compile its instructions?
      size_t frame_size = m->GetFrameSizeInBytes();
      return GetVReg(cur_quick_frame_, code_item, m->GetCoreSpillMask(), m->GetFpSpillMask(),
                     frame_size, vreg);
    }
  } else {
    return cur_shadow_frame_->GetVReg(vreg);
  }
}

void StackVisitor::SetVReg(mirror::ArtMethod* m, uint16_t vreg, uint32_t new_value,
                           VRegKind kind) {
  if (cur_quick_frame_ != NULL) {
    DCHECK(context_ != NULL);  // You can't reliably write registers without a context.
    DCHECK(m == GetMethod());
    const VmapTable vmap_table(m->GetVmapTable());
    uint32_t vmap_offset;
    // TODO: IsInContext stops before spotting floating point registers.
    if (vmap_table.IsInContext(vreg, kind, &vmap_offset)) {
      bool is_float = (kind == kFloatVReg) || (kind == kDoubleLoVReg) || (kind == kDoubleHiVReg);
      uint32_t spill_mask = is_float ? m->GetFpSpillMask() : m->GetCoreSpillMask();
      const uint32_t reg = vmap_table.ComputeRegister(spill_mask, vmap_offset, kReferenceVReg);
      SetGPR(reg, new_value);
    } else {
      const DexFile::CodeItem* code_item = MethodHelper(m).GetCodeItem();
      DCHECK(code_item != NULL) << PrettyMethod(m);  // Can't be NULL or how would we compile its instructions?
      uint32_t core_spills = m->GetCoreSpillMask();
      uint32_t fp_spills = m->GetFpSpillMask();
      size_t frame_size = m->GetFrameSizeInBytes();
      int offset = GetVRegOffset(code_item, core_spills, fp_spills, frame_size, vreg);
      byte* vreg_addr = reinterpret_cast<byte*>(GetCurrentQuickFrame()) + offset;
      *reinterpret_cast<uint32_t*>(vreg_addr) = new_value;
    }
  } else {
    return cur_shadow_frame_->SetVReg(vreg, new_value);
  }
}

uintptr_t StackVisitor::GetGPR(uint32_t reg) const {
  DCHECK(cur_quick_frame_ != NULL) << "This is a quick frame routine";
  return context_->GetGPR(reg);
}

void StackVisitor::SetGPR(uint32_t reg, uintptr_t value) {
  DCHECK(cur_quick_frame_ != NULL) << "This is a quick frame routine";
  context_->SetGPR(reg, value);
}

uintptr_t StackVisitor::GetReturnPc() const {
  mirror::ArtMethod** sp = GetCurrentQuickFrame();
  DCHECK(sp != NULL);
  byte* pc_addr = reinterpret_cast<byte*>(sp) + GetMethod()->GetReturnPcOffsetInBytes();
  return *reinterpret_cast<uintptr_t*>(pc_addr);
}

void StackVisitor::SetReturnPc(uintptr_t new_ret_pc) {
  mirror::ArtMethod** sp = GetCurrentQuickFrame();
  CHECK(sp != NULL);
  byte* pc_addr = reinterpret_cast<byte*>(sp) + GetMethod()->GetReturnPcOffsetInBytes();
  *reinterpret_cast<uintptr_t*>(pc_addr) = new_ret_pc;
}

size_t StackVisitor::ComputeNumFrames(Thread* thread) {
  struct NumFramesVisitor : public StackVisitor {
    explicit NumFramesVisitor(Thread* thread)
        : StackVisitor(thread, NULL), frames(0) {}

    virtual bool VisitFrame() {
      frames++;
      return true;
    }

    size_t frames;
  };
  NumFramesVisitor visitor(thread);
  visitor.WalkStack(true);
  return visitor.frames;
}

void StackVisitor::DescribeStack(Thread* thread) {
  struct DescribeStackVisitor : public StackVisitor {
    explicit DescribeStackVisitor(Thread* thread)
        : StackVisitor(thread, NULL) {}

    virtual bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      LOG(INFO) << "Frame Id=" << GetFrameId() << " " << DescribeLocation();
      return true;
    }
  };
  DescribeStackVisitor visitor(thread);
  visitor.WalkStack(true);
}

std::string StackVisitor::DescribeLocation() const {
  std::string result("Visiting method '");
  mirror::ArtMethod* m = GetMethod();
  if (m == NULL) {
    return "upcall";
  }
  result += PrettyMethod(m);
  result += StringPrintf("' at dex PC 0x%04zx", GetDexPc());
  if (!IsShadowFrame()) {
    result += StringPrintf(" (native PC %p)", reinterpret_cast<void*>(GetCurrentQuickFramePc()));
  }
  return result;
}

instrumentation::InstrumentationStackFrame StackVisitor::GetInstrumentationStackFrame(uint32_t depth) const {
  return thread_->GetInstrumentationStack()->at(depth);
}

void StackVisitor::SanityCheckFrame() const {
#ifndef NDEBUG
  mirror::ArtMethod* method = GetMethod();
  CHECK(method->GetClass() == mirror::ArtMethod::GetJavaLangReflectArtMethod());
  if (cur_quick_frame_ != NULL) {
    method->AssertPcIsWithinCode(cur_quick_frame_pc_);
    // Frame sanity.
    size_t frame_size = method->GetFrameSizeInBytes();
    CHECK_NE(frame_size, 0u);
    // A rough guess at an upper size we expect to see for a frame. The 256 is
    // a dex register limit. The 16 incorporates callee save spills and
    // outgoing argument set up.
    const size_t kMaxExpectedFrameSize = 256 * sizeof(word) + 16;
    CHECK_LE(frame_size, kMaxExpectedFrameSize);
    size_t return_pc_offset = method->GetReturnPcOffsetInBytes();
    CHECK_LT(return_pc_offset, frame_size);
  }
#endif
}

void StackVisitor::WalkStack(bool include_transitions) {
  DCHECK(thread_ == Thread::Current() || thread_->IsSuspended());
  CHECK_EQ(cur_depth_, 0U);
  bool exit_stubs_installed = Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled();
  uint32_t instrumentation_stack_depth = 0;
  for (const ManagedStack* current_fragment = thread_->GetManagedStack(); current_fragment != NULL;
       current_fragment = current_fragment->GetLink()) {
    cur_shadow_frame_ = current_fragment->GetTopShadowFrame();
    cur_quick_frame_ = current_fragment->GetTopQuickFrame();
    cur_quick_frame_pc_ = current_fragment->GetTopQuickFramePc();
    if (cur_quick_frame_ != NULL) {  // Handle quick stack frames.
      // Can't be both a shadow and a quick fragment.
      DCHECK(current_fragment->GetTopShadowFrame() == NULL);
      mirror::ArtMethod* method = *cur_quick_frame_;
      while (method != NULL) {
        SanityCheckFrame();
        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }
        if (context_ != NULL) {
          context_->FillCalleeSaves(*this);
        }
        size_t frame_size = method->GetFrameSizeInBytes();
        // Compute PC for next stack frame from return PC.
        size_t return_pc_offset = method->GetReturnPcOffsetInBytes();
        byte* return_pc_addr = reinterpret_cast<byte*>(cur_quick_frame_) + return_pc_offset;
        uintptr_t return_pc = *reinterpret_cast<uintptr_t*>(return_pc_addr);
        if (UNLIKELY(exit_stubs_installed)) {
          // While profiling, the return pc is restored from the side stack, except when walking
          // the stack for an exception where the side stack will be unwound in VisitFrame.
          if (GetQuickInstrumentationExitPc() == return_pc) {
            instrumentation::InstrumentationStackFrame instrumentation_frame =
                GetInstrumentationStackFrame(instrumentation_stack_depth);
            instrumentation_stack_depth++;
            if (GetMethod() == Runtime::Current()->GetCalleeSaveMethod(Runtime::kSaveAll)) {
              // Skip runtime save all callee frames which are used to deliver exceptions.
            } else if (instrumentation_frame.interpreter_entry_) {
              mirror::ArtMethod* callee = Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs);
              CHECK_EQ(GetMethod(), callee) << "Expected: " << PrettyMethod(callee) << " Found: "
                  << PrettyMethod(GetMethod());
            } else if (instrumentation_frame.method_ != GetMethod()) {
              LOG(FATAL)  << "Expected: " << PrettyMethod(instrumentation_frame.method_)
                << " Found: " << PrettyMethod(GetMethod());
            }
            if (num_frames_ != 0) {
              // Check agreement of frame Ids only if num_frames_ is computed to avoid infinite
              // recursion.
              CHECK(instrumentation_frame.frame_id_ == GetFrameId())
                    << "Expected: " << instrumentation_frame.frame_id_
                    << " Found: " << GetFrameId();
            }
            return_pc = instrumentation_frame.return_pc_;
          }
        }
        cur_quick_frame_pc_ = return_pc;
        byte* next_frame = reinterpret_cast<byte*>(cur_quick_frame_) + frame_size;
        cur_quick_frame_ = reinterpret_cast<mirror::ArtMethod**>(next_frame);
        cur_depth_++;
        method = *cur_quick_frame_;
      }
    } else if (cur_shadow_frame_ != NULL) {
      do {
        SanityCheckFrame();
        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }
        cur_depth_++;
        cur_shadow_frame_ = cur_shadow_frame_->GetLink();
      } while (cur_shadow_frame_ != NULL);
    }
    if (include_transitions) {
      bool should_continue = VisitFrame();
      if (!should_continue) {
        return;
      }
    }
    cur_depth_++;
  }
  if (num_frames_ != 0) {
    CHECK_EQ(cur_depth_, num_frames_);
  }
}

}  // namespace art
