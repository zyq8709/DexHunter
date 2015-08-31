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

#include "assembler_arm.h"

#include "base/logging.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "offsets.h"
#include "thread.h"
#include "utils.h"

namespace art {
namespace arm {

// Instruction encoding bits.
enum {
  H   = 1 << 5,   // halfword (or byte)
  L   = 1 << 20,  // load (or store)
  S   = 1 << 20,  // set condition code (or leave unchanged)
  W   = 1 << 21,  // writeback base register (or leave unchanged)
  A   = 1 << 21,  // accumulate in multiply instruction (or not)
  B   = 1 << 22,  // unsigned byte (or word)
  N   = 1 << 22,  // long (or short)
  U   = 1 << 23,  // positive (or negative) offset/index
  P   = 1 << 24,  // offset/pre-indexed addressing (or post-indexed addressing)
  I   = 1 << 25,  // immediate shifter operand (or not)

  B0 = 1,
  B1 = 1 << 1,
  B2 = 1 << 2,
  B3 = 1 << 3,
  B4 = 1 << 4,
  B5 = 1 << 5,
  B6 = 1 << 6,
  B7 = 1 << 7,
  B8 = 1 << 8,
  B9 = 1 << 9,
  B10 = 1 << 10,
  B11 = 1 << 11,
  B12 = 1 << 12,
  B16 = 1 << 16,
  B17 = 1 << 17,
  B18 = 1 << 18,
  B19 = 1 << 19,
  B20 = 1 << 20,
  B21 = 1 << 21,
  B22 = 1 << 22,
  B23 = 1 << 23,
  B24 = 1 << 24,
  B25 = 1 << 25,
  B26 = 1 << 26,
  B27 = 1 << 27,

  // Instruction bit masks.
  RdMask = 15 << 12,  // in str instruction
  CondMask = 15 << 28,
  CoprocessorMask = 15 << 8,
  OpCodeMask = 15 << 21,  // in data-processing instructions
  Imm24Mask = (1 << 24) - 1,
  Off12Mask = (1 << 12) - 1,

  // ldrex/strex register field encodings.
  kLdExRnShift = 16,
  kLdExRtShift = 12,
  kStrExRnShift = 16,
  kStrExRdShift = 12,
  kStrExRtShift = 0,
};


static const char* kRegisterNames[] = {
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
  "fp", "ip", "sp", "lr", "pc"
};
std::ostream& operator<<(std::ostream& os, const Register& rhs) {
  if (rhs >= R0 && rhs <= PC) {
    os << kRegisterNames[rhs];
  } else {
    os << "Register[" << static_cast<int>(rhs) << "]";
  }
  return os;
}


std::ostream& operator<<(std::ostream& os, const SRegister& rhs) {
  if (rhs >= S0 && rhs < kNumberOfSRegisters) {
    os << "s" << static_cast<int>(rhs);
  } else {
    os << "SRegister[" << static_cast<int>(rhs) << "]";
  }
  return os;
}


std::ostream& operator<<(std::ostream& os, const DRegister& rhs) {
  if (rhs >= D0 && rhs < kNumberOfDRegisters) {
    os << "d" << static_cast<int>(rhs);
  } else {
    os << "DRegister[" << static_cast<int>(rhs) << "]";
  }
  return os;
}


static const char* kConditionNames[] = {
  "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT",
  "LE", "AL",
};
std::ostream& operator<<(std::ostream& os, const Condition& rhs) {
  if (rhs >= EQ && rhs <= AL) {
    os << kConditionNames[rhs];
  } else {
    os << "Condition[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

void ArmAssembler::Emit(int32_t value) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  buffer_.Emit<int32_t>(value);
}


void ArmAssembler::EmitType01(Condition cond,
                              int type,
                              Opcode opcode,
                              int set_cc,
                              Register rn,
                              Register rd,
                              ShifterOperand so) {
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = static_cast<int32_t>(cond) << kConditionShift |
                     type << kTypeShift |
                     static_cast<int32_t>(opcode) << kOpcodeShift |
                     set_cc << kSShift |
                     static_cast<int32_t>(rn) << kRnShift |
                     static_cast<int32_t>(rd) << kRdShift |
                     so.encoding();
  Emit(encoding);
}


void ArmAssembler::EmitType5(Condition cond, int offset, bool link) {
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = static_cast<int32_t>(cond) << kConditionShift |
                     5 << kTypeShift |
                     (link ? 1 : 0) << kLinkShift;
  Emit(ArmAssembler::EncodeBranchOffset(offset, encoding));
}


void ArmAssembler::EmitMemOp(Condition cond,
                             bool load,
                             bool byte,
                             Register rd,
                             Address ad) {
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B26 |
                     (load ? L : 0) |
                     (byte ? B : 0) |
                     (static_cast<int32_t>(rd) << kRdShift) |
                     ad.encoding();
  Emit(encoding);
}


void ArmAssembler::EmitMemOpAddressMode3(Condition cond,
                                         int32_t mode,
                                         Register rd,
                                         Address ad) {
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B22  |
                     mode |
                     (static_cast<int32_t>(rd) << kRdShift) |
                     ad.encoding3();
  Emit(encoding);
}


void ArmAssembler::EmitMultiMemOp(Condition cond,
                                  BlockAddressMode am,
                                  bool load,
                                  Register base,
                                  RegList regs) {
  CHECK_NE(base, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 |
                     am |
                     (load ? L : 0) |
                     (static_cast<int32_t>(base) << kRnShift) |
                     regs;
  Emit(encoding);
}


void ArmAssembler::EmitShiftImmediate(Condition cond,
                                      Shift opcode,
                                      Register rd,
                                      Register rm,
                                      ShifterOperand so) {
  CHECK_NE(cond, kNoCondition);
  CHECK_EQ(so.type(), 1U);
  int32_t encoding = static_cast<int32_t>(cond) << kConditionShift |
                     static_cast<int32_t>(MOV) << kOpcodeShift |
                     static_cast<int32_t>(rd) << kRdShift |
                     so.encoding() << kShiftImmShift |
                     static_cast<int32_t>(opcode) << kShiftShift |
                     static_cast<int32_t>(rm);
  Emit(encoding);
}


void ArmAssembler::EmitShiftRegister(Condition cond,
                                     Shift opcode,
                                     Register rd,
                                     Register rm,
                                     ShifterOperand so) {
  CHECK_NE(cond, kNoCondition);
  CHECK_EQ(so.type(), 0U);
  int32_t encoding = static_cast<int32_t>(cond) << kConditionShift |
                     static_cast<int32_t>(MOV) << kOpcodeShift |
                     static_cast<int32_t>(rd) << kRdShift |
                     so.encoding() << kShiftRegisterShift |
                     static_cast<int32_t>(opcode) << kShiftShift |
                     B4 |
                     static_cast<int32_t>(rm);
  Emit(encoding);
}


void ArmAssembler::EmitBranch(Condition cond, Label* label, bool link) {
  if (label->IsBound()) {
    EmitType5(cond, label->Position() - buffer_.Size(), link);
  } else {
    int position = buffer_.Size();
    // Use the offset field of the branch instruction for linking the sites.
    EmitType5(cond, label->position_, link);
    label->LinkTo(position);
  }
}

void ArmAssembler::and_(Register rd, Register rn, ShifterOperand so,
                        Condition cond) {
  EmitType01(cond, so.type(), AND, 0, rn, rd, so);
}


void ArmAssembler::eor(Register rd, Register rn, ShifterOperand so,
                       Condition cond) {
  EmitType01(cond, so.type(), EOR, 0, rn, rd, so);
}


void ArmAssembler::sub(Register rd, Register rn, ShifterOperand so,
                       Condition cond) {
  EmitType01(cond, so.type(), SUB, 0, rn, rd, so);
}

void ArmAssembler::rsb(Register rd, Register rn, ShifterOperand so,
                       Condition cond) {
  EmitType01(cond, so.type(), RSB, 0, rn, rd, so);
}

void ArmAssembler::rsbs(Register rd, Register rn, ShifterOperand so,
                        Condition cond) {
  EmitType01(cond, so.type(), RSB, 1, rn, rd, so);
}


void ArmAssembler::add(Register rd, Register rn, ShifterOperand so,
                       Condition cond) {
  EmitType01(cond, so.type(), ADD, 0, rn, rd, so);
}


void ArmAssembler::adds(Register rd, Register rn, ShifterOperand so,
                        Condition cond) {
  EmitType01(cond, so.type(), ADD, 1, rn, rd, so);
}


void ArmAssembler::subs(Register rd, Register rn, ShifterOperand so,
                        Condition cond) {
  EmitType01(cond, so.type(), SUB, 1, rn, rd, so);
}


void ArmAssembler::adc(Register rd, Register rn, ShifterOperand so,
                       Condition cond) {
  EmitType01(cond, so.type(), ADC, 0, rn, rd, so);
}


void ArmAssembler::sbc(Register rd, Register rn, ShifterOperand so,
                       Condition cond) {
  EmitType01(cond, so.type(), SBC, 0, rn, rd, so);
}


void ArmAssembler::rsc(Register rd, Register rn, ShifterOperand so,
                       Condition cond) {
  EmitType01(cond, so.type(), RSC, 0, rn, rd, so);
}


void ArmAssembler::tst(Register rn, ShifterOperand so, Condition cond) {
  CHECK_NE(rn, PC);  // Reserve tst pc instruction for exception handler marker.
  EmitType01(cond, so.type(), TST, 1, rn, R0, so);
}


void ArmAssembler::teq(Register rn, ShifterOperand so, Condition cond) {
  CHECK_NE(rn, PC);  // Reserve teq pc instruction for exception handler marker.
  EmitType01(cond, so.type(), TEQ, 1, rn, R0, so);
}


void ArmAssembler::cmp(Register rn, ShifterOperand so, Condition cond) {
  EmitType01(cond, so.type(), CMP, 1, rn, R0, so);
}


void ArmAssembler::cmn(Register rn, ShifterOperand so, Condition cond) {
  EmitType01(cond, so.type(), CMN, 1, rn, R0, so);
}


void ArmAssembler::orr(Register rd, Register rn,
                    ShifterOperand so, Condition cond) {
  EmitType01(cond, so.type(), ORR, 0, rn, rd, so);
}


void ArmAssembler::orrs(Register rd, Register rn,
                        ShifterOperand so, Condition cond) {
  EmitType01(cond, so.type(), ORR, 1, rn, rd, so);
}


void ArmAssembler::mov(Register rd, ShifterOperand so, Condition cond) {
  EmitType01(cond, so.type(), MOV, 0, R0, rd, so);
}


void ArmAssembler::movs(Register rd, ShifterOperand so, Condition cond) {
  EmitType01(cond, so.type(), MOV, 1, R0, rd, so);
}


void ArmAssembler::bic(Register rd, Register rn, ShifterOperand so,
                       Condition cond) {
  EmitType01(cond, so.type(), BIC, 0, rn, rd, so);
}


void ArmAssembler::mvn(Register rd, ShifterOperand so, Condition cond) {
  EmitType01(cond, so.type(), MVN, 0, R0, rd, so);
}


void ArmAssembler::mvns(Register rd, ShifterOperand so, Condition cond) {
  EmitType01(cond, so.type(), MVN, 1, R0, rd, so);
}


void ArmAssembler::clz(Register rd, Register rm, Condition cond) {
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(rm, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  CHECK_NE(rd, PC);
  CHECK_NE(rm, PC);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B24 | B22 | B21 | (0xf << 16) |
                     (static_cast<int32_t>(rd) << kRdShift) |
                     (0xf << 8) | B4 | static_cast<int32_t>(rm);
  Emit(encoding);
}


void ArmAssembler::movw(Register rd, uint16_t imm16, Condition cond) {
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = static_cast<int32_t>(cond) << kConditionShift |
                     B25 | B24 | ((imm16 >> 12) << 16) |
                     static_cast<int32_t>(rd) << kRdShift | (imm16 & 0xfff);
  Emit(encoding);
}


void ArmAssembler::movt(Register rd, uint16_t imm16, Condition cond) {
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = static_cast<int32_t>(cond) << kConditionShift |
                     B25 | B24 | B22 | ((imm16 >> 12) << 16) |
                     static_cast<int32_t>(rd) << kRdShift | (imm16 & 0xfff);
  Emit(encoding);
}


void ArmAssembler::EmitMulOp(Condition cond, int32_t opcode,
                             Register rd, Register rn,
                             Register rm, Register rs) {
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(rn, kNoRegister);
  CHECK_NE(rm, kNoRegister);
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = opcode |
      (static_cast<int32_t>(cond) << kConditionShift) |
      (static_cast<int32_t>(rn) << kRnShift) |
      (static_cast<int32_t>(rd) << kRdShift) |
      (static_cast<int32_t>(rs) << kRsShift) |
      B7 | B4 |
      (static_cast<int32_t>(rm) << kRmShift);
  Emit(encoding);
}


void ArmAssembler::mul(Register rd, Register rn, Register rm, Condition cond) {
  // Assembler registers rd, rn, rm are encoded as rn, rm, rs.
  EmitMulOp(cond, 0, R0, rd, rn, rm);
}


void ArmAssembler::mla(Register rd, Register rn, Register rm, Register ra,
                       Condition cond) {
  // Assembler registers rd, rn, rm, ra are encoded as rn, rm, rs, rd.
  EmitMulOp(cond, B21, ra, rd, rn, rm);
}


void ArmAssembler::mls(Register rd, Register rn, Register rm, Register ra,
                       Condition cond) {
  // Assembler registers rd, rn, rm, ra are encoded as rn, rm, rs, rd.
  EmitMulOp(cond, B22 | B21, ra, rd, rn, rm);
}


void ArmAssembler::umull(Register rd_lo, Register rd_hi, Register rn,
                         Register rm, Condition cond) {
  // Assembler registers rd_lo, rd_hi, rn, rm are encoded as rd, rn, rm, rs.
  EmitMulOp(cond, B23, rd_lo, rd_hi, rn, rm);
}


void ArmAssembler::ldr(Register rd, Address ad, Condition cond) {
  EmitMemOp(cond, true, false, rd, ad);
}


void ArmAssembler::str(Register rd, Address ad, Condition cond) {
  EmitMemOp(cond, false, false, rd, ad);
}


void ArmAssembler::ldrb(Register rd, Address ad, Condition cond) {
  EmitMemOp(cond, true, true, rd, ad);
}


void ArmAssembler::strb(Register rd, Address ad, Condition cond) {
  EmitMemOp(cond, false, true, rd, ad);
}


void ArmAssembler::ldrh(Register rd, Address ad, Condition cond) {
  EmitMemOpAddressMode3(cond, L | B7 | H | B4, rd, ad);
}


void ArmAssembler::strh(Register rd, Address ad, Condition cond) {
  EmitMemOpAddressMode3(cond, B7 | H | B4, rd, ad);
}


void ArmAssembler::ldrsb(Register rd, Address ad, Condition cond) {
  EmitMemOpAddressMode3(cond, L | B7 | B6 | B4, rd, ad);
}


void ArmAssembler::ldrsh(Register rd, Address ad, Condition cond) {
  EmitMemOpAddressMode3(cond, L | B7 | B6 | H | B4, rd, ad);
}


void ArmAssembler::ldrd(Register rd, Address ad, Condition cond) {
  CHECK_EQ(rd % 2, 0);
  EmitMemOpAddressMode3(cond, B7 | B6 | B4, rd, ad);
}


void ArmAssembler::strd(Register rd, Address ad, Condition cond) {
  CHECK_EQ(rd % 2, 0);
  EmitMemOpAddressMode3(cond, B7 | B6 | B5 | B4, rd, ad);
}


void ArmAssembler::ldm(BlockAddressMode am,
                       Register base,
                       RegList regs,
                       Condition cond) {
  EmitMultiMemOp(cond, am, true, base, regs);
}


void ArmAssembler::stm(BlockAddressMode am,
                       Register base,
                       RegList regs,
                       Condition cond) {
  EmitMultiMemOp(cond, am, false, base, regs);
}


void ArmAssembler::ldrex(Register rt, Register rn, Condition cond) {
  CHECK_NE(rn, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B24 |
                     B23 |
                     L   |
                     (static_cast<int32_t>(rn) << kLdExRnShift) |
                     (static_cast<int32_t>(rt) << kLdExRtShift) |
                     B11 | B10 | B9 | B8 | B7 | B4 | B3 | B2 | B1 | B0;
  Emit(encoding);
}


void ArmAssembler::strex(Register rd,
                         Register rt,
                         Register rn,
                         Condition cond) {
  CHECK_NE(rn, kNoRegister);
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B24 |
                     B23 |
                     (static_cast<int32_t>(rn) << kStrExRnShift) |
                     (static_cast<int32_t>(rd) << kStrExRdShift) |
                     B11 | B10 | B9 | B8 | B7 | B4 |
                     (static_cast<int32_t>(rt) << kStrExRtShift);
  Emit(encoding);
}


void ArmAssembler::clrex() {
  int32_t encoding = (kSpecialCondition << kConditionShift) |
                     B26 | B24 | B22 | B21 | B20 | (0xff << 12) | B4 | 0xf;
  Emit(encoding);
}


void ArmAssembler::nop(Condition cond) {
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B25 | B24 | B21 | (0xf << 12);
  Emit(encoding);
}


void ArmAssembler::vmovsr(SRegister sn, Register rt, Condition cond) {
  CHECK_NE(sn, kNoSRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B25 |
                     ((static_cast<int32_t>(sn) >> 1)*B16) |
                     (static_cast<int32_t>(rt)*B12) | B11 | B9 |
                     ((static_cast<int32_t>(sn) & 1)*B7) | B4;
  Emit(encoding);
}


void ArmAssembler::vmovrs(Register rt, SRegister sn, Condition cond) {
  CHECK_NE(sn, kNoSRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B25 | B20 |
                     ((static_cast<int32_t>(sn) >> 1)*B16) |
                     (static_cast<int32_t>(rt)*B12) | B11 | B9 |
                     ((static_cast<int32_t>(sn) & 1)*B7) | B4;
  Emit(encoding);
}


void ArmAssembler::vmovsrr(SRegister sm, Register rt, Register rt2,
                           Condition cond) {
  CHECK_NE(sm, kNoSRegister);
  CHECK_NE(sm, S31);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(rt2, kNoRegister);
  CHECK_NE(rt2, SP);
  CHECK_NE(rt2, PC);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B22 |
                     (static_cast<int32_t>(rt2)*B16) |
                     (static_cast<int32_t>(rt)*B12) | B11 | B9 |
                     ((static_cast<int32_t>(sm) & 1)*B5) | B4 |
                     (static_cast<int32_t>(sm) >> 1);
  Emit(encoding);
}


void ArmAssembler::vmovrrs(Register rt, Register rt2, SRegister sm,
                           Condition cond) {
  CHECK_NE(sm, kNoSRegister);
  CHECK_NE(sm, S31);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(rt2, kNoRegister);
  CHECK_NE(rt2, SP);
  CHECK_NE(rt2, PC);
  CHECK_NE(rt, rt2);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B22 | B20 |
                     (static_cast<int32_t>(rt2)*B16) |
                     (static_cast<int32_t>(rt)*B12) | B11 | B9 |
                     ((static_cast<int32_t>(sm) & 1)*B5) | B4 |
                     (static_cast<int32_t>(sm) >> 1);
  Emit(encoding);
}


void ArmAssembler::vmovdrr(DRegister dm, Register rt, Register rt2,
                           Condition cond) {
  CHECK_NE(dm, kNoDRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(rt2, kNoRegister);
  CHECK_NE(rt2, SP);
  CHECK_NE(rt2, PC);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B22 |
                     (static_cast<int32_t>(rt2)*B16) |
                     (static_cast<int32_t>(rt)*B12) | B11 | B9 | B8 |
                     ((static_cast<int32_t>(dm) >> 4)*B5) | B4 |
                     (static_cast<int32_t>(dm) & 0xf);
  Emit(encoding);
}


void ArmAssembler::vmovrrd(Register rt, Register rt2, DRegister dm,
                           Condition cond) {
  CHECK_NE(dm, kNoDRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(rt2, kNoRegister);
  CHECK_NE(rt2, SP);
  CHECK_NE(rt2, PC);
  CHECK_NE(rt, rt2);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B22 | B20 |
                     (static_cast<int32_t>(rt2)*B16) |
                     (static_cast<int32_t>(rt)*B12) | B11 | B9 | B8 |
                     ((static_cast<int32_t>(dm) >> 4)*B5) | B4 |
                     (static_cast<int32_t>(dm) & 0xf);
  Emit(encoding);
}


void ArmAssembler::vldrs(SRegister sd, Address ad, Condition cond) {
  CHECK_NE(sd, kNoSRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B24 | B20 |
                     ((static_cast<int32_t>(sd) & 1)*B22) |
                     ((static_cast<int32_t>(sd) >> 1)*B12) |
                     B11 | B9 | ad.vencoding();
  Emit(encoding);
}


void ArmAssembler::vstrs(SRegister sd, Address ad, Condition cond) {
  CHECK_NE(static_cast<Register>(ad.encoding_ & (0xf << kRnShift)), PC);
  CHECK_NE(sd, kNoSRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B24 |
                     ((static_cast<int32_t>(sd) & 1)*B22) |
                     ((static_cast<int32_t>(sd) >> 1)*B12) |
                     B11 | B9 | ad.vencoding();
  Emit(encoding);
}


void ArmAssembler::vldrd(DRegister dd, Address ad, Condition cond) {
  CHECK_NE(dd, kNoDRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B24 | B20 |
                     ((static_cast<int32_t>(dd) >> 4)*B22) |
                     ((static_cast<int32_t>(dd) & 0xf)*B12) |
                     B11 | B9 | B8 | ad.vencoding();
  Emit(encoding);
}


void ArmAssembler::vstrd(DRegister dd, Address ad, Condition cond) {
  CHECK_NE(static_cast<Register>(ad.encoding_ & (0xf << kRnShift)), PC);
  CHECK_NE(dd, kNoDRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B24 |
                     ((static_cast<int32_t>(dd) >> 4)*B22) |
                     ((static_cast<int32_t>(dd) & 0xf)*B12) |
                     B11 | B9 | B8 | ad.vencoding();
  Emit(encoding);
}


void ArmAssembler::EmitVFPsss(Condition cond, int32_t opcode,
                              SRegister sd, SRegister sn, SRegister sm) {
  CHECK_NE(sd, kNoSRegister);
  CHECK_NE(sn, kNoSRegister);
  CHECK_NE(sm, kNoSRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B25 | B11 | B9 | opcode |
                     ((static_cast<int32_t>(sd) & 1)*B22) |
                     ((static_cast<int32_t>(sn) >> 1)*B16) |
                     ((static_cast<int32_t>(sd) >> 1)*B12) |
                     ((static_cast<int32_t>(sn) & 1)*B7) |
                     ((static_cast<int32_t>(sm) & 1)*B5) |
                     (static_cast<int32_t>(sm) >> 1);
  Emit(encoding);
}


void ArmAssembler::EmitVFPddd(Condition cond, int32_t opcode,
                              DRegister dd, DRegister dn, DRegister dm) {
  CHECK_NE(dd, kNoDRegister);
  CHECK_NE(dn, kNoDRegister);
  CHECK_NE(dm, kNoDRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B25 | B11 | B9 | B8 | opcode |
                     ((static_cast<int32_t>(dd) >> 4)*B22) |
                     ((static_cast<int32_t>(dn) & 0xf)*B16) |
                     ((static_cast<int32_t>(dd) & 0xf)*B12) |
                     ((static_cast<int32_t>(dn) >> 4)*B7) |
                     ((static_cast<int32_t>(dm) >> 4)*B5) |
                     (static_cast<int32_t>(dm) & 0xf);
  Emit(encoding);
}


void ArmAssembler::vmovs(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B6, sd, S0, sm);
}


void ArmAssembler::vmovd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B6, dd, D0, dm);
}


bool ArmAssembler::vmovs(SRegister sd, float s_imm, Condition cond) {
  uint32_t imm32 = bit_cast<uint32_t, float>(s_imm);
  if (((imm32 & ((1 << 19) - 1)) == 0) &&
      ((((imm32 >> 25) & ((1 << 6) - 1)) == (1 << 5)) ||
       (((imm32 >> 25) & ((1 << 6) - 1)) == ((1 << 5) -1)))) {
    uint8_t imm8 = ((imm32 >> 31) << 7) | (((imm32 >> 29) & 1) << 6) |
        ((imm32 >> 19) & ((1 << 6) -1));
    EmitVFPsss(cond, B23 | B21 | B20 | ((imm8 >> 4)*B16) | (imm8 & 0xf),
               sd, S0, S0);
    return true;
  }
  return false;
}


bool ArmAssembler::vmovd(DRegister dd, double d_imm, Condition cond) {
  uint64_t imm64 = bit_cast<uint64_t, double>(d_imm);
  if (((imm64 & ((1LL << 48) - 1)) == 0) &&
      ((((imm64 >> 54) & ((1 << 9) - 1)) == (1 << 8)) ||
       (((imm64 >> 54) & ((1 << 9) - 1)) == ((1 << 8) -1)))) {
    uint8_t imm8 = ((imm64 >> 63) << 7) | (((imm64 >> 61) & 1) << 6) |
        ((imm64 >> 48) & ((1 << 6) -1));
    EmitVFPddd(cond, B23 | B21 | B20 | ((imm8 >> 4)*B16) | B8 | (imm8 & 0xf),
               dd, D0, D0);
    return true;
  }
  return false;
}


void ArmAssembler::vadds(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B21 | B20, sd, sn, sm);
}


void ArmAssembler::vaddd(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B21 | B20, dd, dn, dm);
}


void ArmAssembler::vsubs(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B21 | B20 | B6, sd, sn, sm);
}


void ArmAssembler::vsubd(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B21 | B20 | B6, dd, dn, dm);
}


void ArmAssembler::vmuls(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B21, sd, sn, sm);
}


void ArmAssembler::vmuld(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B21, dd, dn, dm);
}


void ArmAssembler::vmlas(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, 0, sd, sn, sm);
}


void ArmAssembler::vmlad(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, 0, dd, dn, dm);
}


void ArmAssembler::vmlss(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B6, sd, sn, sm);
}


void ArmAssembler::vmlsd(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B6, dd, dn, dm);
}


void ArmAssembler::vdivs(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B23, sd, sn, sm);
}


void ArmAssembler::vdivd(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B23, dd, dn, dm);
}


void ArmAssembler::vabss(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B7 | B6, sd, S0, sm);
}


void ArmAssembler::vabsd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B7 | B6, dd, D0, dm);
}


void ArmAssembler::vnegs(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B16 | B6, sd, S0, sm);
}


void ArmAssembler::vnegd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B16 | B6, dd, D0, dm);
}


void ArmAssembler::vsqrts(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B16 | B7 | B6, sd, S0, sm);
}

void ArmAssembler::vsqrtd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B16 | B7 | B6, dd, D0, dm);
}


void ArmAssembler::EmitVFPsd(Condition cond, int32_t opcode,
                             SRegister sd, DRegister dm) {
  CHECK_NE(sd, kNoSRegister);
  CHECK_NE(dm, kNoDRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B25 | B11 | B9 | opcode |
                     ((static_cast<int32_t>(sd) & 1)*B22) |
                     ((static_cast<int32_t>(sd) >> 1)*B12) |
                     ((static_cast<int32_t>(dm) >> 4)*B5) |
                     (static_cast<int32_t>(dm) & 0xf);
  Emit(encoding);
}


void ArmAssembler::EmitVFPds(Condition cond, int32_t opcode,
                             DRegister dd, SRegister sm) {
  CHECK_NE(dd, kNoDRegister);
  CHECK_NE(sm, kNoSRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B25 | B11 | B9 | opcode |
                     ((static_cast<int32_t>(dd) >> 4)*B22) |
                     ((static_cast<int32_t>(dd) & 0xf)*B12) |
                     ((static_cast<int32_t>(sm) & 1)*B5) |
                     (static_cast<int32_t>(sm) >> 1);
  Emit(encoding);
}


void ArmAssembler::vcvtsd(SRegister sd, DRegister dm, Condition cond) {
  EmitVFPsd(cond, B23 | B21 | B20 | B18 | B17 | B16 | B8 | B7 | B6, sd, dm);
}


void ArmAssembler::vcvtds(DRegister dd, SRegister sm, Condition cond) {
  EmitVFPds(cond, B23 | B21 | B20 | B18 | B17 | B16 | B7 | B6, dd, sm);
}


void ArmAssembler::vcvtis(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B19 | B18 | B16 | B7 | B6, sd, S0, sm);
}


void ArmAssembler::vcvtid(SRegister sd, DRegister dm, Condition cond) {
  EmitVFPsd(cond, B23 | B21 | B20 | B19 | B18 | B16 | B8 | B7 | B6, sd, dm);
}


void ArmAssembler::vcvtsi(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B19 | B7 | B6, sd, S0, sm);
}


void ArmAssembler::vcvtdi(DRegister dd, SRegister sm, Condition cond) {
  EmitVFPds(cond, B23 | B21 | B20 | B19 | B8 | B7 | B6, dd, sm);
}


void ArmAssembler::vcvtus(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B19 | B18 | B7 | B6, sd, S0, sm);
}


void ArmAssembler::vcvtud(SRegister sd, DRegister dm, Condition cond) {
  EmitVFPsd(cond, B23 | B21 | B20 | B19 | B18 | B8 | B7 | B6, sd, dm);
}


void ArmAssembler::vcvtsu(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B19 | B6, sd, S0, sm);
}


void ArmAssembler::vcvtdu(DRegister dd, SRegister sm, Condition cond) {
  EmitVFPds(cond, B23 | B21 | B20 | B19 | B8 | B6, dd, sm);
}


void ArmAssembler::vcmps(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B18 | B6, sd, S0, sm);
}


void ArmAssembler::vcmpd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B18 | B6, dd, D0, dm);
}


void ArmAssembler::vcmpsz(SRegister sd, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B18 | B16 | B6, sd, S0, S0);
}


void ArmAssembler::vcmpdz(DRegister dd, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B18 | B16 | B6, dd, D0, D0);
}


void ArmAssembler::vmstat(Condition cond) {  // VMRS APSR_nzcv, FPSCR
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B27 | B26 | B25 | B23 | B22 | B21 | B20 | B16 |
                     (static_cast<int32_t>(PC)*B12) |
                     B11 | B9 | B4;
  Emit(encoding);
}


void ArmAssembler::svc(uint32_t imm24) {
  CHECK(IsUint(24, imm24)) << imm24;
  int32_t encoding = (AL << kConditionShift) | B27 | B26 | B25 | B24 | imm24;
  Emit(encoding);
}


void ArmAssembler::bkpt(uint16_t imm16) {
  int32_t encoding = (AL << kConditionShift) | B24 | B21 |
                     ((imm16 >> 4) << 8) | B6 | B5 | B4 | (imm16 & 0xf);
  Emit(encoding);
}


void ArmAssembler::b(Label* label, Condition cond) {
  EmitBranch(cond, label, false);
}


void ArmAssembler::bl(Label* label, Condition cond) {
  EmitBranch(cond, label, true);
}


void ArmAssembler::blx(Register rm, Condition cond) {
  CHECK_NE(rm, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B24 | B21 | (0xfff << 8) | B5 | B4 |
                     (static_cast<int32_t>(rm) << kRmShift);
  Emit(encoding);
}

void ArmAssembler::bx(Register rm, Condition cond) {
  CHECK_NE(rm, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  int32_t encoding = (static_cast<int32_t>(cond) << kConditionShift) |
                     B24 | B21 | (0xfff << 8) | B4 |
                     (static_cast<int32_t>(rm) << kRmShift);
  Emit(encoding);
}

void ArmAssembler::MarkExceptionHandler(Label* label) {
  EmitType01(AL, 1, TST, 1, PC, R0, ShifterOperand(0));
  Label l;
  b(&l);
  EmitBranch(AL, label, false);
  Bind(&l);
}


void ArmAssembler::Bind(Label* label) {
  CHECK(!label->IsBound());
  int bound_pc = buffer_.Size();
  while (label->IsLinked()) {
    int32_t position = label->Position();
    int32_t next = buffer_.Load<int32_t>(position);
    int32_t encoded = ArmAssembler::EncodeBranchOffset(bound_pc - position, next);
    buffer_.Store<int32_t>(position, encoded);
    label->position_ = ArmAssembler::DecodeBranchOffset(next);
  }
  label->BindTo(bound_pc);
}


void ArmAssembler::EncodeUint32InTstInstructions(uint32_t data) {
  // TODO: Consider using movw ip, <16 bits>.
  while (!IsUint(8, data)) {
    tst(R0, ShifterOperand(data & 0xFF), VS);
    data >>= 8;
  }
  tst(R0, ShifterOperand(data), MI);
}


int32_t ArmAssembler::EncodeBranchOffset(int offset, int32_t inst) {
  // The offset is off by 8 due to the way the ARM CPUs read PC.
  offset -= 8;
  CHECK_ALIGNED(offset, 4);
  CHECK(IsInt(CountOneBits(kBranchOffsetMask), offset)) << offset;

  // Properly preserve only the bits supported in the instruction.
  offset >>= 2;
  offset &= kBranchOffsetMask;
  return (inst & ~kBranchOffsetMask) | offset;
}


int ArmAssembler::DecodeBranchOffset(int32_t inst) {
  // Sign-extend, left-shift by 2, then add 8.
  return ((((inst & kBranchOffsetMask) << 8) >> 6) + 8);
}

void ArmAssembler::AddConstant(Register rd, int32_t value, Condition cond) {
  AddConstant(rd, rd, value, cond);
}


void ArmAssembler::AddConstant(Register rd, Register rn, int32_t value,
                               Condition cond) {
  if (value == 0) {
    if (rd != rn) {
      mov(rd, ShifterOperand(rn), cond);
    }
    return;
  }
  // We prefer to select the shorter code sequence rather than selecting add for
  // positive values and sub for negatives ones, which would slightly improve
  // the readability of generated code for some constants.
  ShifterOperand shifter_op;
  if (ShifterOperand::CanHold(value, &shifter_op)) {
    add(rd, rn, shifter_op, cond);
  } else if (ShifterOperand::CanHold(-value, &shifter_op)) {
    sub(rd, rn, shifter_op, cond);
  } else {
    CHECK(rn != IP);
    if (ShifterOperand::CanHold(~value, &shifter_op)) {
      mvn(IP, shifter_op, cond);
      add(rd, rn, ShifterOperand(IP), cond);
    } else if (ShifterOperand::CanHold(~(-value), &shifter_op)) {
      mvn(IP, shifter_op, cond);
      sub(rd, rn, ShifterOperand(IP), cond);
    } else {
      movw(IP, Low16Bits(value), cond);
      uint16_t value_high = High16Bits(value);
      if (value_high != 0) {
        movt(IP, value_high, cond);
      }
      add(rd, rn, ShifterOperand(IP), cond);
    }
  }
}


void ArmAssembler::AddConstantSetFlags(Register rd, Register rn, int32_t value,
                                       Condition cond) {
  ShifterOperand shifter_op;
  if (ShifterOperand::CanHold(value, &shifter_op)) {
    adds(rd, rn, shifter_op, cond);
  } else if (ShifterOperand::CanHold(-value, &shifter_op)) {
    subs(rd, rn, shifter_op, cond);
  } else {
    CHECK(rn != IP);
    if (ShifterOperand::CanHold(~value, &shifter_op)) {
      mvn(IP, shifter_op, cond);
      adds(rd, rn, ShifterOperand(IP), cond);
    } else if (ShifterOperand::CanHold(~(-value), &shifter_op)) {
      mvn(IP, shifter_op, cond);
      subs(rd, rn, ShifterOperand(IP), cond);
    } else {
      movw(IP, Low16Bits(value), cond);
      uint16_t value_high = High16Bits(value);
      if (value_high != 0) {
        movt(IP, value_high, cond);
      }
      adds(rd, rn, ShifterOperand(IP), cond);
    }
  }
}


void ArmAssembler::LoadImmediate(Register rd, int32_t value, Condition cond) {
  ShifterOperand shifter_op;
  if (ShifterOperand::CanHold(value, &shifter_op)) {
    mov(rd, shifter_op, cond);
  } else if (ShifterOperand::CanHold(~value, &shifter_op)) {
    mvn(rd, shifter_op, cond);
  } else {
    movw(rd, Low16Bits(value), cond);
    uint16_t value_high = High16Bits(value);
    if (value_high != 0) {
      movt(rd, value_high, cond);
    }
  }
}


bool Address::CanHoldLoadOffset(LoadOperandType type, int offset) {
  switch (type) {
    case kLoadSignedByte:
    case kLoadSignedHalfword:
    case kLoadUnsignedHalfword:
    case kLoadWordPair:
      return IsAbsoluteUint(8, offset);  // Addressing mode 3.
    case kLoadUnsignedByte:
    case kLoadWord:
      return IsAbsoluteUint(12, offset);  // Addressing mode 2.
    case kLoadSWord:
    case kLoadDWord:
      return IsAbsoluteUint(10, offset);  // VFP addressing mode.
    default:
      LOG(FATAL) << "UNREACHABLE";
      return false;
  }
}


bool Address::CanHoldStoreOffset(StoreOperandType type, int offset) {
  switch (type) {
    case kStoreHalfword:
    case kStoreWordPair:
      return IsAbsoluteUint(8, offset);  // Addressing mode 3.
    case kStoreByte:
    case kStoreWord:
      return IsAbsoluteUint(12, offset);  // Addressing mode 2.
    case kStoreSWord:
    case kStoreDWord:
      return IsAbsoluteUint(10, offset);  // VFP addressing mode.
    default:
      LOG(FATAL) << "UNREACHABLE";
      return false;
  }
}


// Implementation note: this method must emit at most one instruction when
// Address::CanHoldLoadOffset.
void ArmAssembler::LoadFromOffset(LoadOperandType type,
                                  Register reg,
                                  Register base,
                                  int32_t offset,
                                  Condition cond) {
  if (!Address::CanHoldLoadOffset(type, offset)) {
    CHECK(base != IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldLoadOffset(type, offset));
  switch (type) {
    case kLoadSignedByte:
      ldrsb(reg, Address(base, offset), cond);
      break;
    case kLoadUnsignedByte:
      ldrb(reg, Address(base, offset), cond);
      break;
    case kLoadSignedHalfword:
      ldrsh(reg, Address(base, offset), cond);
      break;
    case kLoadUnsignedHalfword:
      ldrh(reg, Address(base, offset), cond);
      break;
    case kLoadWord:
      ldr(reg, Address(base, offset), cond);
      break;
    case kLoadWordPair:
      ldrd(reg, Address(base, offset), cond);
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldLoadOffset, as expected by JIT::GuardedLoadFromOffset.
void ArmAssembler::LoadSFromOffset(SRegister reg,
                                   Register base,
                                   int32_t offset,
                                   Condition cond) {
  if (!Address::CanHoldLoadOffset(kLoadSWord, offset)) {
    CHECK_NE(base, IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldLoadOffset(kLoadSWord, offset));
  vldrs(reg, Address(base, offset), cond);
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldLoadOffset, as expected by JIT::GuardedLoadFromOffset.
void ArmAssembler::LoadDFromOffset(DRegister reg,
                                   Register base,
                                   int32_t offset,
                                   Condition cond) {
  if (!Address::CanHoldLoadOffset(kLoadDWord, offset)) {
    CHECK_NE(base, IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldLoadOffset(kLoadDWord, offset));
  vldrd(reg, Address(base, offset), cond);
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldStoreOffset.
void ArmAssembler::StoreToOffset(StoreOperandType type,
                                 Register reg,
                                 Register base,
                                 int32_t offset,
                                 Condition cond) {
  if (!Address::CanHoldStoreOffset(type, offset)) {
    CHECK(reg != IP);
    CHECK(base != IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldStoreOffset(type, offset));
  switch (type) {
    case kStoreByte:
      strb(reg, Address(base, offset), cond);
      break;
    case kStoreHalfword:
      strh(reg, Address(base, offset), cond);
      break;
    case kStoreWord:
      str(reg, Address(base, offset), cond);
      break;
    case kStoreWordPair:
      strd(reg, Address(base, offset), cond);
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldStoreOffset, as expected by JIT::GuardedStoreToOffset.
void ArmAssembler::StoreSToOffset(SRegister reg,
                                  Register base,
                                  int32_t offset,
                                  Condition cond) {
  if (!Address::CanHoldStoreOffset(kStoreSWord, offset)) {
    CHECK_NE(base, IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldStoreOffset(kStoreSWord, offset));
  vstrs(reg, Address(base, offset), cond);
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldStoreOffset, as expected by JIT::GuardedStoreSToOffset.
void ArmAssembler::StoreDToOffset(DRegister reg,
                                  Register base,
                                  int32_t offset,
                                  Condition cond) {
  if (!Address::CanHoldStoreOffset(kStoreDWord, offset)) {
    CHECK_NE(base, IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldStoreOffset(kStoreDWord, offset));
  vstrd(reg, Address(base, offset), cond);
}

void ArmAssembler::Push(Register rd, Condition cond) {
  str(rd, Address(SP, -kRegisterSize, Address::PreIndex), cond);
}

void ArmAssembler::Pop(Register rd, Condition cond) {
  ldr(rd, Address(SP, kRegisterSize, Address::PostIndex), cond);
}

void ArmAssembler::PushList(RegList regs, Condition cond) {
  stm(DB_W, SP, regs, cond);
}

void ArmAssembler::PopList(RegList regs, Condition cond) {
  ldm(IA_W, SP, regs, cond);
}

void ArmAssembler::Mov(Register rd, Register rm, Condition cond) {
  if (rd != rm) {
    mov(rd, ShifterOperand(rm), cond);
  }
}

void ArmAssembler::Lsl(Register rd, Register rm, uint32_t shift_imm,
                       Condition cond) {
  CHECK_NE(shift_imm, 0u);  // Do not use Lsl if no shift is wanted.
  mov(rd, ShifterOperand(rm, LSL, shift_imm), cond);
}

void ArmAssembler::Lsr(Register rd, Register rm, uint32_t shift_imm,
                       Condition cond) {
  CHECK_NE(shift_imm, 0u);  // Do not use Lsr if no shift is wanted.
  if (shift_imm == 32) shift_imm = 0;  // Comply to UAL syntax.
  mov(rd, ShifterOperand(rm, LSR, shift_imm), cond);
}

void ArmAssembler::Asr(Register rd, Register rm, uint32_t shift_imm,
                       Condition cond) {
  CHECK_NE(shift_imm, 0u);  // Do not use Asr if no shift is wanted.
  if (shift_imm == 32) shift_imm = 0;  // Comply to UAL syntax.
  mov(rd, ShifterOperand(rm, ASR, shift_imm), cond);
}

void ArmAssembler::Ror(Register rd, Register rm, uint32_t shift_imm,
                       Condition cond) {
  CHECK_NE(shift_imm, 0u);  // Use Rrx instruction.
  mov(rd, ShifterOperand(rm, ROR, shift_imm), cond);
}

void ArmAssembler::Rrx(Register rd, Register rm, Condition cond) {
  mov(rd, ShifterOperand(rm, ROR, 0), cond);
}

void ArmAssembler::BuildFrame(size_t frame_size, ManagedRegister method_reg,
                              const std::vector<ManagedRegister>& callee_save_regs,
                              const std::vector<ManagedRegister>& entry_spills) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  CHECK_EQ(R0, method_reg.AsArm().AsCoreRegister());

  // Push callee saves and link register.
  RegList push_list = 1 << LR;
  size_t pushed_values = 1;
  for (size_t i = 0; i < callee_save_regs.size(); i++) {
    Register reg = callee_save_regs.at(i).AsArm().AsCoreRegister();
    push_list |= 1 << reg;
    pushed_values++;
  }
  PushList(push_list);

  // Increase frame to required size.
  CHECK_GT(frame_size, pushed_values * kPointerSize);  // Must be at least space to push Method*
  size_t adjust = frame_size - (pushed_values * kPointerSize);
  IncreaseFrameSize(adjust);

  // Write out Method*.
  StoreToOffset(kStoreWord, R0, SP, 0);

  // Write out entry spills.
  for (size_t i = 0; i < entry_spills.size(); ++i) {
    Register reg = entry_spills.at(i).AsArm().AsCoreRegister();
    StoreToOffset(kStoreWord, reg, SP, frame_size + kPointerSize + (i * kPointerSize));
  }
}

void ArmAssembler::RemoveFrame(size_t frame_size,
                              const std::vector<ManagedRegister>& callee_save_regs) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  // Compute callee saves to pop and PC
  RegList pop_list = 1 << PC;
  size_t pop_values = 1;
  for (size_t i = 0; i < callee_save_regs.size(); i++) {
    Register reg = callee_save_regs.at(i).AsArm().AsCoreRegister();
    pop_list |= 1 << reg;
    pop_values++;
  }

  // Decrease frame to start of callee saves
  CHECK_GT(frame_size, pop_values * kPointerSize);
  size_t adjust = frame_size - (pop_values * kPointerSize);
  DecreaseFrameSize(adjust);

  // Pop callee saves and PC
  PopList(pop_list);
}

void ArmAssembler::IncreaseFrameSize(size_t adjust) {
  AddConstant(SP, -adjust);
}

void ArmAssembler::DecreaseFrameSize(size_t adjust) {
  AddConstant(SP, adjust);
}

void ArmAssembler::Store(FrameOffset dest, ManagedRegister msrc, size_t size) {
  ArmManagedRegister src = msrc.AsArm();
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
  } else if (src.IsSRegister()) {
    StoreSToOffset(src.AsSRegister(), SP, dest.Int32Value());
  } else {
    CHECK(src.IsDRegister()) << src;
    StoreDToOffset(src.AsDRegister(), SP, dest.Int32Value());
  }
}

void ArmAssembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  ArmManagedRegister src = msrc.AsArm();
  CHECK(src.IsCoreRegister()) << src;
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
}

void ArmAssembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  ArmManagedRegister src = msrc.AsArm();
  CHECK(src.IsCoreRegister()) << src;
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
}

void ArmAssembler::StoreSpanning(FrameOffset dest, ManagedRegister msrc,
                              FrameOffset in_off, ManagedRegister mscratch) {
  ArmManagedRegister src = msrc.AsArm();
  ArmManagedRegister scratch = mscratch.AsArm();
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, in_off.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value() + 4);
}

void ArmAssembler::CopyRef(FrameOffset dest, FrameOffset src,
                        ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
}

void ArmAssembler::LoadRef(ManagedRegister mdest, ManagedRegister base,
                           MemberOffset offs) {
  ArmManagedRegister dst = mdest.AsArm();
  CHECK(dst.IsCoreRegister() && dst.IsCoreRegister()) << dst;
  LoadFromOffset(kLoadWord, dst.AsCoreRegister(),
                 base.AsArm().AsCoreRegister(), offs.Int32Value());
}

void ArmAssembler::LoadRef(ManagedRegister mdest, FrameOffset  src) {
  ArmManagedRegister dst = mdest.AsArm();
  CHECK(dst.IsCoreRegister()) << dst;
  LoadFromOffset(kLoadWord, dst.AsCoreRegister(), SP, src.Int32Value());
}

void ArmAssembler::LoadRawPtr(ManagedRegister mdest, ManagedRegister base,
                           Offset offs) {
  ArmManagedRegister dst = mdest.AsArm();
  CHECK(dst.IsCoreRegister() && dst.IsCoreRegister()) << dst;
  LoadFromOffset(kLoadWord, dst.AsCoreRegister(),
                 base.AsArm().AsCoreRegister(), offs.Int32Value());
}

void ArmAssembler::StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                      ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadImmediate(scratch.AsCoreRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
}

void ArmAssembler::StoreImmediateToThread(ThreadOffset dest, uint32_t imm,
                                       ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadImmediate(scratch.AsCoreRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), TR, dest.Int32Value());
}

static void EmitLoad(ArmAssembler* assembler, ManagedRegister m_dst,
                     Register src_register, int32_t src_offset, size_t size) {
  ArmManagedRegister dst = m_dst.AsArm();
  if (dst.IsNoRegister()) {
    CHECK_EQ(0u, size) << dst;
  } else if (dst.IsCoreRegister()) {
    CHECK_EQ(4u, size) << dst;
    assembler->LoadFromOffset(kLoadWord, dst.AsCoreRegister(), src_register, src_offset);
  } else if (dst.IsRegisterPair()) {
    CHECK_EQ(8u, size) << dst;
    assembler->LoadFromOffset(kLoadWord, dst.AsRegisterPairLow(), src_register, src_offset);
    assembler->LoadFromOffset(kLoadWord, dst.AsRegisterPairHigh(), src_register, src_offset + 4);
  } else if (dst.IsSRegister()) {
    assembler->LoadSFromOffset(dst.AsSRegister(), src_register, src_offset);
  } else {
    CHECK(dst.IsDRegister()) << dst;
    assembler->LoadDFromOffset(dst.AsDRegister(), src_register, src_offset);
  }
}

void ArmAssembler::Load(ManagedRegister m_dst, FrameOffset src, size_t size) {
  return EmitLoad(this, m_dst, SP, src.Int32Value(), size);
}

void ArmAssembler::Load(ManagedRegister m_dst, ThreadOffset src, size_t size) {
  return EmitLoad(this, m_dst, TR, src.Int32Value(), size);
}

void ArmAssembler::LoadRawPtrFromThread(ManagedRegister m_dst, ThreadOffset offs) {
  ArmManagedRegister dst = m_dst.AsArm();
  CHECK(dst.IsCoreRegister()) << dst;
  LoadFromOffset(kLoadWord, dst.AsCoreRegister(), TR, offs.Int32Value());
}

void ArmAssembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                        ThreadOffset thr_offs,
                                        ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 TR, thr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                SP, fr_offs.Int32Value());
}

void ArmAssembler::CopyRawPtrToThread(ThreadOffset thr_offs,
                                      FrameOffset fr_offs,
                                      ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 SP, fr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                TR, thr_offs.Int32Value());
}

void ArmAssembler::StoreStackOffsetToThread(ThreadOffset thr_offs,
                                            FrameOffset fr_offs,
                                            ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  AddConstant(scratch.AsCoreRegister(), SP, fr_offs.Int32Value(), AL);
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                TR, thr_offs.Int32Value());
}

void ArmAssembler::StoreStackPointerToThread(ThreadOffset thr_offs) {
  StoreToOffset(kStoreWord, SP, TR, thr_offs.Int32Value());
}

void ArmAssembler::SignExtend(ManagedRegister /*mreg*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "no sign extension necessary for arm";
}

void ArmAssembler::ZeroExtend(ManagedRegister /*mreg*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "no zero extension necessary for arm";
}

void ArmAssembler::Move(ManagedRegister m_dst, ManagedRegister m_src, size_t /*size*/) {
  ArmManagedRegister dst = m_dst.AsArm();
  ArmManagedRegister src = m_src.AsArm();
  if (!dst.Equals(src)) {
    if (dst.IsCoreRegister()) {
      CHECK(src.IsCoreRegister()) << src;
      mov(dst.AsCoreRegister(), ShifterOperand(src.AsCoreRegister()));
    } else if (dst.IsDRegister()) {
      CHECK(src.IsDRegister()) << src;
      vmovd(dst.AsDRegister(), src.AsDRegister());
    } else if (dst.IsSRegister()) {
      CHECK(src.IsSRegister()) << src;
      vmovs(dst.AsSRegister(), src.AsSRegister());
    } else {
      CHECK(dst.IsRegisterPair()) << dst;
      CHECK(src.IsRegisterPair()) << src;
      // Ensure that the first move doesn't clobber the input of the second
      if (src.AsRegisterPairHigh() != dst.AsRegisterPairLow()) {
        mov(dst.AsRegisterPairLow(), ShifterOperand(src.AsRegisterPairLow()));
        mov(dst.AsRegisterPairHigh(), ShifterOperand(src.AsRegisterPairHigh()));
      } else {
        mov(dst.AsRegisterPairHigh(), ShifterOperand(src.AsRegisterPairHigh()));
        mov(dst.AsRegisterPairLow(), ShifterOperand(src.AsRegisterPairLow()));
      }
    }
  }
}

void ArmAssembler::Copy(FrameOffset dest, FrameOffset src, ManagedRegister mscratch, size_t size) {
  ArmManagedRegister scratch = mscratch.AsArm();
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

void ArmAssembler::Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                        ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsArm().AsCoreRegister();
  CHECK_EQ(size, 4u);
  LoadFromOffset(kLoadWord, scratch, src_base.AsArm().AsCoreRegister(), src_offset.Int32Value());
  StoreToOffset(kStoreWord, scratch, SP, dest.Int32Value());
}

void ArmAssembler::Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                        ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsArm().AsCoreRegister();
  CHECK_EQ(size, 4u);
  LoadFromOffset(kLoadWord, scratch, SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch, dest_base.AsArm().AsCoreRegister(), dest_offset.Int32Value());
}

void ArmAssembler::Copy(FrameOffset /*dst*/, FrameOffset /*src_base*/, Offset /*src_offset*/,
                        ManagedRegister /*mscratch*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL);
}

void ArmAssembler::Copy(ManagedRegister dest, Offset dest_offset,
                        ManagedRegister src, Offset src_offset,
                        ManagedRegister mscratch, size_t size) {
  CHECK_EQ(size, 4u);
  Register scratch = mscratch.AsArm().AsCoreRegister();
  LoadFromOffset(kLoadWord, scratch, src.AsArm().AsCoreRegister(), src_offset.Int32Value());
  StoreToOffset(kStoreWord, scratch, dest.AsArm().AsCoreRegister(), dest_offset.Int32Value());
}

void ArmAssembler::Copy(FrameOffset /*dst*/, Offset /*dest_offset*/, FrameOffset /*src*/, Offset /*src_offset*/,
                        ManagedRegister /*scratch*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL);
}


void ArmAssembler::MemoryBarrier(ManagedRegister mscratch) {
  CHECK_EQ(mscratch.AsArm().AsCoreRegister(), R12);
#if ANDROID_SMP != 0
#if defined(__ARM_HAVE_DMB)
  int32_t encoding = 0xf57ff05f;  // dmb
  Emit(encoding);
#elif  defined(__ARM_HAVE_LDREX_STREX)
  LoadImmediate(R12, 0);
  int32_t encoding = 0xee07cfba;  // mcr p15, 0, r12, c7, c10, 5
  Emit(encoding);
#else
  LoadImmediate(R12, 0xffff0fa0);  // kuser_memory_barrier
  blx(R12);
#endif
#endif
}

void ArmAssembler::CreateSirtEntry(ManagedRegister mout_reg,
                                   FrameOffset sirt_offset,
                                   ManagedRegister min_reg, bool null_allowed) {
  ArmManagedRegister out_reg = mout_reg.AsArm();
  ArmManagedRegister in_reg = min_reg.AsArm();
  CHECK(in_reg.IsNoRegister() || in_reg.IsCoreRegister()) << in_reg;
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  if (null_allowed) {
    // Null values get a SIRT entry value of 0.  Otherwise, the SIRT entry is
    // the address in the SIRT holding the reference.
    // e.g. out_reg = (handle == 0) ? 0 : (SP+handle_offset)
    if (in_reg.IsNoRegister()) {
      LoadFromOffset(kLoadWord, out_reg.AsCoreRegister(),
                     SP, sirt_offset.Int32Value());
      in_reg = out_reg;
    }
    cmp(in_reg.AsCoreRegister(), ShifterOperand(0));
    if (!out_reg.Equals(in_reg)) {
      LoadImmediate(out_reg.AsCoreRegister(), 0, EQ);
    }
    AddConstant(out_reg.AsCoreRegister(), SP, sirt_offset.Int32Value(), NE);
  } else {
    AddConstant(out_reg.AsCoreRegister(), SP, sirt_offset.Int32Value(), AL);
  }
}

void ArmAssembler::CreateSirtEntry(FrameOffset out_off,
                                   FrameOffset sirt_offset,
                                   ManagedRegister mscratch,
                                   bool null_allowed) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  if (null_allowed) {
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP,
                   sirt_offset.Int32Value());
    // Null values get a SIRT entry value of 0.  Otherwise, the sirt entry is
    // the address in the SIRT holding the reference.
    // e.g. scratch = (scratch == 0) ? 0 : (SP+sirt_offset)
    cmp(scratch.AsCoreRegister(), ShifterOperand(0));
    AddConstant(scratch.AsCoreRegister(), SP, sirt_offset.Int32Value(), NE);
  } else {
    AddConstant(scratch.AsCoreRegister(), SP, sirt_offset.Int32Value(), AL);
  }
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, out_off.Int32Value());
}

void ArmAssembler::LoadReferenceFromSirt(ManagedRegister mout_reg,
                                         ManagedRegister min_reg) {
  ArmManagedRegister out_reg = mout_reg.AsArm();
  ArmManagedRegister in_reg = min_reg.AsArm();
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  CHECK(in_reg.IsCoreRegister()) << in_reg;
  Label null_arg;
  if (!out_reg.Equals(in_reg)) {
    LoadImmediate(out_reg.AsCoreRegister(), 0, EQ);
  }
  cmp(in_reg.AsCoreRegister(), ShifterOperand(0));
  LoadFromOffset(kLoadWord, out_reg.AsCoreRegister(),
                 in_reg.AsCoreRegister(), 0, NE);
}

void ArmAssembler::VerifyObject(ManagedRegister /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references
}

void ArmAssembler::VerifyObject(FrameOffset /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references
}

void ArmAssembler::Call(ManagedRegister mbase, Offset offset,
                        ManagedRegister mscratch) {
  ArmManagedRegister base = mbase.AsArm();
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(base.IsCoreRegister()) << base;
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 base.AsCoreRegister(), offset.Int32Value());
  blx(scratch.AsCoreRegister());
  // TODO: place reference map on call
}

void ArmAssembler::Call(FrameOffset base, Offset offset,
                        ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  // Call *(*(SP + base) + offset)
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 SP, base.Int32Value());
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 scratch.AsCoreRegister(), offset.Int32Value());
  blx(scratch.AsCoreRegister());
  // TODO: place reference map on call
}

void ArmAssembler::Call(ThreadOffset /*offset*/, ManagedRegister /*scratch*/) {
  UNIMPLEMENTED(FATAL);
}

void ArmAssembler::GetCurrentThread(ManagedRegister tr) {
  mov(tr.AsArm().AsCoreRegister(), ShifterOperand(TR));
}

void ArmAssembler::GetCurrentThread(FrameOffset offset,
                                    ManagedRegister /*scratch*/) {
  StoreToOffset(kStoreWord, TR, SP, offset.Int32Value(), AL);
}

void ArmAssembler::ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust) {
  ArmManagedRegister scratch = mscratch.AsArm();
  ArmExceptionSlowPath* slow = new ArmExceptionSlowPath(scratch, stack_adjust);
  buffer_.EnqueueSlowPath(slow);
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 TR, Thread::ExceptionOffset().Int32Value());
  cmp(scratch.AsCoreRegister(), ShifterOperand(0));
  b(slow->Entry(), NE);
}

void ArmExceptionSlowPath::Emit(Assembler* sasm) {
  ArmAssembler* sp_asm = down_cast<ArmAssembler*>(sasm);
#define __ sp_asm->
  __ Bind(&entry_);
  if (stack_adjust_ != 0) {  // Fix up the frame.
    __ DecreaseFrameSize(stack_adjust_);
  }
  // Pass exception object as argument
  // Don't care about preserving R0 as this call won't return
  __ mov(R0, ShifterOperand(scratch_.AsCoreRegister()));
  // Set up call to Thread::Current()->pDeliverException
  __ LoadFromOffset(kLoadWord, R12, TR, QUICK_ENTRYPOINT_OFFSET(pDeliverException).Int32Value());
  __ blx(R12);
  // Call never returns
  __ bkpt(0);
#undef __
}

}  // namespace arm
}  // namespace art
