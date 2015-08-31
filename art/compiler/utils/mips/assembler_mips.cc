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

#include "assembler_mips.h"

#include "base/casts.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "memory_region.h"
#include "thread.h"

namespace art {
namespace mips {
#if 0
class DirectCallRelocation : public AssemblerFixup {
 public:
  void Process(const MemoryRegion& region, int position) {
    // Direct calls are relative to the following instruction on mips.
    int32_t pointer = region.Load<int32_t>(position);
    int32_t start = reinterpret_cast<int32_t>(region.start());
    int32_t delta = start + position + sizeof(int32_t);
    region.Store<int32_t>(position, pointer - delta);
  }
};
#endif

std::ostream& operator<<(std::ostream& os, const DRegister& rhs) {
  if (rhs >= D0 && rhs < kNumberOfDRegisters) {
    os << "d" << static_cast<int>(rhs);
  } else {
    os << "DRegister[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

void MipsAssembler::Emit(int32_t value) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  buffer_.Emit<int32_t>(value);
}

void MipsAssembler::EmitR(int opcode, Register rs, Register rt, Register rd, int shamt, int funct) {
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rd, kNoRegister);
  int32_t encoding = opcode << kOpcodeShift |
                     static_cast<int32_t>(rs) << kRsShift |
                     static_cast<int32_t>(rt) << kRtShift |
                     static_cast<int32_t>(rd) << kRdShift |
                     shamt << kShamtShift |
                     funct;
  Emit(encoding);
}

void MipsAssembler::EmitI(int opcode, Register rs, Register rt, uint16_t imm) {
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  int32_t encoding = opcode << kOpcodeShift |
                     static_cast<int32_t>(rs) << kRsShift |
                     static_cast<int32_t>(rt) << kRtShift |
                     imm;
  Emit(encoding);
}

void MipsAssembler::EmitJ(int opcode, int address) {
  int32_t encoding = opcode << kOpcodeShift |
                     address;
  Emit(encoding);
}

void MipsAssembler::EmitFR(int opcode, int fmt, FRegister ft, FRegister fs, FRegister fd, int funct) {
  CHECK_NE(ft, kNoFRegister);
  CHECK_NE(fs, kNoFRegister);
  CHECK_NE(fd, kNoFRegister);
  int32_t encoding = opcode << kOpcodeShift |
                     fmt << kFmtShift |
                     static_cast<int32_t>(ft) << kFtShift |
                     static_cast<int32_t>(fs) << kFsShift |
                     static_cast<int32_t>(fd) << kFdShift |
                     funct;
  Emit(encoding);
}

void MipsAssembler::EmitFI(int opcode, int fmt, FRegister rt, uint16_t imm) {
  CHECK_NE(rt, kNoFRegister);
  int32_t encoding = opcode << kOpcodeShift |
                     fmt << kFmtShift |
                     static_cast<int32_t>(rt) << kRtShift |
                     imm;
  Emit(encoding);
}

void MipsAssembler::EmitBranch(Register rt, Register rs, Label* label, bool equal) {
  int offset;
  if (label->IsBound()) {
    offset = label->Position() - buffer_.Size();
  } else {
    // Use the offset field of the branch instruction for linking the sites.
    offset = label->position_;
    label->LinkTo(buffer_.Size());
  }
  if (equal) {
    Beq(rt, rs, (offset >> 2) & kBranchOffsetMask);
  } else {
    Bne(rt, rs, (offset >> 2) & kBranchOffsetMask);
  }
}

void MipsAssembler::EmitJump(Label* label, bool link) {
  int offset;
  if (label->IsBound()) {
    offset = label->Position() - buffer_.Size();
  } else {
    // Use the offset field of the jump instruction for linking the sites.
    offset = label->position_;
    label->LinkTo(buffer_.Size());
  }
  if (link) {
    Jal((offset >> 2) & kJumpOffsetMask);
  } else {
    J((offset >> 2) & kJumpOffsetMask);
  }
}

int32_t MipsAssembler::EncodeBranchOffset(int offset, int32_t inst, bool is_jump) {
  CHECK_ALIGNED(offset, 4);
  CHECK(IsInt(CountOneBits(kBranchOffsetMask), offset)) << offset;

  // Properly preserve only the bits supported in the instruction.
  offset >>= 2;
  if (is_jump) {
    offset &= kJumpOffsetMask;
    return (inst & ~kJumpOffsetMask) | offset;
  } else {
    offset &= kBranchOffsetMask;
    return (inst & ~kBranchOffsetMask) | offset;
  }
}

int MipsAssembler::DecodeBranchOffset(int32_t inst, bool is_jump) {
  // Sign-extend, then left-shift by 2.
  if (is_jump) {
    return (((inst & kJumpOffsetMask) << 6) >> 4);
  } else {
    return (((inst & kBranchOffsetMask) << 16) >> 14);
  }
}

void MipsAssembler::Bind(Label* label, bool is_jump) {
  CHECK(!label->IsBound());
  int bound_pc = buffer_.Size();
  while (label->IsLinked()) {
    int32_t position = label->Position();
    int32_t next = buffer_.Load<int32_t>(position);
    int32_t offset = is_jump ? bound_pc - position : bound_pc - position - 4;
    int32_t encoded = MipsAssembler::EncodeBranchOffset(offset, next, is_jump);
    buffer_.Store<int32_t>(position, encoded);
    label->position_ = MipsAssembler::DecodeBranchOffset(next, is_jump);
  }
  label->BindTo(bound_pc);
}

void MipsAssembler::Add(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x20);
}

void MipsAssembler::Addu(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x21);
}

void MipsAssembler::Addi(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x8, rs, rt, imm16);
}

void MipsAssembler::Addiu(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x9, rs, rt, imm16);
}

void MipsAssembler::Sub(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x22);
}

void MipsAssembler::Subu(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x23);
}

void MipsAssembler::Mult(Register rs, Register rt) {
  EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x18);
}

void MipsAssembler::Multu(Register rs, Register rt) {
  EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x19);
}

void MipsAssembler::Div(Register rs, Register rt) {
  EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x1a);
}

void MipsAssembler::Divu(Register rs, Register rt) {
  EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x1b);
}

void MipsAssembler::And(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x24);
}

void MipsAssembler::Andi(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xc, rs, rt, imm16);
}

void MipsAssembler::Or(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x25);
}

void MipsAssembler::Ori(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xd, rs, rt, imm16);
}

void MipsAssembler::Xor(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x26);
}

void MipsAssembler::Xori(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xe, rs, rt, imm16);
}

void MipsAssembler::Nor(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x27);
}

void MipsAssembler::Sll(Register rd, Register rs, int shamt) {
  EmitR(0, rs, static_cast<Register>(0), rd, shamt, 0x00);
}

void MipsAssembler::Srl(Register rd, Register rs, int shamt) {
  EmitR(0, rs, static_cast<Register>(0), rd, shamt, 0x02);
}

void MipsAssembler::Sra(Register rd, Register rs, int shamt) {
  EmitR(0, rs, static_cast<Register>(0), rd, shamt, 0x03);
}

void MipsAssembler::Sllv(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x04);
}

void MipsAssembler::Srlv(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x06);
}

void MipsAssembler::Srav(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x07);
}

void MipsAssembler::Lb(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x20, rs, rt, imm16);
}

void MipsAssembler::Lh(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x21, rs, rt, imm16);
}

void MipsAssembler::Lw(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x23, rs, rt, imm16);
}

void MipsAssembler::Lbu(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x24, rs, rt, imm16);
}

void MipsAssembler::Lhu(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x25, rs, rt, imm16);
}

void MipsAssembler::Lui(Register rt, uint16_t imm16) {
  EmitI(0xf, static_cast<Register>(0), rt, imm16);
}

void MipsAssembler::Mfhi(Register rd) {
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0), rd, 0, 0x10);
}

void MipsAssembler::Mflo(Register rd) {
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0), rd, 0, 0x12);
}

void MipsAssembler::Sb(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x28, rs, rt, imm16);
}

void MipsAssembler::Sh(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x29, rs, rt, imm16);
}

void MipsAssembler::Sw(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x2b, rs, rt, imm16);
}

void MipsAssembler::Slt(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x2a);
}

void MipsAssembler::Sltu(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x2b);
}

void MipsAssembler::Slti(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xa, rs, rt, imm16);
}

void MipsAssembler::Sltiu(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xb, rs, rt, imm16);
}

void MipsAssembler::Beq(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x4, rs, rt, imm16);
  Nop();
}

void MipsAssembler::Bne(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x5, rs, rt, imm16);
  Nop();
}

void MipsAssembler::J(uint32_t address) {
  EmitJ(0x2, address);
  Nop();
}

void MipsAssembler::Jal(uint32_t address) {
  EmitJ(0x2, address);
  Nop();
}

void MipsAssembler::Jr(Register rs) {
  EmitR(0, rs, static_cast<Register>(0), static_cast<Register>(0), 0, 0x08);
  Nop();
}

void MipsAssembler::Jalr(Register rs) {
  EmitR(0, rs, static_cast<Register>(0), RA, 0, 0x09);
  Nop();
}

void MipsAssembler::AddS(FRegister fd, FRegister fs, FRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x0);
}

void MipsAssembler::SubS(FRegister fd, FRegister fs, FRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x1);
}

void MipsAssembler::MulS(FRegister fd, FRegister fs, FRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x2);
}

void MipsAssembler::DivS(FRegister fd, FRegister fs, FRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x3);
}

void MipsAssembler::AddD(DRegister fd, DRegister fs, DRegister ft) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(ft), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x0);
}

void MipsAssembler::SubD(DRegister fd, DRegister fs, DRegister ft) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(ft), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x1);
}

void MipsAssembler::MulD(DRegister fd, DRegister fs, DRegister ft) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(ft), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x2);
}

void MipsAssembler::DivD(DRegister fd, DRegister fs, DRegister ft) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(ft), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x3);
}

void MipsAssembler::MovS(FRegister fd, FRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x6);
}

void MipsAssembler::MovD(DRegister fd, DRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(0), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x6);
}

void MipsAssembler::Mfc1(Register rt, FRegister fs) {
  EmitFR(0x11, 0x00, static_cast<FRegister>(rt), fs, static_cast<FRegister>(0), 0x0);
}

void MipsAssembler::Mtc1(FRegister ft, Register rs) {
  EmitFR(0x11, 0x04, ft, static_cast<FRegister>(rs), static_cast<FRegister>(0), 0x0);
}

void MipsAssembler::Lwc1(FRegister ft, Register rs, uint16_t imm16) {
  EmitI(0x31, rs, static_cast<Register>(ft), imm16);
}

void MipsAssembler::Ldc1(DRegister ft, Register rs, uint16_t imm16) {
  EmitI(0x35, rs, static_cast<Register>(ft), imm16);
}

void MipsAssembler::Swc1(FRegister ft, Register rs, uint16_t imm16) {
  EmitI(0x39, rs, static_cast<Register>(ft), imm16);
}

void MipsAssembler::Sdc1(DRegister ft, Register rs, uint16_t imm16) {
  EmitI(0x3d, rs, static_cast<Register>(ft), imm16);
}

void MipsAssembler::Break() {
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0),
        static_cast<Register>(0), 0, 0xD);
}

void MipsAssembler::Nop() {
  EmitR(0x0, static_cast<Register>(0), static_cast<Register>(0), static_cast<Register>(0), 0, 0x0);
}

void MipsAssembler::Move(Register rt, Register rs) {
  EmitI(0x8, rs, rt, 0);
}

void MipsAssembler::Clear(Register rt) {
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0), rt, 0, 0x20);
}

void MipsAssembler::Not(Register rt, Register rs) {
  EmitR(0, static_cast<Register>(0), rs, rt, 0, 0x27);
}

void MipsAssembler::Mul(Register rd, Register rs, Register rt) {
  Mult(rs, rt);
  Mflo(rd);
}

void MipsAssembler::Div(Register rd, Register rs, Register rt) {
  Div(rs, rt);
  Mflo(rd);
}

void MipsAssembler::Rem(Register rd, Register rs, Register rt) {
  Div(rs, rt);
  Mfhi(rd);
}

void MipsAssembler::AddConstant(Register rt, Register rs, int32_t value) {
  Addi(rt, rs, value);
}

void MipsAssembler::LoadImmediate(Register rt, int32_t value) {
  Addi(rt, ZERO, value);
}

void MipsAssembler::EmitLoad(ManagedRegister m_dst, Register src_register, int32_t src_offset,
                             size_t size) {
  MipsManagedRegister dst = m_dst.AsMips();
  if (dst.IsNoRegister()) {
    CHECK_EQ(0u, size) << dst;
  } else if (dst.IsCoreRegister()) {
    CHECK_EQ(4u, size) << dst;
    LoadFromOffset(kLoadWord, dst.AsCoreRegister(), src_register, src_offset);
  } else if (dst.IsRegisterPair()) {
    CHECK_EQ(8u, size) << dst;
    LoadFromOffset(kLoadWord, dst.AsRegisterPairLow(), src_register, src_offset);
    LoadFromOffset(kLoadWord, dst.AsRegisterPairHigh(), src_register, src_offset + 4);
  } else if (dst.IsFRegister()) {
    LoadSFromOffset(dst.AsFRegister(), src_register, src_offset);
  } else {
    CHECK(dst.IsDRegister()) << dst;
    LoadDFromOffset(dst.AsDRegister(), src_register, src_offset);
  }
}

void MipsAssembler::LoadFromOffset(LoadOperandType type, Register reg, Register base,
                                   int32_t offset) {
  switch (type) {
    case kLoadSignedByte:
      Lb(reg, base, offset);
      break;
    case kLoadUnsignedByte:
      Lbu(reg, base, offset);
      break;
    case kLoadSignedHalfword:
      Lh(reg, base, offset);
      break;
    case kLoadUnsignedHalfword:
      Lhu(reg, base, offset);
      break;
    case kLoadWord:
      Lw(reg, base, offset);
      break;
    case kLoadWordPair:
      LOG(FATAL) << "UNREACHABLE";
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
}

void MipsAssembler::LoadSFromOffset(FRegister reg, Register base, int32_t offset) {
  Lwc1(reg, base, offset);
}

void MipsAssembler::LoadDFromOffset(DRegister reg, Register base, int32_t offset) {
  Ldc1(reg, base, offset);
}

void MipsAssembler::StoreToOffset(StoreOperandType type, Register reg, Register base,
                                  int32_t offset) {
  switch (type) {
    case kStoreByte:
      Sb(reg, base, offset);
      break;
    case kStoreHalfword:
      Sh(reg, base, offset);
      break;
    case kStoreWord:
      Sw(reg, base, offset);
      break;
    case kStoreWordPair:
      LOG(FATAL) << "UNREACHABLE";
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
}

void MipsAssembler::StoreFToOffset(FRegister reg, Register base, int32_t offset) {
  Swc1(reg, base, offset);
}

void MipsAssembler::StoreDToOffset(DRegister reg, Register base, int32_t offset) {
  Sdc1(reg, base, offset);
}

void MipsAssembler::BuildFrame(size_t frame_size, ManagedRegister method_reg,
                               const std::vector<ManagedRegister>& callee_save_regs,
                               const std::vector<ManagedRegister>& entry_spills) {
  CHECK_ALIGNED(frame_size, kStackAlignment);

  // Increase frame to required size.
  IncreaseFrameSize(frame_size);

  // Push callee saves and return address
  int stack_offset = frame_size - kPointerSize;
  StoreToOffset(kStoreWord, RA, SP, stack_offset);
  for (int i = callee_save_regs.size() - 1; i >= 0; --i) {
    stack_offset -= kPointerSize;
    Register reg = callee_save_regs.at(i).AsMips().AsCoreRegister();
    StoreToOffset(kStoreWord, reg, SP, stack_offset);
  }

  // Write out Method*.
  StoreToOffset(kStoreWord, method_reg.AsMips().AsCoreRegister(), SP, 0);

  // Write out entry spills.
  for (size_t i = 0; i < entry_spills.size(); ++i) {
    Register reg = entry_spills.at(i).AsMips().AsCoreRegister();
    StoreToOffset(kStoreWord, reg, SP, frame_size + kPointerSize + (i * kPointerSize));
  }
}

void MipsAssembler::RemoveFrame(size_t frame_size,
                                const std::vector<ManagedRegister>& callee_save_regs) {
  CHECK_ALIGNED(frame_size, kStackAlignment);

  // Pop callee saves and return address
  int stack_offset = frame_size - (callee_save_regs.size() * kPointerSize) - kPointerSize;
  for (size_t i = 0; i < callee_save_regs.size(); ++i) {
    Register reg = callee_save_regs.at(i).AsMips().AsCoreRegister();
    LoadFromOffset(kLoadWord, reg, SP, stack_offset);
    stack_offset += kPointerSize;
  }
  LoadFromOffset(kLoadWord, RA, SP, stack_offset);

  // Decrease frame to required size.
  DecreaseFrameSize(frame_size);

  // Then jump to the return address.
  Jr(RA);
}

void MipsAssembler::IncreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kStackAlignment);
  AddConstant(SP, SP, -adjust);
}

void MipsAssembler::DecreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kStackAlignment);
  AddConstant(SP, SP, adjust);
}

void MipsAssembler::Store(FrameOffset dest, ManagedRegister msrc, size_t size) {
  MipsManagedRegister src = msrc.AsMips();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsCoreRegister()) {
    CHECK_EQ(4u, size);
    StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
  } else if (src.IsRegisterPair()) {
    CHECK_EQ(8u, size);
    StoreToOffset(kStoreWord, src.AsRegisterPairLow(), SP, dest.Int32Value());
    StoreToOffset(kStoreWord, src.AsRegisterPairHigh(),
                  SP, dest.Int32Value() + 4);
  } else if (src.IsFRegister()) {
    StoreFToOffset(src.AsFRegister(), SP, dest.Int32Value());
  } else {
    CHECK(src.IsDRegister());
    StoreDToOffset(src.AsDRegister(), SP, dest.Int32Value());
  }
}

void MipsAssembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  MipsManagedRegister src = msrc.AsMips();
  CHECK(src.IsCoreRegister());
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  MipsManagedRegister src = msrc.AsMips();
  CHECK(src.IsCoreRegister());
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                          ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadImmediate(scratch.AsCoreRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreImmediateToThread(ThreadOffset dest, uint32_t imm,
                                           ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadImmediate(scratch.AsCoreRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), S1, dest.Int32Value());
}

void MipsAssembler::StoreStackOffsetToThread(ThreadOffset thr_offs,
                                             FrameOffset fr_offs,
                                             ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  AddConstant(scratch.AsCoreRegister(), SP, fr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                S1, thr_offs.Int32Value());
}

void MipsAssembler::StoreStackPointerToThread(ThreadOffset thr_offs) {
  StoreToOffset(kStoreWord, SP, S1, thr_offs.Int32Value());
}

void MipsAssembler::StoreSpanning(FrameOffset dest, ManagedRegister msrc,
                                  FrameOffset in_off, ManagedRegister mscratch) {
  MipsManagedRegister src = msrc.AsMips();
  MipsManagedRegister scratch = mscratch.AsMips();
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, in_off.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value() + 4);
}

void MipsAssembler::Load(ManagedRegister mdest, FrameOffset src, size_t size) {
  return EmitLoad(mdest, SP, src.Int32Value(), size);
}

void MipsAssembler::Load(ManagedRegister mdest, ThreadOffset src, size_t size) {
  return EmitLoad(mdest, S1, src.Int32Value(), size);
}

void MipsAssembler::LoadRef(ManagedRegister mdest, FrameOffset src) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(), SP, src.Int32Value());
}

void MipsAssembler::LoadRef(ManagedRegister mdest, ManagedRegister base,
                            MemberOffset offs) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister() && dest.IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(),
                 base.AsMips().AsCoreRegister(), offs.Int32Value());
}

void MipsAssembler::LoadRawPtr(ManagedRegister mdest, ManagedRegister base,
                               Offset offs) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister() && dest.IsCoreRegister()) << dest;
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(),
                 base.AsMips().AsCoreRegister(), offs.Int32Value());
}

void MipsAssembler::LoadRawPtrFromThread(ManagedRegister mdest,
                                         ThreadOffset offs) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(), S1, offs.Int32Value());
}

void MipsAssembler::SignExtend(ManagedRegister /*mreg*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "no sign extension necessary for mips";
}

void MipsAssembler::ZeroExtend(ManagedRegister /*mreg*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "no zero extension necessary for mips";
}

void MipsAssembler::Move(ManagedRegister mdest, ManagedRegister msrc, size_t /*size*/) {
  MipsManagedRegister dest = mdest.AsMips();
  MipsManagedRegister src = msrc.AsMips();
  if (!dest.Equals(src)) {
    if (dest.IsCoreRegister()) {
      CHECK(src.IsCoreRegister()) << src;
      Move(dest.AsCoreRegister(), src.AsCoreRegister());
    } else if (dest.IsFRegister()) {
      CHECK(src.IsFRegister()) << src;
      MovS(dest.AsFRegister(), src.AsFRegister());
    } else if (dest.IsDRegister()) {
      CHECK(src.IsDRegister()) << src;
      MovD(dest.AsDRegister(), src.AsDRegister());
    } else {
      CHECK(dest.IsRegisterPair()) << dest;
      CHECK(src.IsRegisterPair()) << src;
      // Ensure that the first move doesn't clobber the input of the second
      if (src.AsRegisterPairHigh() != dest.AsRegisterPairLow()) {
        Move(dest.AsRegisterPairLow(), src.AsRegisterPairLow());
        Move(dest.AsRegisterPairHigh(), src.AsRegisterPairHigh());
      } else {
        Move(dest.AsRegisterPairHigh(), src.AsRegisterPairHigh());
        Move(dest.AsRegisterPairLow(), src.AsRegisterPairLow());
      }
    }
  }
}

void MipsAssembler::CopyRef(FrameOffset dest, FrameOffset src,
                            ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                         ThreadOffset thr_offs,
                                         ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 S1, thr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                SP, fr_offs.Int32Value());
}

void MipsAssembler::CopyRawPtrToThread(ThreadOffset thr_offs,
                                       FrameOffset fr_offs,
                                       ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 SP, fr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                S1, thr_offs.Int32Value());
}

void MipsAssembler::Copy(FrameOffset dest, FrameOffset src,
                         ManagedRegister mscratch, size_t size) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value() + 4);
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value() + 4);
  }
}

void MipsAssembler::Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                         ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsMips().AsCoreRegister();
  CHECK_EQ(size, 4u);
  LoadFromOffset(kLoadWord, scratch, src_base.AsMips().AsCoreRegister(), src_offset.Int32Value());
  StoreToOffset(kStoreWord, scratch, SP, dest.Int32Value());
}

void MipsAssembler::Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                         ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsMips().AsCoreRegister();
  CHECK_EQ(size, 4u);
  LoadFromOffset(kLoadWord, scratch, SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch, dest_base.AsMips().AsCoreRegister(), dest_offset.Int32Value());
}

void MipsAssembler::Copy(FrameOffset /*dest*/, FrameOffset /*src_base*/, Offset /*src_offset*/,
                         ManagedRegister /*mscratch*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::Copy(ManagedRegister dest, Offset dest_offset,
                         ManagedRegister src, Offset src_offset,
                         ManagedRegister mscratch, size_t size) {
  CHECK_EQ(size, 4u);
  Register scratch = mscratch.AsMips().AsCoreRegister();
  LoadFromOffset(kLoadWord, scratch, src.AsMips().AsCoreRegister(), src_offset.Int32Value());
  StoreToOffset(kStoreWord, scratch, dest.AsMips().AsCoreRegister(), dest_offset.Int32Value());
}

void MipsAssembler::Copy(FrameOffset /*dest*/, Offset /*dest_offset*/, FrameOffset /*src*/, Offset /*src_offset*/,
                         ManagedRegister /*mscratch*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::MemoryBarrier(ManagedRegister) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::CreateSirtEntry(ManagedRegister mout_reg,
                                    FrameOffset sirt_offset,
                                    ManagedRegister min_reg, bool null_allowed) {
  MipsManagedRegister out_reg = mout_reg.AsMips();
  MipsManagedRegister in_reg = min_reg.AsMips();
  CHECK(in_reg.IsNoRegister() || in_reg.IsCoreRegister()) << in_reg;
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  if (null_allowed) {
    Label null_arg;
    // Null values get a SIRT entry value of 0.  Otherwise, the SIRT entry is
    // the address in the SIRT holding the reference.
    // e.g. out_reg = (handle == 0) ? 0 : (SP+handle_offset)
    if (in_reg.IsNoRegister()) {
      LoadFromOffset(kLoadWord, out_reg.AsCoreRegister(),
                     SP, sirt_offset.Int32Value());
      in_reg = out_reg;
    }
    if (!out_reg.Equals(in_reg)) {
      LoadImmediate(out_reg.AsCoreRegister(), 0);
    }
    EmitBranch(in_reg.AsCoreRegister(), ZERO, &null_arg, true);
    AddConstant(out_reg.AsCoreRegister(), SP, sirt_offset.Int32Value());
    Bind(&null_arg, false);
  } else {
    AddConstant(out_reg.AsCoreRegister(), SP, sirt_offset.Int32Value());
  }
}

void MipsAssembler::CreateSirtEntry(FrameOffset out_off,
                                    FrameOffset sirt_offset,
                                    ManagedRegister mscratch,
                                    bool null_allowed) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  if (null_allowed) {
    Label null_arg;
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP,
                   sirt_offset.Int32Value());
    // Null values get a SIRT entry value of 0.  Otherwise, the sirt entry is
    // the address in the SIRT holding the reference.
    // e.g. scratch = (scratch == 0) ? 0 : (SP+sirt_offset)
    EmitBranch(scratch.AsCoreRegister(), ZERO, &null_arg, true);
    AddConstant(scratch.AsCoreRegister(), SP, sirt_offset.Int32Value());
    Bind(&null_arg, false);
  } else {
    AddConstant(scratch.AsCoreRegister(), SP, sirt_offset.Int32Value());
  }
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, out_off.Int32Value());
}

// Given a SIRT entry, load the associated reference.
void MipsAssembler::LoadReferenceFromSirt(ManagedRegister mout_reg,
                                          ManagedRegister min_reg) {
  MipsManagedRegister out_reg = mout_reg.AsMips();
  MipsManagedRegister in_reg = min_reg.AsMips();
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  CHECK(in_reg.IsCoreRegister()) << in_reg;
  Label null_arg;
  if (!out_reg.Equals(in_reg)) {
    LoadImmediate(out_reg.AsCoreRegister(), 0);
  }
  EmitBranch(in_reg.AsCoreRegister(), ZERO, &null_arg, true);
  LoadFromOffset(kLoadWord, out_reg.AsCoreRegister(),
                 in_reg.AsCoreRegister(), 0);
  Bind(&null_arg, false);
}

void MipsAssembler::VerifyObject(ManagedRegister /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references
}

void MipsAssembler::VerifyObject(FrameOffset /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references
}

void MipsAssembler::Call(ManagedRegister mbase, Offset offset, ManagedRegister mscratch) {
  MipsManagedRegister base = mbase.AsMips();
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(base.IsCoreRegister()) << base;
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 base.AsCoreRegister(), offset.Int32Value());
  Jalr(scratch.AsCoreRegister());
  // TODO: place reference map on call
}

void MipsAssembler::Call(FrameOffset base, Offset offset, ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  // Call *(*(SP + base) + offset)
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 SP, base.Int32Value());
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 scratch.AsCoreRegister(), offset.Int32Value());
  Jalr(scratch.AsCoreRegister());
  // TODO: place reference map on call
}

void MipsAssembler::Call(ThreadOffset /*offset*/, ManagedRegister /*mscratch*/) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::GetCurrentThread(ManagedRegister tr) {
  Move(tr.AsMips().AsCoreRegister(), S1);
}

void MipsAssembler::GetCurrentThread(FrameOffset offset,
                                     ManagedRegister /*mscratch*/) {
  StoreToOffset(kStoreWord, S1, SP, offset.Int32Value());
}

void MipsAssembler::ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust) {
  MipsManagedRegister scratch = mscratch.AsMips();
  MipsExceptionSlowPath* slow = new MipsExceptionSlowPath(scratch, stack_adjust);
  buffer_.EnqueueSlowPath(slow);
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 S1, Thread::ExceptionOffset().Int32Value());
  EmitBranch(scratch.AsCoreRegister(), ZERO, slow->Entry(), false);
}

void MipsExceptionSlowPath::Emit(Assembler* sasm) {
  MipsAssembler* sp_asm = down_cast<MipsAssembler*>(sasm);
#define __ sp_asm->
  __ Bind(&entry_, false);
  if (stack_adjust_ != 0) {  // Fix up the frame.
    __ DecreaseFrameSize(stack_adjust_);
  }
  // Pass exception object as argument
  // Don't care about preserving A0 as this call won't return
  __ Move(A0, scratch_.AsCoreRegister());
  // Set up call to Thread::Current()->pDeliverException
  __ LoadFromOffset(kLoadWord, T9, S1, QUICK_ENTRYPOINT_OFFSET(pDeliverException).Int32Value());
  __ Jr(T9);
  // Call never returns
  __ Break();
#undef __
}

}  // namespace mips
}  // namespace art
