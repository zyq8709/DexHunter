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

#ifndef ART_COMPILER_UTILS_ARM_ASSEMBLER_ARM_H_
#define ART_COMPILER_UTILS_ARM_ASSEMBLER_ARM_H_

#include <vector>

#include "base/logging.h"
#include "constants_arm.h"
#include "utils/arm/managed_register_arm.h"
#include "utils/assembler.h"
#include "offsets.h"
#include "utils.h"

namespace art {
namespace arm {

// Encodes Addressing Mode 1 - Data-processing operands defined in Section 5.1.
class ShifterOperand {
 public:
  // Data-processing operands - Uninitialized
  ShifterOperand() {
    type_ = -1;
  }

  // Data-processing operands - Immediate
  explicit ShifterOperand(uint32_t immediate) {
    CHECK(immediate < (1 << kImmed8Bits));
    type_ = 1;
    encoding_ = immediate;
  }

  // Data-processing operands - Rotated immediate
  ShifterOperand(uint32_t rotate, uint32_t immed8) {
    CHECK((rotate < (1 << kRotateBits)) && (immed8 < (1 << kImmed8Bits)));
    type_ = 1;
    encoding_ = (rotate << kRotateShift) | (immed8 << kImmed8Shift);
  }

  // Data-processing operands - Register
  explicit ShifterOperand(Register rm) {
    type_ = 0;
    encoding_ = static_cast<uint32_t>(rm);
  }

  // Data-processing operands - Logical shift/rotate by immediate
  ShifterOperand(Register rm, Shift shift, uint32_t shift_imm) {
    CHECK(shift_imm < (1 << kShiftImmBits));
    type_ = 0;
    encoding_ = shift_imm << kShiftImmShift |
                static_cast<uint32_t>(shift) << kShiftShift |
                static_cast<uint32_t>(rm);
  }

  // Data-processing operands - Logical shift/rotate by register
  ShifterOperand(Register rm, Shift shift, Register rs) {
    type_ = 0;
    encoding_ = static_cast<uint32_t>(rs) << kShiftRegisterShift |
                static_cast<uint32_t>(shift) << kShiftShift | (1 << 4) |
                static_cast<uint32_t>(rm);
  }

  static bool CanHold(uint32_t immediate, ShifterOperand* shifter_op) {
    // Avoid the more expensive test for frequent small immediate values.
    if (immediate < (1 << kImmed8Bits)) {
      shifter_op->type_ = 1;
      shifter_op->encoding_ = (0 << kRotateShift) | (immediate << kImmed8Shift);
      return true;
    }
    // Note that immediate must be unsigned for the test to work correctly.
    for (int rot = 0; rot < 16; rot++) {
      uint32_t imm8 = (immediate << 2*rot) | (immediate >> (32 - 2*rot));
      if (imm8 < (1 << kImmed8Bits)) {
        shifter_op->type_ = 1;
        shifter_op->encoding_ = (rot << kRotateShift) | (imm8 << kImmed8Shift);
        return true;
      }
    }
    return false;
  }

 private:
  bool is_valid() const { return (type_ == 0) || (type_ == 1); }

  uint32_t type() const {
    CHECK(is_valid());
    return type_;
  }

  uint32_t encoding() const {
    CHECK(is_valid());
    return encoding_;
  }

  uint32_t type_;  // Encodes the type field (bits 27-25) in the instruction.
  uint32_t encoding_;

  friend class ArmAssembler;
#ifdef SOURCE_ASSEMBLER_SUPPORT
  friend class BinaryAssembler;
#endif
};


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


// Load/store multiple addressing mode.
enum BlockAddressMode {
  // bit encoding P U W
  DA           = (0|0|0) << 21,  // decrement after
  IA           = (0|4|0) << 21,  // increment after
  DB           = (8|0|0) << 21,  // decrement before
  IB           = (8|4|0) << 21,  // increment before
  DA_W         = (0|0|1) << 21,  // decrement after with writeback to base
  IA_W         = (0|4|1) << 21,  // increment after with writeback to base
  DB_W         = (8|0|1) << 21,  // decrement before with writeback to base
  IB_W         = (8|4|1) << 21   // increment before with writeback to base
};


class Address {
 public:
  // Memory operand addressing mode
  enum Mode {
    // bit encoding P U W
    Offset       = (8|4|0) << 21,  // offset (w/o writeback to base)
    PreIndex     = (8|4|1) << 21,  // pre-indexed addressing with writeback
    PostIndex    = (0|4|0) << 21,  // post-indexed addressing with writeback
    NegOffset    = (8|0|0) << 21,  // negative offset (w/o writeback to base)
    NegPreIndex  = (8|0|1) << 21,  // negative pre-indexed with writeback
    NegPostIndex = (0|0|0) << 21   // negative post-indexed with writeback
  };

  explicit Address(Register rn, int32_t offset = 0, Mode am = Offset) {
    CHECK(IsAbsoluteUint(12, offset));
    if (offset < 0) {
      encoding_ = (am ^ (1 << kUShift)) | -offset;  // Flip U to adjust sign.
    } else {
      encoding_ = am | offset;
    }
    encoding_ |= static_cast<uint32_t>(rn) << kRnShift;
  }

  static bool CanHoldLoadOffset(LoadOperandType type, int offset);
  static bool CanHoldStoreOffset(StoreOperandType type, int offset);

 private:
  uint32_t encoding() const { return encoding_; }

  // Encoding for addressing mode 3.
  uint32_t encoding3() const {
    const uint32_t offset_mask = (1 << 12) - 1;
    uint32_t offset = encoding_ & offset_mask;
    CHECK_LT(offset, 256u);
    return (encoding_ & ~offset_mask) | ((offset & 0xf0) << 4) | (offset & 0xf);
  }

  // Encoding for vfp load/store addressing.
  uint32_t vencoding() const {
    const uint32_t offset_mask = (1 << 12) - 1;
    uint32_t offset = encoding_ & offset_mask;
    CHECK(IsAbsoluteUint(10, offset));  // In the range -1020 to +1020.
    CHECK_ALIGNED(offset, 2);  // Multiple of 4.
    int mode = encoding_ & ((8|4|1) << 21);
    CHECK((mode == Offset) || (mode == NegOffset));
    uint32_t vencoding = (encoding_ & (0xf << kRnShift)) | (offset >> 2);
    if (mode == Offset) {
      vencoding |= 1 << 23;
    }
    return vencoding;
  }

  uint32_t encoding_;

  friend class ArmAssembler;
};


class ArmAssembler : public Assembler {
 public:
  ArmAssembler() {}
  virtual ~ArmAssembler() {}

  // Data-processing instructions.
  void and_(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void eor(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void sub(Register rd, Register rn, ShifterOperand so, Condition cond = AL);
  void subs(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void rsb(Register rd, Register rn, ShifterOperand so, Condition cond = AL);
  void rsbs(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void add(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void adds(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void adc(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void sbc(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void rsc(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void tst(Register rn, ShifterOperand so, Condition cond = AL);

  void teq(Register rn, ShifterOperand so, Condition cond = AL);

  void cmp(Register rn, ShifterOperand so, Condition cond = AL);

  void cmn(Register rn, ShifterOperand so, Condition cond = AL);

  void orr(Register rd, Register rn, ShifterOperand so, Condition cond = AL);
  void orrs(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void mov(Register rd, ShifterOperand so, Condition cond = AL);
  void movs(Register rd, ShifterOperand so, Condition cond = AL);

  void bic(Register rd, Register rn, ShifterOperand so, Condition cond = AL);

  void mvn(Register rd, ShifterOperand so, Condition cond = AL);
  void mvns(Register rd, ShifterOperand so, Condition cond = AL);

  // Miscellaneous data-processing instructions.
  void clz(Register rd, Register rm, Condition cond = AL);
  void movw(Register rd, uint16_t imm16, Condition cond = AL);
  void movt(Register rd, uint16_t imm16, Condition cond = AL);

  // Multiply instructions.
  void mul(Register rd, Register rn, Register rm, Condition cond = AL);
  void mla(Register rd, Register rn, Register rm, Register ra,
           Condition cond = AL);
  void mls(Register rd, Register rn, Register rm, Register ra,
           Condition cond = AL);
  void umull(Register rd_lo, Register rd_hi, Register rn, Register rm,
             Condition cond = AL);

  // Load/store instructions.
  void ldr(Register rd, Address ad, Condition cond = AL);
  void str(Register rd, Address ad, Condition cond = AL);

  void ldrb(Register rd, Address ad, Condition cond = AL);
  void strb(Register rd, Address ad, Condition cond = AL);

  void ldrh(Register rd, Address ad, Condition cond = AL);
  void strh(Register rd, Address ad, Condition cond = AL);

  void ldrsb(Register rd, Address ad, Condition cond = AL);
  void ldrsh(Register rd, Address ad, Condition cond = AL);

  void ldrd(Register rd, Address ad, Condition cond = AL);
  void strd(Register rd, Address ad, Condition cond = AL);

  void ldm(BlockAddressMode am, Register base,
           RegList regs, Condition cond = AL);
  void stm(BlockAddressMode am, Register base,
           RegList regs, Condition cond = AL);

  void ldrex(Register rd, Register rn, Condition cond = AL);
  void strex(Register rd, Register rt, Register rn, Condition cond = AL);

  // Miscellaneous instructions.
  void clrex();
  void nop(Condition cond = AL);

  // Note that gdb sets breakpoints using the undefined instruction 0xe7f001f0.
  void bkpt(uint16_t imm16);
  void svc(uint32_t imm24);

  // Floating point instructions (VFPv3-D16 and VFPv3-D32 profiles).
  void vmovsr(SRegister sn, Register rt, Condition cond = AL);
  void vmovrs(Register rt, SRegister sn, Condition cond = AL);
  void vmovsrr(SRegister sm, Register rt, Register rt2, Condition cond = AL);
  void vmovrrs(Register rt, Register rt2, SRegister sm, Condition cond = AL);
  void vmovdrr(DRegister dm, Register rt, Register rt2, Condition cond = AL);
  void vmovrrd(Register rt, Register rt2, DRegister dm, Condition cond = AL);
  void vmovs(SRegister sd, SRegister sm, Condition cond = AL);
  void vmovd(DRegister dd, DRegister dm, Condition cond = AL);

  // Returns false if the immediate cannot be encoded.
  bool vmovs(SRegister sd, float s_imm, Condition cond = AL);
  bool vmovd(DRegister dd, double d_imm, Condition cond = AL);

  void vldrs(SRegister sd, Address ad, Condition cond = AL);
  void vstrs(SRegister sd, Address ad, Condition cond = AL);
  void vldrd(DRegister dd, Address ad, Condition cond = AL);
  void vstrd(DRegister dd, Address ad, Condition cond = AL);

  void vadds(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL);
  void vaddd(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL);
  void vsubs(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL);
  void vsubd(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL);
  void vmuls(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL);
  void vmuld(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL);
  void vmlas(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL);
  void vmlad(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL);
  void vmlss(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL);
  void vmlsd(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL);
  void vdivs(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL);
  void vdivd(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL);

  void vabss(SRegister sd, SRegister sm, Condition cond = AL);
  void vabsd(DRegister dd, DRegister dm, Condition cond = AL);
  void vnegs(SRegister sd, SRegister sm, Condition cond = AL);
  void vnegd(DRegister dd, DRegister dm, Condition cond = AL);
  void vsqrts(SRegister sd, SRegister sm, Condition cond = AL);
  void vsqrtd(DRegister dd, DRegister dm, Condition cond = AL);

  void vcvtsd(SRegister sd, DRegister dm, Condition cond = AL);
  void vcvtds(DRegister dd, SRegister sm, Condition cond = AL);
  void vcvtis(SRegister sd, SRegister sm, Condition cond = AL);
  void vcvtid(SRegister sd, DRegister dm, Condition cond = AL);
  void vcvtsi(SRegister sd, SRegister sm, Condition cond = AL);
  void vcvtdi(DRegister dd, SRegister sm, Condition cond = AL);
  void vcvtus(SRegister sd, SRegister sm, Condition cond = AL);
  void vcvtud(SRegister sd, DRegister dm, Condition cond = AL);
  void vcvtsu(SRegister sd, SRegister sm, Condition cond = AL);
  void vcvtdu(DRegister dd, SRegister sm, Condition cond = AL);

  void vcmps(SRegister sd, SRegister sm, Condition cond = AL);
  void vcmpd(DRegister dd, DRegister dm, Condition cond = AL);
  void vcmpsz(SRegister sd, Condition cond = AL);
  void vcmpdz(DRegister dd, Condition cond = AL);
  void vmstat(Condition cond = AL);  // VMRS APSR_nzcv, FPSCR

  // Branch instructions.
  void b(Label* label, Condition cond = AL);
  void bl(Label* label, Condition cond = AL);
  void blx(Register rm, Condition cond = AL);
  void bx(Register rm, Condition cond = AL);

  // Macros.
  // Add signed constant value to rd. May clobber IP.
  void AddConstant(Register rd, int32_t value, Condition cond = AL);
  void AddConstant(Register rd, Register rn, int32_t value,
                   Condition cond = AL);
  void AddConstantSetFlags(Register rd, Register rn, int32_t value,
                           Condition cond = AL);
  void AddConstantWithCarry(Register rd, Register rn, int32_t value,
                            Condition cond = AL);

  // Load and Store. May clobber IP.
  void LoadImmediate(Register rd, int32_t value, Condition cond = AL);
  void LoadSImmediate(SRegister sd, float value, Condition cond = AL);
  void LoadDImmediate(DRegister dd, double value,
                      Register scratch, Condition cond = AL);
  void MarkExceptionHandler(Label* label);
  void LoadFromOffset(LoadOperandType type,
                      Register reg,
                      Register base,
                      int32_t offset,
                      Condition cond = AL);
  void StoreToOffset(StoreOperandType type,
                     Register reg,
                     Register base,
                     int32_t offset,
                     Condition cond = AL);
  void LoadSFromOffset(SRegister reg,
                       Register base,
                       int32_t offset,
                       Condition cond = AL);
  void StoreSToOffset(SRegister reg,
                      Register base,
                      int32_t offset,
                      Condition cond = AL);
  void LoadDFromOffset(DRegister reg,
                       Register base,
                       int32_t offset,
                       Condition cond = AL);
  void StoreDToOffset(DRegister reg,
                      Register base,
                      int32_t offset,
                      Condition cond = AL);

  void Push(Register rd, Condition cond = AL);
  void Pop(Register rd, Condition cond = AL);

  void PushList(RegList regs, Condition cond = AL);
  void PopList(RegList regs, Condition cond = AL);

  void Mov(Register rd, Register rm, Condition cond = AL);

  // Convenience shift instructions. Use mov instruction with shifter operand
  // for variants setting the status flags or using a register shift count.
  void Lsl(Register rd, Register rm, uint32_t shift_imm, Condition cond = AL);
  void Lsr(Register rd, Register rm, uint32_t shift_imm, Condition cond = AL);
  void Asr(Register rd, Register rm, uint32_t shift_imm, Condition cond = AL);
  void Ror(Register rd, Register rm, uint32_t shift_imm, Condition cond = AL);
  void Rrx(Register rd, Register rm, Condition cond = AL);

  // Encode a signed constant in tst instructions, only affecting the flags.
  void EncodeUint32InTstInstructions(uint32_t data);
  // ... and decode from a pc pointing to the start of encoding instructions.
  static uint32_t DecodeUint32FromTstInstructions(uword pc);
  static bool IsInstructionForExceptionHandling(uword pc);

  // Emit data (e.g. encoded instruction or immediate) to the
  // instruction stream.
  void Emit(int32_t value);
  void Bind(Label* label);

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

  virtual void MemoryBarrier(ManagedRegister scratch);

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
  void EmitType01(Condition cond,
                  int type,
                  Opcode opcode,
                  int set_cc,
                  Register rn,
                  Register rd,
                  ShifterOperand so);

  void EmitType5(Condition cond, int offset, bool link);

  void EmitMemOp(Condition cond,
                 bool load,
                 bool byte,
                 Register rd,
                 Address ad);

  void EmitMemOpAddressMode3(Condition cond,
                             int32_t mode,
                             Register rd,
                             Address ad);

  void EmitMultiMemOp(Condition cond,
                      BlockAddressMode am,
                      bool load,
                      Register base,
                      RegList regs);

  void EmitShiftImmediate(Condition cond,
                          Shift opcode,
                          Register rd,
                          Register rm,
                          ShifterOperand so);

  void EmitShiftRegister(Condition cond,
                         Shift opcode,
                         Register rd,
                         Register rm,
                         ShifterOperand so);

  void EmitMulOp(Condition cond,
                 int32_t opcode,
                 Register rd,
                 Register rn,
                 Register rm,
                 Register rs);

  void EmitVFPsss(Condition cond,
                  int32_t opcode,
                  SRegister sd,
                  SRegister sn,
                  SRegister sm);

  void EmitVFPddd(Condition cond,
                  int32_t opcode,
                  DRegister dd,
                  DRegister dn,
                  DRegister dm);

  void EmitVFPsd(Condition cond,
                 int32_t opcode,
                 SRegister sd,
                 DRegister dm);

  void EmitVFPds(Condition cond,
                 int32_t opcode,
                 DRegister dd,
                 SRegister sm);

  void EmitBranch(Condition cond, Label* label, bool link);
  static int32_t EncodeBranchOffset(int offset, int32_t inst);
  static int DecodeBranchOffset(int32_t inst);
  int32_t EncodeTstOffset(int offset, int32_t inst);
  int DecodeTstOffset(int32_t inst);

  // Returns whether or not the given register is used for passing parameters.
  static int RegisterCompare(const Register* reg1, const Register* reg2) {
    return *reg1 - *reg2;
  }
};

// Slowpath entered when Thread::Current()->_exception is non-null
class ArmExceptionSlowPath : public SlowPath {
 public:
  explicit ArmExceptionSlowPath(ArmManagedRegister scratch, size_t stack_adjust)
      : scratch_(scratch), stack_adjust_(stack_adjust) {
  }
  virtual void Emit(Assembler *sp_asm);
 private:
  const ArmManagedRegister scratch_;
  const size_t stack_adjust_;
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM_ASSEMBLER_ARM_H_
