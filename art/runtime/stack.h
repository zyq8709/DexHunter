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

#ifndef ART_RUNTIME_STACK_H_
#define ART_RUNTIME_STACK_H_

#include "dex_file.h"
#include "instrumentation.h"
#include "base/macros.h"
#include "arch/context.h"

#include <stdint.h>
#include <string>

namespace art {

namespace mirror {
  class ArtMethod;
  class Object;
}  // namespace mirror

class Context;
class ShadowFrame;
class StackIndirectReferenceTable;
class ScopedObjectAccess;
class Thread;

// The kind of vreg being accessed in calls to Set/GetVReg.
enum VRegKind {
  kReferenceVReg,
  kIntVReg,
  kFloatVReg,
  kLongLoVReg,
  kLongHiVReg,
  kDoubleLoVReg,
  kDoubleHiVReg,
  kConstant,
  kImpreciseConstant,
  kUndefined,
};

// ShadowFrame has 3 possible layouts:
//  - portable - a unified array of VRegs and references. Precise references need GC maps.
//  - interpreter - separate VRegs and reference arrays. References are in the reference array.
//  - JNI - just VRegs, but where every VReg holds a reference.
class ShadowFrame {
 public:
  // Compute size of ShadowFrame in bytes.
  static size_t ComputeSize(uint32_t num_vregs) {
    return sizeof(ShadowFrame) + (sizeof(uint32_t) * num_vregs) +
           (sizeof(mirror::Object*) * num_vregs);
  }

  // Create ShadowFrame in heap for deoptimization.
  static ShadowFrame* Create(uint32_t num_vregs, ShadowFrame* link,
                             mirror::ArtMethod* method, uint32_t dex_pc) {
    uint8_t* memory = new uint8_t[ComputeSize(num_vregs)];
    ShadowFrame* sf = new (memory) ShadowFrame(num_vregs, link, method, dex_pc, true);
    return sf;
  }

  // Create ShadowFrame for interpreter using provided memory.
  static ShadowFrame* Create(uint32_t num_vregs, ShadowFrame* link,
                             mirror::ArtMethod* method, uint32_t dex_pc, void* memory) {
    ShadowFrame* sf = new (memory) ShadowFrame(num_vregs, link, method, dex_pc, true);
    return sf;
  }
  ~ShadowFrame() {}

  bool HasReferenceArray() const {
#if defined(ART_USE_PORTABLE_COMPILER)
    return (number_of_vregs_ & kHasReferenceArray) != 0;
#else
    return true;
#endif
  }

  uint32_t NumberOfVRegs() const {
#if defined(ART_USE_PORTABLE_COMPILER)
    return number_of_vregs_ & ~kHasReferenceArray;
#else
    return number_of_vregs_;
#endif
  }

  void SetNumberOfVRegs(uint32_t number_of_vregs) {
#if defined(ART_USE_PORTABLE_COMPILER)
    number_of_vregs_ = number_of_vregs | (number_of_vregs_ & kHasReferenceArray);
#else
    UNUSED(number_of_vregs);
    UNIMPLEMENTED(FATAL) << "Should only be called when portable is enabled";
#endif
  }

  uint32_t GetDexPC() const {
    return dex_pc_;
  }

  void SetDexPC(uint32_t dex_pc) {
    dex_pc_ = dex_pc;
  }

  ShadowFrame* GetLink() const {
    return link_;
  }

  void SetLink(ShadowFrame* frame) {
    DCHECK_NE(this, frame);
    link_ = frame;
  }

  int32_t GetVReg(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const int32_t*>(vreg);
  }

  float GetVRegFloat(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    // NOTE: Strict-aliasing?
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const float*>(vreg);
  }

  int64_t GetVRegLong(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const int64_t*>(vreg);
  }

  double GetVRegDouble(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const double*>(vreg);
  }

  mirror::Object* GetVRegReference(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    if (HasReferenceArray()) {
      return References()[i];
    } else {
      const uint32_t* vreg = &vregs_[i];
      return *reinterpret_cast<mirror::Object* const*>(vreg);
    }
  }

  // Get view of vregs as range of consecutive arguments starting at i.
  uint32_t* GetVRegArgs(size_t i) {
    return &vregs_[i];
  }

  void SetVReg(size_t i, int32_t val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<int32_t*>(vreg) = val;
  }

  void SetVRegFloat(size_t i, float val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<float*>(vreg) = val;
  }

  void SetVRegLong(size_t i, int64_t val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<int64_t*>(vreg) = val;
  }

  void SetVRegDouble(size_t i, double val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<double*>(vreg) = val;
  }

  void SetVRegReference(size_t i, mirror::Object* val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<mirror::Object**>(vreg) = val;
    if (HasReferenceArray()) {
      References()[i] = val;
    }
  }

  mirror::ArtMethod* GetMethod() const {
    DCHECK_NE(method_, static_cast<void*>(NULL));
    return method_;
  }

  mirror::Object* GetThisObject() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Object* GetThisObject(uint16_t num_ins) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ThrowLocation GetCurrentLocationForThrow() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetMethod(mirror::ArtMethod* method) {
#if defined(ART_USE_PORTABLE_COMPILER)
    DCHECK_NE(method, static_cast<void*>(NULL));
    method_ = method;
#else
    UNUSED(method);
    UNIMPLEMENTED(FATAL) << "Should only be called when portable is enabled";
#endif
  }

  bool Contains(mirror::Object** shadow_frame_entry_obj) const {
    if (HasReferenceArray()) {
      return ((&References()[0] <= shadow_frame_entry_obj) &&
              (shadow_frame_entry_obj <= (&References()[NumberOfVRegs() - 1])));
    } else {
      uint32_t* shadow_frame_entry = reinterpret_cast<uint32_t*>(shadow_frame_entry_obj);
      return ((&vregs_[0] <= shadow_frame_entry) &&
              (shadow_frame_entry <= (&vregs_[NumberOfVRegs() - 1])));
    }
  }

  static size_t LinkOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, link_);
  }

  static size_t MethodOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, method_);
  }

  static size_t DexPCOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, dex_pc_);
  }

  static size_t NumberOfVRegsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, number_of_vregs_);
  }

  static size_t VRegsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, vregs_);
  }

 private:
  ShadowFrame(uint32_t num_vregs, ShadowFrame* link, mirror::ArtMethod* method,
              uint32_t dex_pc, bool has_reference_array)
      : number_of_vregs_(num_vregs), link_(link), method_(method), dex_pc_(dex_pc) {
    if (has_reference_array) {
#if defined(ART_USE_PORTABLE_COMPILER)
      CHECK_LT(num_vregs, static_cast<uint32_t>(kHasReferenceArray));
      number_of_vregs_ |= kHasReferenceArray;
#endif
      memset(vregs_, 0, num_vregs * (sizeof(uint32_t) + sizeof(mirror::Object*)));
    } else {
      memset(vregs_, 0, num_vregs * sizeof(uint32_t));
    }
  }

  mirror::Object* const* References() const {
    DCHECK(HasReferenceArray());
    const uint32_t* vreg_end = &vregs_[NumberOfVRegs()];
    return reinterpret_cast<mirror::Object* const*>(vreg_end);
  }

  mirror::Object** References() {
    return const_cast<mirror::Object**>(const_cast<const ShadowFrame*>(this)->References());
  }

#if defined(ART_USE_PORTABLE_COMPILER)
  enum ShadowFrameFlag {
    kHasReferenceArray = 1ul << 31
  };
  // TODO: make const in the portable case.
  uint32_t number_of_vregs_;
#else
  const uint32_t number_of_vregs_;
#endif
  // Link to previous shadow frame or NULL.
  ShadowFrame* link_;
#if defined(ART_USE_PORTABLE_COMPILER)
  // TODO: make const in the portable case.
  mirror::ArtMethod* method_;
#else
  mirror::ArtMethod* const method_;
#endif
  uint32_t dex_pc_;
  uint32_t vregs_[0];

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShadowFrame);
};

// The managed stack is used to record fragments of managed code stacks. Managed code stacks
// may either be shadow frames or lists of frames using fixed frame sizes. Transition records are
// necessary for transitions between code using different frame layouts and transitions into native
// code.
class PACKED(4) ManagedStack {
 public:
  ManagedStack()
      : link_(NULL), top_shadow_frame_(NULL), top_quick_frame_(NULL), top_quick_frame_pc_(0) {}

  void PushManagedStackFragment(ManagedStack* fragment) {
    // Copy this top fragment into given fragment.
    memcpy(fragment, this, sizeof(ManagedStack));
    // Clear this fragment, which has become the top.
    memset(this, 0, sizeof(ManagedStack));
    // Link our top fragment onto the given fragment.
    link_ = fragment;
  }

  void PopManagedStackFragment(const ManagedStack& fragment) {
    DCHECK(&fragment == link_);
    // Copy this given fragment back to the top.
    memcpy(this, &fragment, sizeof(ManagedStack));
  }

  ManagedStack* GetLink() const {
    return link_;
  }

  mirror::ArtMethod** GetTopQuickFrame() const {
    return top_quick_frame_;
  }

  void SetTopQuickFrame(mirror::ArtMethod** top) {
    DCHECK(top_shadow_frame_ == NULL);
    top_quick_frame_ = top;
  }

  uintptr_t GetTopQuickFramePc() const {
    return top_quick_frame_pc_;
  }

  void SetTopQuickFramePc(uintptr_t pc) {
    DCHECK(top_shadow_frame_ == NULL);
    top_quick_frame_pc_ = pc;
  }

  static size_t TopQuickFrameOffset() {
    return OFFSETOF_MEMBER(ManagedStack, top_quick_frame_);
  }

  static size_t TopQuickFramePcOffset() {
    return OFFSETOF_MEMBER(ManagedStack, top_quick_frame_pc_);
  }

  ShadowFrame* PushShadowFrame(ShadowFrame* new_top_frame) {
    DCHECK(top_quick_frame_ == NULL);
    ShadowFrame* old_frame = top_shadow_frame_;
    top_shadow_frame_ = new_top_frame;
    new_top_frame->SetLink(old_frame);
    return old_frame;
  }

  ShadowFrame* PopShadowFrame() {
    DCHECK(top_quick_frame_ == NULL);
    CHECK(top_shadow_frame_ != NULL);
    ShadowFrame* frame = top_shadow_frame_;
    top_shadow_frame_ = frame->GetLink();
    return frame;
  }

  ShadowFrame* GetTopShadowFrame() const {
    return top_shadow_frame_;
  }

  void SetTopShadowFrame(ShadowFrame* top) {
    DCHECK(top_quick_frame_ == NULL);
    top_shadow_frame_ = top;
  }

  static size_t TopShadowFrameOffset() {
    return OFFSETOF_MEMBER(ManagedStack, top_shadow_frame_);
  }

  size_t NumJniShadowFrameReferences() const;

  bool ShadowFramesContain(mirror::Object** shadow_frame_entry) const;

 private:
  ManagedStack* link_;
  ShadowFrame* top_shadow_frame_;
  mirror::ArtMethod** top_quick_frame_;
  uintptr_t top_quick_frame_pc_;
};

class StackVisitor {
 protected:
  StackVisitor(Thread* thread, Context* context) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 public:
  virtual ~StackVisitor() {}

  // Return 'true' if we should continue to visit more frames, 'false' to stop.
  virtual bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  void WalkStack(bool include_transitions = false)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtMethod* GetMethod() const {
    if (cur_shadow_frame_ != NULL) {
      return cur_shadow_frame_->GetMethod();
    } else if (cur_quick_frame_ != NULL) {
      return *cur_quick_frame_;
    } else {
      return NULL;
    }
  }

  bool IsShadowFrame() const {
    return cur_shadow_frame_ != NULL;
  }

  uint32_t GetDexPc() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Object* GetThisObject() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t GetNativePcOffset() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uintptr_t* CalleeSaveAddress(int num, size_t frame_size) const {
    // Callee saves are held at the top of the frame
    DCHECK(GetMethod() != NULL);
    byte* save_addr =
        reinterpret_cast<byte*>(cur_quick_frame_) + frame_size - ((num + 1) * kPointerSize);
#if defined(__i386__)
    save_addr -= kPointerSize;  // account for return address
#endif
    return reinterpret_cast<uintptr_t*>(save_addr);
  }

  // Returns the height of the stack in the managed stack frames, including transitions.
  size_t GetFrameHeight() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetNumFrames() - cur_depth_ - 1;
  }

  // Returns a frame ID for JDWP use, starting from 1.
  size_t GetFrameId() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFrameHeight() + 1;
  }

  size_t GetNumFrames() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (num_frames_ == 0) {
      num_frames_ = ComputeNumFrames(thread_);
    }
    return num_frames_;
  }

  uint32_t GetVReg(mirror::ArtMethod* m, uint16_t vreg, VRegKind kind) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetVReg(mirror::ArtMethod* m, uint16_t vreg, uint32_t new_value, VRegKind kind)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uintptr_t GetGPR(uint32_t reg) const;
  void SetGPR(uint32_t reg, uintptr_t value);

  uint32_t GetVReg(mirror::ArtMethod** cur_quick_frame, const DexFile::CodeItem* code_item,
                   uint32_t core_spills, uint32_t fp_spills, size_t frame_size,
                   uint16_t vreg) const {
    int offset = GetVRegOffset(code_item, core_spills, fp_spills, frame_size, vreg);
    DCHECK_EQ(cur_quick_frame, GetCurrentQuickFrame());
    byte* vreg_addr = reinterpret_cast<byte*>(cur_quick_frame) + offset;
    return *reinterpret_cast<uint32_t*>(vreg_addr);
  }

  uintptr_t GetReturnPc() const;

  void SetReturnPc(uintptr_t new_ret_pc);

  /*
   * Return sp-relative offset for a Dalvik virtual register, compiler
   * spill or Method* in bytes using Method*.
   * Note that (reg >= 0) refers to a Dalvik register, (reg == -2)
   * denotes Method* and (reg <= -3) denotes a compiler temp.
   *
   *     +------------------------+
   *     | IN[ins-1]              |  {Note: resides in caller's frame}
   *     |       .                |
   *     | IN[0]                  |
   *     | caller's Method*       |
   *     +========================+  {Note: start of callee's frame}
   *     | core callee-save spill |  {variable sized}
   *     +------------------------+
   *     | fp callee-save spill   |
   *     +------------------------+
   *     | filler word            |  {For compatibility, if V[locals-1] used as wide
   *     +------------------------+
   *     | V[locals-1]            |
   *     | V[locals-2]            |
   *     |      .                 |
   *     |      .                 |  ... (reg == 2)
   *     | V[1]                   |  ... (reg == 1)
   *     | V[0]                   |  ... (reg == 0) <---- "locals_start"
   *     +------------------------+
   *     | Compiler temps         |  ... (reg == -2)
   *     |                        |  ... (reg == -3)
   *     |                        |  ... (reg == -4)
   *     +------------------------+
   *     | stack alignment padding|  {0 to (kStackAlignWords-1) of padding}
   *     +------------------------+
   *     | OUT[outs-1]            |
   *     | OUT[outs-2]            |
   *     |       .                |
   *     | OUT[0]                 |
   *     | curMethod*             |  ... (reg == -1) <<== sp, 16-byte aligned
   *     +========================+
   */
  static int GetVRegOffset(const DexFile::CodeItem* code_item,
                           uint32_t core_spills, uint32_t fp_spills,
                           size_t frame_size, int reg) {
    DCHECK_EQ(frame_size & (kStackAlignment - 1), 0U);
    int num_spills = __builtin_popcount(core_spills) + __builtin_popcount(fp_spills) + 1;  // Filler.
    int num_ins = code_item->ins_size_;
    int num_regs = code_item->registers_size_ - num_ins;
    int locals_start = frame_size - ((num_spills + num_regs) * sizeof(uint32_t));
    if (reg == -2) {
      return 0;  // Method*
    } else if (reg <= -3) {
      return locals_start - ((reg + 1) * sizeof(uint32_t));  // Compiler temp.
    } else if (reg < num_regs) {
      return locals_start + (reg * sizeof(uint32_t));        // Dalvik local reg.
    } else {
      return frame_size + ((reg - num_regs) * sizeof(uint32_t)) + sizeof(uint32_t);  // Dalvik in.
    }
  }

  uintptr_t GetCurrentQuickFramePc() const {
    return cur_quick_frame_pc_;
  }

  mirror::ArtMethod** GetCurrentQuickFrame() const {
    return cur_quick_frame_;
  }

  ShadowFrame* GetCurrentShadowFrame() const {
    return cur_shadow_frame_;
  }

  StackIndirectReferenceTable* GetCurrentSirt() const {
    mirror::ArtMethod** sp = GetCurrentQuickFrame();
    ++sp;  // Skip Method*; SIRT comes next;
    return reinterpret_cast<StackIndirectReferenceTable*>(sp);
  }

  std::string DescribeLocation() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static size_t ComputeNumFrames(Thread* thread) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void DescribeStack(Thread* thread) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  instrumentation::InstrumentationStackFrame GetInstrumentationStackFrame(uint32_t depth) const;

  void SanityCheckFrame() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Thread* const thread_;
  ShadowFrame* cur_shadow_frame_;
  mirror::ArtMethod** cur_quick_frame_;
  uintptr_t cur_quick_frame_pc_;
  // Lazily computed, number of frames in the stack.
  size_t num_frames_;
  // Depth of the frame we're currently at.
  size_t cur_depth_;

 protected:
  Context* const context_;
};

}  // namespace art

#endif  // ART_RUNTIME_STACK_H_
