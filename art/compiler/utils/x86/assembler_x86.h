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

#ifndef ART_COMPILER_UTILS_X86_ASSEMBLER_X86_H_
#define ART_COMPILER_UTILS_X86_ASSEMBLER_X86_H_

#include <vector>
#include "base/macros.h"
#include "constants_x86.h"
#include "globals.h"
#include "managed_register_x86.h"
#include "offsets.h"
#include "utils/assembler.h"
#include "utils.h"

namespace art {
namespace x86 {

class Immediate {
 public:
  explicit Immediate(int32_t value) : value_(value) {}

  int32_t value() const { return value_; }

  bool is_int8() const { return IsInt(8, value_); }
  bool is_uint8() const { return IsUint(8, value_); }
  bool is_uint16() const { return IsUint(16, value_); }

 private:
  const int32_t value_;

  DISALLOW_COPY_AND_ASSIGN(Immediate);
};


class Operand {
 public:
  uint8_t mod() const {
    return (encoding_at(0) >> 6) & 3;
  }

  Register rm() const {
    return static_cast<Register>(encoding_at(0) & 7);
  }

  ScaleFactor scale() const {
    return static_cast<ScaleFactor>((encoding_at(1) >> 6) & 3);
  }

  Register index() const {
    return static_cast<Register>((encoding_at(1) >> 3) & 7);
  }

  Register base() const {
    return static_cast<Register>(encoding_at(1) & 7);
  }

  int8_t disp8() const {
    CHECK_GE(length_, 2);
    return static_cast<int8_t>(encoding_[length_ - 1]);
  }

  int32_t disp32() const {
    CHECK_GE(length_, 5);
    int32_t value;
    memcpy(&value, &encoding_[length_ - 4], sizeof(value));
    return value;
  }

  bool IsRegister(Register reg) const {
    return ((encoding_[0] & 0xF8) == 0xC0)  // Addressing mode is register only.
        && ((encoding_[0] & 0x07) == reg);  // Register codes match.
  }

 protected:
  // Operand can be sub classed (e.g: Address).
  Operand() : length_(0) { }

  void SetModRM(int mod, Register rm) {
    CHECK_EQ(mod & ~3, 0);
    encoding_[0] = (mod << 6) | rm;
    length_ = 1;
  }

  void SetSIB(ScaleFactor scale, Register index, Register base) {
    CHECK_EQ(length_, 1);
    CHECK_EQ(scale & ~3, 0);
    encoding_[1] = (scale << 6) | (index << 3) | base;
    length_ = 2;
  }

  void SetDisp8(int8_t disp) {
    CHECK(length_ == 1 || length_ == 2);
    encoding_[length_++] = static_cast<uint8_t>(disp);
  }

  void SetDisp32(int32_t disp) {
    CHECK(length_ == 1 || length_ == 2);
    int disp_size = sizeof(disp);
    memmove(&encoding_[length_], &disp, disp_size);
    length_ += disp_size;
  }

 private:
  byte length_;
  byte encoding_[6];
  byte padding_;

  explicit Operand(Register reg) { SetModRM(3, reg); }

  // Get the operand encoding byte at the given index.
  uint8_t encoding_at(int index) const {
    CHECK_GE(index, 0);
    CHECK_LT(index, length_);
    return encoding_[index];
  }

  friend class X86Assembler;

  DISALLOW_COPY_AND_ASSIGN(Operand);
};


class Address : public Operand {
 public:
  Address(Register base, int32_t disp) {
    Init(base, disp);
  }

  Address(Register base, Offset disp) {
    Init(base, disp.Int32Value());
  }

  Address(Register base, FrameOffset disp) {
    CHECK_EQ(base, ESP);
    Init(ESP, disp.Int32Value());
  }

  Address(Register base, MemberOffset disp) {
    Init(base, disp.Int32Value());
  }

  void Init(Register base, int32_t disp) {
    if (disp == 0 && base != EBP) {
      SetModRM(0, base);
      if (base == ESP) SetSIB(TIMES_1, ESP, base);
    } else if (disp >= -128 && disp <= 127) {
      SetModRM(1, base);
      if (base == ESP) SetSIB(TIMES_1, ESP, base);
      SetDisp8(disp);
    } else {
      SetModRM(2, base);
      if (base == ESP) SetSIB(TIMES_1, ESP, base);
      SetDisp32(disp);
    }
  }


  Address(Register index, ScaleFactor scale, int32_t disp) {
    CHECK_NE(index, ESP);  // Illegal addressing mode.
    SetModRM(0, ESP);
    SetSIB(scale, index, EBP);
    SetDisp32(disp);
  }

  Address(Register base, Register index, ScaleFactor scale, int32_t disp) {
    CHECK_NE(index, ESP);  // Illegal addressing mode.
    if (disp == 0 && base != EBP) {
      SetModRM(0, ESP);
      SetSIB(scale, index, base);
    } else if (disp >= -128 && disp <= 127) {
      SetModRM(1, ESP);
      SetSIB(scale, index, base);
      SetDisp8(disp);
    } else {
      SetModRM(2, ESP);
      SetSIB(scale, index, base);
      SetDisp32(disp);
    }
  }

  static Address Absolute(uword addr) {
    Address result;
    result.SetModRM(0, EBP);
    result.SetDisp32(addr);
    return result;
  }

  static Address Absolute(ThreadOffset addr) {
    return Absolute(addr.Int32Value());
  }

 private:
  Address() {}

  DISALLOW_COPY_AND_ASSIGN(Address);
};


class X86Assembler : public Assembler {
 public:
  X86Assembler() {}
  virtual ~X86Assembler() {}

  /*
   * Emit Machine Instructions.
   */
  void call(Register reg);
  void call(const Address& address);
  void call(Label* label);

  void pushl(Register reg);
  void pushl(const Address& address);
  void pushl(const Immediate& imm);

  void popl(Register reg);
  void popl(const Address& address);

  void movl(Register dst, const Immediate& src);
  void movl(Register dst, Register src);

  void movl(Register dst, const Address& src);
  void movl(const Address& dst, Register src);
  void movl(const Address& dst, const Immediate& imm);
  void movl(const Address& dst, Label* lbl);

  void movzxb(Register dst, ByteRegister src);
  void movzxb(Register dst, const Address& src);
  void movsxb(Register dst, ByteRegister src);
  void movsxb(Register dst, const Address& src);
  void movb(Register dst, const Address& src);
  void movb(const Address& dst, ByteRegister src);
  void movb(const Address& dst, const Immediate& imm);

  void movzxw(Register dst, Register src);
  void movzxw(Register dst, const Address& src);
  void movsxw(Register dst, Register src);
  void movsxw(Register dst, const Address& src);
  void movw(Register dst, const Address& src);
  void movw(const Address& dst, Register src);

  void leal(Register dst, const Address& src);

  void cmovl(Condition condition, Register dst, Register src);

  void setb(Condition condition, Register dst);

  void movss(XmmRegister dst, const Address& src);
  void movss(const Address& dst, XmmRegister src);
  void movss(XmmRegister dst, XmmRegister src);

  void movd(XmmRegister dst, Register src);
  void movd(Register dst, XmmRegister src);

  void addss(XmmRegister dst, XmmRegister src);
  void addss(XmmRegister dst, const Address& src);
  void subss(XmmRegister dst, XmmRegister src);
  void subss(XmmRegister dst, const Address& src);
  void mulss(XmmRegister dst, XmmRegister src);
  void mulss(XmmRegister dst, const Address& src);
  void divss(XmmRegister dst, XmmRegister src);
  void divss(XmmRegister dst, const Address& src);

  void movsd(XmmRegister dst, const Address& src);
  void movsd(const Address& dst, XmmRegister src);
  void movsd(XmmRegister dst, XmmRegister src);

  void addsd(XmmRegister dst, XmmRegister src);
  void addsd(XmmRegister dst, const Address& src);
  void subsd(XmmRegister dst, XmmRegister src);
  void subsd(XmmRegister dst, const Address& src);
  void mulsd(XmmRegister dst, XmmRegister src);
  void mulsd(XmmRegister dst, const Address& src);
  void divsd(XmmRegister dst, XmmRegister src);
  void divsd(XmmRegister dst, const Address& src);

  void cvtsi2ss(XmmRegister dst, Register src);
  void cvtsi2sd(XmmRegister dst, Register src);

  void cvtss2si(Register dst, XmmRegister src);
  void cvtss2sd(XmmRegister dst, XmmRegister src);

  void cvtsd2si(Register dst, XmmRegister src);
  void cvtsd2ss(XmmRegister dst, XmmRegister src);

  void cvttss2si(Register dst, XmmRegister src);
  void cvttsd2si(Register dst, XmmRegister src);

  void cvtdq2pd(XmmRegister dst, XmmRegister src);

  void comiss(XmmRegister a, XmmRegister b);
  void comisd(XmmRegister a, XmmRegister b);

  void sqrtsd(XmmRegister dst, XmmRegister src);
  void sqrtss(XmmRegister dst, XmmRegister src);

  void xorpd(XmmRegister dst, const Address& src);
  void xorpd(XmmRegister dst, XmmRegister src);
  void xorps(XmmRegister dst, const Address& src);
  void xorps(XmmRegister dst, XmmRegister src);

  void andpd(XmmRegister dst, const Address& src);

  void flds(const Address& src);
  void fstps(const Address& dst);

  void fldl(const Address& src);
  void fstpl(const Address& dst);

  void fnstcw(const Address& dst);
  void fldcw(const Address& src);

  void fistpl(const Address& dst);
  void fistps(const Address& dst);
  void fildl(const Address& src);

  void fincstp();
  void ffree(const Immediate& index);

  void fsin();
  void fcos();
  void fptan();

  void xchgl(Register dst, Register src);
  void xchgl(Register reg, const Address& address);

  void cmpl(Register reg, const Immediate& imm);
  void cmpl(Register reg0, Register reg1);
  void cmpl(Register reg, const Address& address);

  void cmpl(const Address& address, Register reg);
  void cmpl(const Address& address, const Immediate& imm);

  void testl(Register reg1, Register reg2);
  void testl(Register reg, const Immediate& imm);

  void andl(Register dst, const Immediate& imm);
  void andl(Register dst, Register src);

  void orl(Register dst, const Immediate& imm);
  void orl(Register dst, Register src);

  void xorl(Register dst, Register src);

  void addl(Register dst, Register src);
  void addl(Register reg, const Immediate& imm);
  void addl(Register reg, const Address& address);

  void addl(const Address& address, Register reg);
  void addl(const Address& address, const Immediate& imm);

  void adcl(Register dst, Register src);
  void adcl(Register reg, const Immediate& imm);
  void adcl(Register dst, const Address& address);

  void subl(Register dst, Register src);
  void subl(Register reg, const Immediate& imm);
  void subl(Register reg, const Address& address);

  void cdq();

  void idivl(Register reg);

  void imull(Register dst, Register src);
  void imull(Register reg, const Immediate& imm);
  void imull(Register reg, const Address& address);

  void imull(Register reg);
  void imull(const Address& address);

  void mull(Register reg);
  void mull(const Address& address);

  void sbbl(Register dst, Register src);
  void sbbl(Register reg, const Immediate& imm);
  void sbbl(Register reg, const Address& address);

  void incl(Register reg);
  void incl(const Address& address);

  void decl(Register reg);
  void decl(const Address& address);

  void shll(Register reg, const Immediate& imm);
  void shll(Register operand, Register shifter);
  void shrl(Register reg, const Immediate& imm);
  void shrl(Register operand, Register shifter);
  void sarl(Register reg, const Immediate& imm);
  void sarl(Register operand, Register shifter);
  void shld(Register dst, Register src);

  void negl(Register reg);
  void notl(Register reg);

  void enter(const Immediate& imm);
  void leave();

  void ret();
  void ret(const Immediate& imm);

  void nop();
  void int3();
  void hlt();

  void j(Condition condition, Label* label);

  void jmp(Register reg);
  void jmp(const Address& address);
  void jmp(Label* label);

  X86Assembler* lock();
  void cmpxchgl(const Address& address, Register reg);

  void mfence();

  X86Assembler* fs();

  //
  // Macros for High-level operations.
  //

  void AddImmediate(Register reg, const Immediate& imm);

  void LoadDoubleConstant(XmmRegister dst, double value);

  void DoubleNegate(XmmRegister d);
  void FloatNegate(XmmRegister f);

  void DoubleAbs(XmmRegister reg);

  void LockCmpxchgl(const Address& address, Register reg) {
    lock()->cmpxchgl(address, reg);
  }

  //
  // Misc. functionality
  //
  int PreferredLoopAlignment() { return 16; }
  void Align(int alignment, int offset);
  void Bind(Label* label);

  // Debugging and bringup support.
  void Stop(const char* message);

  //
  // Overridden common assembler high-level functionality
  //

  // Emit code that will create an activation on the stack
  virtual void BuildFrame(size_t frame_size, ManagedRegister method_reg,
                          const std::vector<ManagedRegister>& callee_save_regs,
                          const std::vector<ManagedRegister>& entry_spills);

  // Emit code that will remove an activation from the stack
  virtual void RemoveFrame(size_t frame_size,
                           const std::vector<ManagedRegister>& callee_save_regs);

  virtual void IncreaseFrameSize(size_t adjust);
  virtual void DecreaseFrameSize(size_t adjust);

  // Store routines
  virtual void Store(FrameOffset offs, ManagedRegister src, size_t size);
  virtual void StoreRef(FrameOffset dest, ManagedRegister src);
  virtual void StoreRawPtr(FrameOffset dest, ManagedRegister src);

  virtual void StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                     ManagedRegister scratch);

  virtual void StoreImmediateToThread(ThreadOffset dest, uint32_t imm,
                                      ManagedRegister scratch);

  virtual void StoreStackOffsetToThread(ThreadOffset thr_offs,
                                        FrameOffset fr_offs,
                                        ManagedRegister scratch);

  virtual void StoreStackPointerToThread(ThreadOffset thr_offs);

  void StoreLabelToThread(ThreadOffset thr_offs, Label* lbl);

  virtual void StoreSpanning(FrameOffset dest, ManagedRegister src,
                             FrameOffset in_off, ManagedRegister scratch);

  // Load routines
  virtual void Load(ManagedRegister dest, FrameOffset src, size_t size);

  virtual void Load(ManagedRegister dest, ThreadOffset src, size_t size);

  virtual void LoadRef(ManagedRegister dest, FrameOffset  src);

  virtual void LoadRef(ManagedRegister dest, ManagedRegister base,
                       MemberOffset offs);

  virtual void LoadRawPtr(ManagedRegister dest, ManagedRegister base,
                          Offset offs);

  virtual void LoadRawPtrFromThread(ManagedRegister dest,
                                    ThreadOffset offs);

  // Copying routines
  virtual void Move(ManagedRegister dest, ManagedRegister src, size_t size);

  virtual void CopyRawPtrFromThread(FrameOffset fr_offs, ThreadOffset thr_offs,
                                    ManagedRegister scratch);

  virtual void CopyRawPtrToThread(ThreadOffset thr_offs, FrameOffset fr_offs,
                                  ManagedRegister scratch);

  virtual void CopyRef(FrameOffset dest, FrameOffset src,
                       ManagedRegister scratch);

  virtual void Copy(FrameOffset dest, FrameOffset src, ManagedRegister scratch, size_t size);

  virtual void Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                    ManagedRegister scratch, size_t size);

  virtual void Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                    ManagedRegister scratch, size_t size);

  virtual void Copy(FrameOffset dest, FrameOffset src_base, Offset src_offset,
                    ManagedRegister scratch, size_t size);

  virtual void Copy(ManagedRegister dest, Offset dest_offset,
                    ManagedRegister src, Offset src_offset,
                    ManagedRegister scratch, size_t size);

  virtual void Copy(FrameOffset dest, Offset dest_offset, FrameOffset src, Offset src_offset,
                    ManagedRegister scratch, size_t size);

  virtual void MemoryBarrier(ManagedRegister);

  // Sign extension
  virtual void SignExtend(ManagedRegister mreg, size_t size);

  // Zero extension
  virtual void ZeroExtend(ManagedRegister mreg, size_t size);

  // Exploit fast access in managed code to Thread::Current()
  virtual void GetCurrentThread(ManagedRegister tr);
  virtual void GetCurrentThread(FrameOffset dest_offset,
                                ManagedRegister scratch);

  // Set up out_reg to hold a Object** into the SIRT, or to be NULL if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the SIRT entry to see if the value is
  // NULL.
  virtual void CreateSirtEntry(ManagedRegister out_reg, FrameOffset sirt_offset,
                               ManagedRegister in_reg, bool null_allowed);

  // Set up out_off to hold a Object** into the SIRT, or to be NULL if the
  // value is null and null_allowed.
  virtual void CreateSirtEntry(FrameOffset out_off, FrameOffset sirt_offset,
                               ManagedRegister scratch, bool null_allowed);

  // src holds a SIRT entry (Object**) load this into dst
  virtual void LoadReferenceFromSirt(ManagedRegister dst,
                                     ManagedRegister src);

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  virtual void VerifyObject(ManagedRegister src, bool could_be_null);
  virtual void VerifyObject(FrameOffset src, bool could_be_null);

  // Call to address held at [base+offset]
  virtual void Call(ManagedRegister base, Offset offset,
                    ManagedRegister scratch);
  virtual void Call(FrameOffset base, Offset offset,
                    ManagedRegister scratch);
  virtual void Call(ThreadOffset offset, ManagedRegister scratch);

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  virtual void ExceptionPoll(ManagedRegister scratch, size_t stack_adjust);

 private:
  inline void EmitUint8(uint8_t value);
  inline void EmitInt32(int32_t value);
  inline void EmitRegisterOperand(int rm, int reg);
  inline void EmitXmmRegisterOperand(int rm, XmmRegister reg);
  inline void EmitFixup(AssemblerFixup* fixup);
  inline void EmitOperandSizeOverride();

  void EmitOperand(int rm, const Operand& operand);
  void EmitImmediate(const Immediate& imm);
  void EmitComplex(int rm, const Operand& operand, const Immediate& immediate);
  void EmitLabel(Label* label, int instruction_size);
  void EmitLabelLink(Label* label);
  void EmitNearLabelLink(Label* label);

  void EmitGenericShift(int rm, Register reg, const Immediate& imm);
  void EmitGenericShift(int rm, Register operand, Register shifter);

  DISALLOW_COPY_AND_ASSIGN(X86Assembler);
};

inline void X86Assembler::EmitUint8(uint8_t value) {
  buffer_.Emit<uint8_t>(value);
}

inline void X86Assembler::EmitInt32(int32_t value) {
  buffer_.Emit<int32_t>(value);
}

inline void X86Assembler::EmitRegisterOperand(int rm, int reg) {
  CHECK_GE(rm, 0);
  CHECK_LT(rm, 8);
  buffer_.Emit<uint8_t>(0xC0 + (rm << 3) + reg);
}

inline void X86Assembler::EmitXmmRegisterOperand(int rm, XmmRegister reg) {
  EmitRegisterOperand(rm, static_cast<Register>(reg));
}

inline void X86Assembler::EmitFixup(AssemblerFixup* fixup) {
  buffer_.EmitFixup(fixup);
}

inline void X86Assembler::EmitOperandSizeOverride() {
  EmitUint8(0x66);
}

// Slowpath entered when Thread::Current()->_exception is non-null
class X86ExceptionSlowPath : public SlowPath {
 public:
  explicit X86ExceptionSlowPath(size_t stack_adjust) : stack_adjust_(stack_adjust) {}
  virtual void Emit(Assembler *sp_asm);
 private:
  const size_t stack_adjust_;
};

}  // namespace x86
}  // namespace art

#endif  // ART_COMPILER_UTILS_X86_ASSEMBLER_X86_H_
