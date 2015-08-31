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

#ifndef ART_COMPILER_UTILS_ASSEMBLER_H_
#define ART_COMPILER_UTILS_ASSEMBLER_H_

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "arm/constants_arm.h"
#include "mips/constants_mips.h"
#include "x86/constants_x86.h"
#include "instruction_set.h"
#include "managed_register.h"
#include "memory_region.h"
#include "offsets.h"

namespace art {

class Assembler;
class AssemblerBuffer;
class AssemblerFixup;

namespace arm {
  class ArmAssembler;
}
namespace mips {
  class MipsAssembler;
}
namespace x86 {
  class X86Assembler;
}

class Label {
 public:
  Label() : position_(0) {}

  ~Label() {
    // Assert if label is being destroyed with unresolved branches pending.
    CHECK(!IsLinked());
  }

  // Returns the position for bound and linked labels. Cannot be used
  // for unused labels.
  int Position() const {
    CHECK(!IsUnused());
    return IsBound() ? -position_ - kPointerSize : position_ - kPointerSize;
  }

  int LinkPosition() const {
    CHECK(IsLinked());
    return position_ - kWordSize;
  }

  bool IsBound() const { return position_ < 0; }
  bool IsUnused() const { return position_ == 0; }
  bool IsLinked() const { return position_ > 0; }

 private:
  int position_;

  void Reinitialize() {
    position_ = 0;
  }

  void BindTo(int position) {
    CHECK(!IsBound());
    position_ = -position - kPointerSize;
    CHECK(IsBound());
  }

  void LinkTo(int position) {
    CHECK(!IsBound());
    position_ = position + kPointerSize;
    CHECK(IsLinked());
  }

  friend class arm::ArmAssembler;
  friend class mips::MipsAssembler;
  friend class x86::X86Assembler;

  DISALLOW_COPY_AND_ASSIGN(Label);
};


// Assembler fixups are positions in generated code that require processing
// after the code has been copied to executable memory. This includes building
// relocation information.
class AssemblerFixup {
 public:
  virtual void Process(const MemoryRegion& region, int position) = 0;
  virtual ~AssemblerFixup() {}

 private:
  AssemblerFixup* previous_;
  int position_;

  AssemblerFixup* previous() const { return previous_; }
  void set_previous(AssemblerFixup* previous) { previous_ = previous; }

  int position() const { return position_; }
  void set_position(int position) { position_ = position; }

  friend class AssemblerBuffer;
};

// Parent of all queued slow paths, emitted during finalization
class SlowPath {
 public:
  SlowPath() : next_(NULL) {}
  virtual ~SlowPath() {}

  Label* Continuation() { return &continuation_; }
  Label* Entry() { return &entry_; }
  // Generate code for slow path
  virtual void Emit(Assembler *sp_asm) = 0;

 protected:
  // Entry branched to by fast path
  Label entry_;
  // Optional continuation that is branched to at the end of the slow path
  Label continuation_;
  // Next in linked list of slow paths
  SlowPath *next_;

 private:
  friend class AssemblerBuffer;
  DISALLOW_COPY_AND_ASSIGN(SlowPath);
};

class AssemblerBuffer {
 public:
  AssemblerBuffer();
  ~AssemblerBuffer();

  // Basic support for emitting, loading, and storing.
  template<typename T> void Emit(T value) {
    CHECK(HasEnsuredCapacity());
    *reinterpret_cast<T*>(cursor_) = value;
    cursor_ += sizeof(T);
  }

  template<typename T> T Load(size_t position) {
    CHECK_LE(position, Size() - static_cast<int>(sizeof(T)));
    return *reinterpret_cast<T*>(contents_ + position);
  }

  template<typename T> void Store(size_t position, T value) {
    CHECK_LE(position, Size() - static_cast<int>(sizeof(T)));
    *reinterpret_cast<T*>(contents_ + position) = value;
  }

  // Emit a fixup at the current location.
  void EmitFixup(AssemblerFixup* fixup) {
    fixup->set_previous(fixup_);
    fixup->set_position(Size());
    fixup_ = fixup;
  }

  void EnqueueSlowPath(SlowPath* slowpath) {
    if (slow_path_ == NULL) {
      slow_path_ = slowpath;
    } else {
      SlowPath* cur = slow_path_;
      for ( ; cur->next_ != NULL ; cur = cur->next_) {}
      cur->next_ = slowpath;
    }
  }

  void EmitSlowPaths(Assembler* sp_asm) {
    SlowPath* cur = slow_path_;
    SlowPath* next = NULL;
    slow_path_ = NULL;
    for ( ; cur != NULL ; cur = next) {
      cur->Emit(sp_asm);
      next = cur->next_;
      delete cur;
    }
  }

  // Get the size of the emitted code.
  size_t Size() const {
    CHECK_GE(cursor_, contents_);
    return cursor_ - contents_;
  }

  byte* contents() const { return contents_; }

  // Copy the assembled instructions into the specified memory block
  // and apply all fixups.
  void FinalizeInstructions(const MemoryRegion& region);

  // To emit an instruction to the assembler buffer, the EnsureCapacity helper
  // must be used to guarantee that the underlying data area is big enough to
  // hold the emitted instruction. Usage:
  //
  //     AssemblerBuffer buffer;
  //     AssemblerBuffer::EnsureCapacity ensured(&buffer);
  //     ... emit bytes for single instruction ...

#ifndef NDEBUG

  class EnsureCapacity {
   public:
    explicit EnsureCapacity(AssemblerBuffer* buffer) {
      if (buffer->cursor() >= buffer->limit()) {
        buffer->ExtendCapacity();
      }
      // In debug mode, we save the assembler buffer along with the gap
      // size before we start emitting to the buffer. This allows us to
      // check that any single generated instruction doesn't overflow the
      // limit implied by the minimum gap size.
      buffer_ = buffer;
      gap_ = ComputeGap();
      // Make sure that extending the capacity leaves a big enough gap
      // for any kind of instruction.
      CHECK_GE(gap_, kMinimumGap);
      // Mark the buffer as having ensured the capacity.
      CHECK(!buffer->HasEnsuredCapacity());  // Cannot nest.
      buffer->has_ensured_capacity_ = true;
    }

    ~EnsureCapacity() {
      // Unmark the buffer, so we cannot emit after this.
      buffer_->has_ensured_capacity_ = false;
      // Make sure the generated instruction doesn't take up more
      // space than the minimum gap.
      int delta = gap_ - ComputeGap();
      CHECK_LE(delta, kMinimumGap);
    }

   private:
    AssemblerBuffer* buffer_;
    int gap_;

    int ComputeGap() { return buffer_->Capacity() - buffer_->Size(); }
  };

  bool has_ensured_capacity_;
  bool HasEnsuredCapacity() const { return has_ensured_capacity_; }

#else

  class EnsureCapacity {
   public:
    explicit EnsureCapacity(AssemblerBuffer* buffer) {
      if (buffer->cursor() >= buffer->limit()) buffer->ExtendCapacity();
    }
  };

  // When building the C++ tests, assertion code is enabled. To allow
  // asserting that the user of the assembler buffer has ensured the
  // capacity needed for emitting, we add a dummy method in non-debug mode.
  bool HasEnsuredCapacity() const { return true; }

#endif

  // Returns the position in the instruction stream.
  int GetPosition() { return  cursor_ - contents_; }

 private:
  // The limit is set to kMinimumGap bytes before the end of the data area.
  // This leaves enough space for the longest possible instruction and allows
  // for a single, fast space check per instruction.
  static const int kMinimumGap = 32;

  byte* contents_;
  byte* cursor_;
  byte* limit_;
  AssemblerFixup* fixup_;
  bool fixups_processed_;

  // Head of linked list of slow paths
  SlowPath* slow_path_;

  byte* cursor() const { return cursor_; }
  byte* limit() const { return limit_; }
  size_t Capacity() const {
    CHECK_GE(limit_, contents_);
    return (limit_ - contents_) + kMinimumGap;
  }

  // Process the fixup chain starting at the given fixup. The offset is
  // non-zero for fixups in the body if the preamble is non-empty.
  void ProcessFixups(const MemoryRegion& region);

  // Compute the limit based on the data area and the capacity. See
  // description of kMinimumGap for the reasoning behind the value.
  static byte* ComputeLimit(byte* data, size_t capacity) {
    return data + capacity - kMinimumGap;
  }

  void ExtendCapacity();

  friend class AssemblerFixup;
};

class Assembler {
 public:
  static Assembler* Create(InstructionSet instruction_set);

  // Emit slow paths queued during assembly
  void EmitSlowPaths() { buffer_.EmitSlowPaths(this); }

  // Size of generated code
  size_t CodeSize() const { return buffer_.Size(); }

  // Copy instructions out of assembly buffer into the given region of memory
  void FinalizeInstructions(const MemoryRegion& region) {
    buffer_.FinalizeInstructions(region);
  }

  // Emit code that will create an activation on the stack
  virtual void BuildFrame(size_t frame_size, ManagedRegister method_reg,
                          const std::vector<ManagedRegister>& callee_save_regs,
                          const std::vector<ManagedRegister>& entry_spills) = 0;

  // Emit code that will remove an activation from the stack
  virtual void RemoveFrame(size_t frame_size,
                           const std::vector<ManagedRegister>& callee_save_regs) = 0;

  virtual void IncreaseFrameSize(size_t adjust) = 0;
  virtual void DecreaseFrameSize(size_t adjust) = 0;

  // Store routines
  virtual void Store(FrameOffset offs, ManagedRegister src, size_t size) = 0;
  virtual void StoreRef(FrameOffset dest, ManagedRegister src) = 0;
  virtual void StoreRawPtr(FrameOffset dest, ManagedRegister src) = 0;

  virtual void StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                     ManagedRegister scratch) = 0;

  virtual void StoreImmediateToThread(ThreadOffset dest, uint32_t imm,
                                      ManagedRegister scratch) = 0;

  virtual void StoreStackOffsetToThread(ThreadOffset thr_offs,
                                        FrameOffset fr_offs,
                                        ManagedRegister scratch) = 0;

  virtual void StoreStackPointerToThread(ThreadOffset thr_offs) = 0;

  virtual void StoreSpanning(FrameOffset dest, ManagedRegister src,
                             FrameOffset in_off, ManagedRegister scratch) = 0;

  // Load routines
  virtual void Load(ManagedRegister dest, FrameOffset src, size_t size) = 0;

  virtual void Load(ManagedRegister dest, ThreadOffset src, size_t size) = 0;

  virtual void LoadRef(ManagedRegister dest, FrameOffset  src) = 0;

  virtual void LoadRef(ManagedRegister dest, ManagedRegister base,
                       MemberOffset offs) = 0;

  virtual void LoadRawPtr(ManagedRegister dest, ManagedRegister base,
                          Offset offs) = 0;

  virtual void LoadRawPtrFromThread(ManagedRegister dest,
                                    ThreadOffset offs) = 0;

  // Copying routines
  virtual void Move(ManagedRegister dest, ManagedRegister src, size_t size) = 0;

  virtual void CopyRawPtrFromThread(FrameOffset fr_offs, ThreadOffset thr_offs,
                                    ManagedRegister scratch) = 0;

  virtual void CopyRawPtrToThread(ThreadOffset thr_offs, FrameOffset fr_offs,
                                  ManagedRegister scratch) = 0;

  virtual void CopyRef(FrameOffset dest, FrameOffset src,
                       ManagedRegister scratch) = 0;

  virtual void Copy(FrameOffset dest, FrameOffset src, ManagedRegister scratch, size_t size) = 0;

  virtual void Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                    ManagedRegister scratch, size_t size) = 0;

  virtual void Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                    ManagedRegister scratch, size_t size) = 0;

  virtual void Copy(FrameOffset dest, FrameOffset src_base, Offset src_offset,
                    ManagedRegister scratch, size_t size) = 0;

  virtual void Copy(ManagedRegister dest, Offset dest_offset,
                    ManagedRegister src, Offset src_offset,
                    ManagedRegister scratch, size_t size) = 0;

  virtual void Copy(FrameOffset dest, Offset dest_offset, FrameOffset src, Offset src_offset,
                    ManagedRegister scratch, size_t size) = 0;

  virtual void MemoryBarrier(ManagedRegister scratch) = 0;

  // Sign extension
  virtual void SignExtend(ManagedRegister mreg, size_t size) = 0;

  // Zero extension
  virtual void ZeroExtend(ManagedRegister mreg, size_t size) = 0;

  // Exploit fast access in managed code to Thread::Current()
  virtual void GetCurrentThread(ManagedRegister tr) = 0;
  virtual void GetCurrentThread(FrameOffset dest_offset,
                                ManagedRegister scratch) = 0;

  // Set up out_reg to hold a Object** into the SIRT, or to be NULL if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the SIRT entry to see if the value is
  // NULL.
  virtual void CreateSirtEntry(ManagedRegister out_reg, FrameOffset sirt_offset,
                               ManagedRegister in_reg, bool null_allowed) = 0;

  // Set up out_off to hold a Object** into the SIRT, or to be NULL if the
  // value is null and null_allowed.
  virtual void CreateSirtEntry(FrameOffset out_off, FrameOffset sirt_offset,
                               ManagedRegister scratch, bool null_allowed) = 0;

  // src holds a SIRT entry (Object**) load this into dst
  virtual void LoadReferenceFromSirt(ManagedRegister dst,
                                     ManagedRegister src) = 0;

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  virtual void VerifyObject(ManagedRegister src, bool could_be_null) = 0;
  virtual void VerifyObject(FrameOffset src, bool could_be_null) = 0;

  // Call to address held at [base+offset]
  virtual void Call(ManagedRegister base, Offset offset,
                    ManagedRegister scratch) = 0;
  virtual void Call(FrameOffset base, Offset offset,
                    ManagedRegister scratch) = 0;
  virtual void Call(ThreadOffset offset, ManagedRegister scratch) = 0;

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  virtual void ExceptionPoll(ManagedRegister scratch, size_t stack_adjust) = 0;

  virtual ~Assembler() {}

 protected:
  Assembler() : buffer_() {}

  AssemblerBuffer buffer_;
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_ASSEMBLER_H_
