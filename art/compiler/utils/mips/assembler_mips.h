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

#ifndef ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_
#define ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_

#include <vector>

#include "base/macros.h"
#include "constants_mips.h"
#include "globals.h"
#include "managed_register_mips.h"
#include "utils/assembler.h"
#include "offsets.h"
#include "utils.h"

namespace art {
namespace mips {
#if 0
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

  friend class MipsAssembler;

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

#endif

enum LoadOperandType {
  kLoadSignedByte,
  kLoadUnsignedByte,
  kLoadSignedHalfword,
  kLoadUnsignedHalfword,
  kLoadWord,
  kLoadWordPair,
  kLoadSWord,
  kLoadDWord
};

enum StoreOperandType {
  kStoreByte,
  kStoreHalfword,
  kStoreWord,
  kStoreWordPair,
  kStoreSWord,
  kStoreDWord
};

class MipsAssembler : public Assembler {
 public:
  MipsAssembler() {}
  virtual ~MipsAssembler() {}

  // Emit Machine Instructions.
  void Add(Register rd, Register rs, Register rt);
  void Addu(Register rd, Register rs, Register rt);
  void Addi(Register rt, Register rs, uint16_t imm16);
  void Addiu(Register rt, Register rs, uint16_t imm16);
  void Sub(Register rd, Register rs, Register rt);
  void Subu(Register rd, Register rs, Register rt);
  void Mult(Register rs, Register rt);
  void Multu(Register rs, Register rt);
  void Div(Register rs, Register rt);
  void Divu(Register rs, Register rt);

  void And(Register rd, Register rs, Register rt);
  void Andi(Register rt, Register rs, uint16_t imm16);
  void Or(Register rd, Register rs, Register rt);
  void Ori(Register rt, Register rs, uint16_t imm16);
  void Xor(Register rd, Register rs, Register rt);
  void Xori(Register rt, Register rs, uint16_t imm16);
  void Nor(Register rd, Register rs, Register rt);

  void Sll(Register rd, Register rs, int shamt);
  void Srl(Register rd, Register rs, int shamt);
  void Sra(Register rd, Register rs, int shamt);
  void Sllv(Register rd, Register rs, Register rt);
  void Srlv(Register rd, Register rs, Register rt);
  void Srav(Register rd, Register rs, Register rt);

  void Lb(Register rt, Register rs, uint16_t imm16);
  void Lh(Register rt, Register rs, uint16_t imm16);
  void Lw(Register rt, Register rs, uint16_t imm16);
  void Lbu(Register rt, Register rs, uint16_t imm16);
  void Lhu(Register rt, Register rs, uint16_t imm16);
  void Lui(Register rt, uint16_t imm16);
  void Mfhi(Register rd);
  void Mflo(Register rd);

  void Sb(Register rt, Register rs, uint16_t imm16);
  void Sh(Register rt, Register rs, uint16_t imm16);
  void Sw(Register rt, Register rs, uint16_t imm16);

  void Slt(Register rd, Register rs, Register rt);
  void Sltu(Register rd, Register rs, Register rt);
  void Slti(Register rt, Register rs, uint16_t imm16);
  void Sltiu(Register rt, Register rs, uint16_t imm16);

  void Beq(Register rt, Register rs, uint16_t imm16);
  void Bne(Register rt, Register rs, uint16_t imm16);
  void J(uint32_t address);
  void Jal(uint32_t address);
  void Jr(Register rs);
  void Jalr(Register rs);

  void AddS(FRegister fd, FRegister fs, FRegister ft);
  void SubS(FRegister fd, FRegister fs, FRegister ft);
  void MulS(FRegister fd, FRegister fs, FRegister ft);
  void DivS(FRegister fd, FRegister fs, FRegister ft);
  void AddD(DRegister fd, DRegister fs, DRegister ft);
  void SubD(DRegister fd, DRegister fs, DRegister ft);
  void MulD(DRegister fd, DRegister fs, DRegister ft);
  void DivD(DRegister fd, DRegister fs, DRegister ft);
  void MovS(FRegister fd, FRegister fs);
  void MovD(DRegister fd, DRegister fs);

  void Mfc1(Register rt, FRegister fs);
  void Mtc1(FRegister ft, Register rs);
  void Lwc1(FRegister ft, Register rs, uint16_t imm16);
  void Ldc1(DRegister ft, Register rs, uint16_t imm16);
  void Swc1(FRegister ft, Register rs, uint16_t imm16);
  void Sdc1(DRegister ft, Register rs, uint16_t imm16);

  void Break();
  void Nop();
  void Move(Register rt, Register rs);
  void Clear(Register rt);
  void Not(Register rt, Register rs);
  void Mul(Register rd, Register rs, Register rt);
  void Div(Register rd, Register rs, Register rt);
  void Rem(Register rd, Register rs, Register rt);

  void AddConstant(Register rt, Register rs, int32_t value);
  void LoadImmediate(Register rt, int32_t value);

  void EmitLoad(ManagedRegister m_dst, Register src_register, int32_t src_offset, size_t size);
  void LoadFromOffset(LoadOperandType type, Register reg, Register base, int32_t offset);
  void LoadSFromOffset(FRegister reg, Register base, int32_t offset);
  void LoadDFromOffset(DRegister reg, Register base, int32_t offset);
  void StoreToOffset(StoreOperandType type, Register reg, Register base, int32_t offset);
  void StoreFToOffset(FRegister reg, Register base, int32_t offset);
  void StoreDToOffset(DRegister reg, Register base, int32_t offset);

#if 0
  MipsAssembler* lock();

  void mfence();

  MipsAssembler* fs();

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

  // Debugging and bringup support.
  void Stop(const char* message);
#endif

  // Emit data (e.g. encoded instruction or immediate) to the instruction stream.
  void Emit(int32_t value);
  void EmitBranch(Register rt, Register rs, Label* label, bool equal);
  void EmitJump(Label* label, bool link);
  void Bind(Label* label, bool is_jump);

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
  virtual void Store(FrameOffset offs, ManagedRegister msrc, size_t size);
  virtual void StoreRef(FrameOffset dest, ManagedRegister msrc);
  virtual void StoreRawPtr(FrameOffset dest, ManagedRegister msrc);

  virtual void StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                     ManagedRegister mscratch);

  virtual void StoreImmediateToThread(ThreadOffset dest, uint32_t imm,
                                      ManagedRegister mscratch);

  virtual void StoreStackOffsetToThread(ThreadOffset thr_offs,
                                        FrameOffset fr_offs,
                                        ManagedRegister mscratch);

  virtual void StoreStackPointerToThread(ThreadOffset thr_offs);

  virtual void StoreSpanning(FrameOffset dest, ManagedRegister msrc,
                             FrameOffset in_off, ManagedRegister mscratch);

  // Load routines
  virtual void Load(ManagedRegister mdest, FrameOffset src, size_t size);

  virtual void Load(ManagedRegister mdest, ThreadOffset src, size_t size);

  virtual void LoadRef(ManagedRegister dest, FrameOffset  src);

  virtual void LoadRef(ManagedRegister mdest, ManagedRegister base,
                       MemberOffset offs);

  virtual void LoadRawPtr(ManagedRegister mdest, ManagedRegister base,
                          Offset offs);

  virtual void LoadRawPtrFromThread(ManagedRegister mdest,
                                    ThreadOffset offs);

  // Copying routines
  virtual void Move(ManagedRegister mdest, ManagedRegister msrc, size_t size);

  virtual void CopyRawPtrFromThread(FrameOffset fr_offs, ThreadOffset thr_offs,
                                    ManagedRegister mscratch);

  virtual void CopyRawPtrToThread(ThreadOffset thr_offs, FrameOffset fr_offs,
                                  ManagedRegister mscratch);

  virtual void CopyRef(FrameOffset dest, FrameOffset src,
                       ManagedRegister mscratch);

  virtual void Copy(FrameOffset dest, FrameOffset src, ManagedRegister mscratch, size_t size);

  virtual void Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                    ManagedRegister mscratch, size_t size);

  virtual void Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                    ManagedRegister mscratch, size_t size);

  virtual void Copy(FrameOffset dest, FrameOffset src_base, Offset src_offset,
                    ManagedRegister mscratch, size_t size);

  virtual void Copy(ManagedRegister dest, Offset dest_offset,
                    ManagedRegister src, Offset src_offset,
                    ManagedRegister mscratch, size_t size);

  virtual void Copy(FrameOffset dest, Offset dest_offset, FrameOffset src, Offset src_offset,
                    ManagedRegister mscratch, size_t size);

  virtual void MemoryBarrier(ManagedRegister);

  // Sign extension
  virtual void SignExtend(ManagedRegister mreg, size_t size);

  // Zero extension
  virtual void ZeroExtend(ManagedRegister mreg, size_t size);

  // Exploit fast access in managed code to Thread::Current()
  virtual void GetCurrentThread(ManagedRegister tr);
  virtual void GetCurrentThread(FrameOffset dest_offset,
                                ManagedRegister mscratch);

  // Set up out_reg to hold a Object** into the SIRT, or to be NULL if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the SIRT entry to see if the value is
  // NULL.
  virtual void CreateSirtEntry(ManagedRegister out_reg, FrameOffset sirt_offset,
                               ManagedRegister in_reg, bool null_allowed);

  // Set up out_off to hold a Object** into the SIRT, or to be NULL if the
  // value is null and null_allowed.
  virtual void CreateSirtEntry(FrameOffset out_off, FrameOffset sirt_offset,
                               ManagedRegister mscratch, bool null_allowed);

  // src holds a SIRT entry (Object**) load this into dst
  virtual void LoadReferenceFromSirt(ManagedRegister dst,
                                     ManagedRegister src);

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  virtual void VerifyObject(ManagedRegister src, bool could_be_null);
  virtual void VerifyObject(FrameOffset src, bool could_be_null);

  // Call to address held at [base+offset]
  virtual void Call(ManagedRegister base, Offset offset,
                    ManagedRegister mscratch);
  virtual void Call(FrameOffset base, Offset offset,
                    ManagedRegister mscratch);
  virtual void Call(ThreadOffset offset, ManagedRegister mscratch);

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  virtual void ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust);

 private:
  void EmitR(int opcode, Register rs, Register rt, Register rd, int shamt, int funct);
  void EmitI(int opcode, Register rs, Register rt, uint16_t imm);
  void EmitJ(int opcode, int address);
  void EmitFR(int opcode, int fmt, FRegister ft, FRegister fs, FRegister fd, int funct);
  void EmitFI(int opcode, int fmt, FRegister rt, uint16_t imm);

  int32_t EncodeBranchOffset(int offset, int32_t inst, bool is_jump);
  int DecodeBranchOffset(int32_t inst, bool is_jump);

  DISALLOW_COPY_AND_ASSIGN(MipsAssembler);
};

// Slowpath entered when Thread::Current()->_exception is non-null
class MipsExceptionSlowPath : public SlowPath {
 public:
  explicit MipsExceptionSlowPath(MipsManagedRegister scratch, size_t stack_adjust)
      : scratch_(scratch), stack_adjust_(stack_adjust) {}
  virtual void Emit(Assembler *sp_asm);
 private:
  const MipsManagedRegister scratch_;
  const size_t stack_adjust_;
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_
