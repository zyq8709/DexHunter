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

#include "codegen_x86.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "x86_lir.h"

namespace art {

/* This file contains codegen for the X86 ISA */

LIR* X86Mir2Lir::OpFpRegCopy(int r_dest, int r_src) {
  int opcode;
  /* must be both DOUBLE or both not DOUBLE */
  DCHECK_EQ(X86_DOUBLEREG(r_dest), X86_DOUBLEREG(r_src));
  if (X86_DOUBLEREG(r_dest)) {
    opcode = kX86MovsdRR;
  } else {
    if (X86_SINGLEREG(r_dest)) {
      if (X86_SINGLEREG(r_src)) {
        opcode = kX86MovssRR;
      } else {  // Fpr <- Gpr
        opcode = kX86MovdxrRR;
      }
    } else {  // Gpr <- Fpr
      DCHECK(X86_SINGLEREG(r_src));
      opcode = kX86MovdrxRR;
    }
  }
  DCHECK_NE((EncodingMap[opcode].flags & IS_BINARY_OP), 0ULL);
  LIR* res = RawLIR(current_dalvik_offset_, opcode, r_dest, r_src);
  if (r_dest == r_src) {
    res->flags.is_nop = true;
  }
  return res;
}

bool X86Mir2Lir::InexpensiveConstantInt(int32_t value) {
  return true;
}

bool X86Mir2Lir::InexpensiveConstantFloat(int32_t value) {
  return false;
}

bool X86Mir2Lir::InexpensiveConstantLong(int64_t value) {
  return true;
}

bool X86Mir2Lir::InexpensiveConstantDouble(int64_t value) {
  return false;  // TUNING
}

/*
 * Load a immediate using a shortcut if possible; otherwise
 * grab from the per-translation literal pool.  If target is
 * a high register, build constant into a low register and copy.
 *
 * No additional register clobbering operation performed. Use this version when
 * 1) r_dest is freshly returned from AllocTemp or
 * 2) The codegen is under fixed register usage
 */
LIR* X86Mir2Lir::LoadConstantNoClobber(int r_dest, int value) {
  int r_dest_save = r_dest;
  if (X86_FPREG(r_dest)) {
    if (value == 0) {
      return NewLIR2(kX86XorpsRR, r_dest, r_dest);
    }
    DCHECK(X86_SINGLEREG(r_dest));
    r_dest = AllocTemp();
  }

  LIR *res;
  if (value == 0) {
    res = NewLIR2(kX86Xor32RR, r_dest, r_dest);
  } else {
    // Note, there is no byte immediate form of a 32 bit immediate move.
    res = NewLIR2(kX86Mov32RI, r_dest, value);
  }

  if (X86_FPREG(r_dest_save)) {
    NewLIR2(kX86MovdxrRR, r_dest_save, r_dest);
    FreeTemp(r_dest);
  }

  return res;
}

LIR* X86Mir2Lir::OpUnconditionalBranch(LIR* target) {
  LIR* res = NewLIR1(kX86Jmp8, 0 /* offset to be patched during assembly*/);
  res->target = target;
  return res;
}

LIR* X86Mir2Lir::OpCondBranch(ConditionCode cc, LIR* target) {
  LIR* branch = NewLIR2(kX86Jcc8, 0 /* offset to be patched */,
                        X86ConditionEncoding(cc));
  branch->target = target;
  return branch;
}

LIR* X86Mir2Lir::OpReg(OpKind op, int r_dest_src) {
  X86OpCode opcode = kX86Bkpt;
  switch (op) {
    case kOpNeg: opcode = kX86Neg32R; break;
    case kOpNot: opcode = kX86Not32R; break;
    case kOpBlx: opcode = kX86CallR; break;
    default:
      LOG(FATAL) << "Bad case in OpReg " << op;
  }
  return NewLIR1(opcode, r_dest_src);
}

LIR* X86Mir2Lir::OpRegImm(OpKind op, int r_dest_src1, int value) {
  X86OpCode opcode = kX86Bkpt;
  bool byte_imm = IS_SIMM8(value);
  DCHECK(!X86_FPREG(r_dest_src1));
  switch (op) {
    case kOpLsl: opcode = kX86Sal32RI; break;
    case kOpLsr: opcode = kX86Shr32RI; break;
    case kOpAsr: opcode = kX86Sar32RI; break;
    case kOpAdd: opcode = byte_imm ? kX86Add32RI8 : kX86Add32RI; break;
    case kOpOr:  opcode = byte_imm ? kX86Or32RI8  : kX86Or32RI;  break;
    case kOpAdc: opcode = byte_imm ? kX86Adc32RI8 : kX86Adc32RI; break;
    // case kOpSbb: opcode = kX86Sbb32RI; break;
    case kOpAnd: opcode = byte_imm ? kX86And32RI8 : kX86And32RI; break;
    case kOpSub: opcode = byte_imm ? kX86Sub32RI8 : kX86Sub32RI; break;
    case kOpXor: opcode = byte_imm ? kX86Xor32RI8 : kX86Xor32RI; break;
    case kOpCmp: opcode = byte_imm ? kX86Cmp32RI8 : kX86Cmp32RI; break;
    case kOpMov: return LoadConstantNoClobber(r_dest_src1, value);
    case kOpMul:
      opcode = byte_imm ? kX86Imul32RRI8 : kX86Imul32RRI;
      return NewLIR3(opcode, r_dest_src1, r_dest_src1, value);
    default:
      LOG(FATAL) << "Bad case in OpRegImm " << op;
  }
  return NewLIR2(opcode, r_dest_src1, value);
}

LIR* X86Mir2Lir::OpRegReg(OpKind op, int r_dest_src1, int r_src2) {
    X86OpCode opcode = kX86Nop;
    bool src2_must_be_cx = false;
    switch (op) {
        // X86 unary opcodes
      case kOpMvn:
        OpRegCopy(r_dest_src1, r_src2);
        return OpReg(kOpNot, r_dest_src1);
      case kOpNeg:
        OpRegCopy(r_dest_src1, r_src2);
        return OpReg(kOpNeg, r_dest_src1);
        // X86 binary opcodes
      case kOpSub: opcode = kX86Sub32RR; break;
      case kOpSbc: opcode = kX86Sbb32RR; break;
      case kOpLsl: opcode = kX86Sal32RC; src2_must_be_cx = true; break;
      case kOpLsr: opcode = kX86Shr32RC; src2_must_be_cx = true; break;
      case kOpAsr: opcode = kX86Sar32RC; src2_must_be_cx = true; break;
      case kOpMov: opcode = kX86Mov32RR; break;
      case kOpCmp: opcode = kX86Cmp32RR; break;
      case kOpAdd: opcode = kX86Add32RR; break;
      case kOpAdc: opcode = kX86Adc32RR; break;
      case kOpAnd: opcode = kX86And32RR; break;
      case kOpOr:  opcode = kX86Or32RR; break;
      case kOpXor: opcode = kX86Xor32RR; break;
      case kOp2Byte:
        // Use shifts instead of a byte operand if the source can't be byte accessed.
        if (r_src2 >= 4) {
          NewLIR2(kX86Mov32RR, r_dest_src1, r_src2);
          NewLIR2(kX86Sal32RI, r_dest_src1, 24);
          return NewLIR2(kX86Sar32RI, r_dest_src1, 24);
        } else {
          opcode = kX86Movsx8RR;
        }
        break;
      case kOp2Short: opcode = kX86Movsx16RR; break;
      case kOp2Char: opcode = kX86Movzx16RR; break;
      case kOpMul: opcode = kX86Imul32RR; break;
      default:
        LOG(FATAL) << "Bad case in OpRegReg " << op;
        break;
    }
    CHECK(!src2_must_be_cx || r_src2 == rCX);
    return NewLIR2(opcode, r_dest_src1, r_src2);
}

LIR* X86Mir2Lir::OpRegMem(OpKind op, int r_dest, int rBase,
              int offset) {
  X86OpCode opcode = kX86Nop;
  switch (op) {
      // X86 binary opcodes
    case kOpSub: opcode = kX86Sub32RM; break;
    case kOpMov: opcode = kX86Mov32RM; break;
    case kOpCmp: opcode = kX86Cmp32RM; break;
    case kOpAdd: opcode = kX86Add32RM; break;
    case kOpAnd: opcode = kX86And32RM; break;
    case kOpOr:  opcode = kX86Or32RM; break;
    case kOpXor: opcode = kX86Xor32RM; break;
    case kOp2Byte: opcode = kX86Movsx8RM; break;
    case kOp2Short: opcode = kX86Movsx16RM; break;
    case kOp2Char: opcode = kX86Movzx16RM; break;
    case kOpMul:
    default:
      LOG(FATAL) << "Bad case in OpRegMem " << op;
      break;
  }
  return NewLIR3(opcode, r_dest, rBase, offset);
}

LIR* X86Mir2Lir::OpRegRegReg(OpKind op, int r_dest, int r_src1,
                 int r_src2) {
  if (r_dest != r_src1 && r_dest != r_src2) {
    if (op == kOpAdd) {  // lea special case, except can't encode rbp as base
      if (r_src1 == r_src2) {
        OpRegCopy(r_dest, r_src1);
        return OpRegImm(kOpLsl, r_dest, 1);
      } else if (r_src1 != rBP) {
        return NewLIR5(kX86Lea32RA, r_dest, r_src1 /* base */,
                       r_src2 /* index */, 0 /* scale */, 0 /* disp */);
      } else {
        return NewLIR5(kX86Lea32RA, r_dest, r_src2 /* base */,
                       r_src1 /* index */, 0 /* scale */, 0 /* disp */);
      }
    } else {
      OpRegCopy(r_dest, r_src1);
      return OpRegReg(op, r_dest, r_src2);
    }
  } else if (r_dest == r_src1) {
    return OpRegReg(op, r_dest, r_src2);
  } else {  // r_dest == r_src2
    switch (op) {
      case kOpSub:  // non-commutative
        OpReg(kOpNeg, r_dest);
        op = kOpAdd;
        break;
      case kOpSbc:
      case kOpLsl: case kOpLsr: case kOpAsr: case kOpRor: {
        int t_reg = AllocTemp();
        OpRegCopy(t_reg, r_src1);
        OpRegReg(op, t_reg, r_src2);
        LIR* res = OpRegCopy(r_dest, t_reg);
        FreeTemp(t_reg);
        return res;
      }
      case kOpAdd:  // commutative
      case kOpOr:
      case kOpAdc:
      case kOpAnd:
      case kOpXor:
        break;
      default:
        LOG(FATAL) << "Bad case in OpRegRegReg " << op;
    }
    return OpRegReg(op, r_dest, r_src1);
  }
}

LIR* X86Mir2Lir::OpRegRegImm(OpKind op, int r_dest, int r_src,
                 int value) {
  if (op == kOpMul) {
    X86OpCode opcode = IS_SIMM8(value) ? kX86Imul32RRI8 : kX86Imul32RRI;
    return NewLIR3(opcode, r_dest, r_src, value);
  } else if (op == kOpAnd) {
    if (value == 0xFF && r_src < 4) {
      return NewLIR2(kX86Movzx8RR, r_dest, r_src);
    } else if (value == 0xFFFF) {
      return NewLIR2(kX86Movzx16RR, r_dest, r_src);
    }
  }
  if (r_dest != r_src) {
    if (false && op == kOpLsl && value >= 0 && value <= 3) {  // lea shift special case
      // TODO: fix bug in LEA encoding when disp == 0
      return NewLIR5(kX86Lea32RA, r_dest,  r5sib_no_base /* base */,
                     r_src /* index */, value /* scale */, 0 /* disp */);
    } else if (op == kOpAdd) {  // lea add special case
      return NewLIR5(kX86Lea32RA, r_dest, r_src /* base */,
                     r4sib_no_index /* index */, 0 /* scale */, value /* disp */);
    }
    OpRegCopy(r_dest, r_src);
  }
  return OpRegImm(op, r_dest, value);
}

LIR* X86Mir2Lir::OpThreadMem(OpKind op, ThreadOffset thread_offset) {
  X86OpCode opcode = kX86Bkpt;
  switch (op) {
    case kOpBlx: opcode = kX86CallT;  break;
    case kOpBx: opcode = kX86JmpT;  break;
    default:
      LOG(FATAL) << "Bad opcode: " << op;
      break;
  }
  return NewLIR1(opcode, thread_offset.Int32Value());
}

LIR* X86Mir2Lir::OpMem(OpKind op, int rBase, int disp) {
  X86OpCode opcode = kX86Bkpt;
  switch (op) {
    case kOpBlx: opcode = kX86CallM;  break;
    default:
      LOG(FATAL) << "Bad opcode: " << op;
      break;
  }
  return NewLIR2(opcode, rBase, disp);
}

LIR* X86Mir2Lir::LoadConstantWide(int r_dest_lo, int r_dest_hi, int64_t value) {
    int32_t val_lo = Low32Bits(value);
    int32_t val_hi = High32Bits(value);
    LIR *res;
    if (X86_FPREG(r_dest_lo)) {
      DCHECK(X86_FPREG(r_dest_hi));  // ignore r_dest_hi
      if (value == 0) {
        return NewLIR2(kX86XorpsRR, r_dest_lo, r_dest_lo);
      } else {
        if (val_lo == 0) {
          res = NewLIR2(kX86XorpsRR, r_dest_lo, r_dest_lo);
        } else {
          res = LoadConstantNoClobber(r_dest_lo, val_lo);
        }
        if (val_hi != 0) {
          LoadConstantNoClobber(r_dest_hi, val_hi);
          NewLIR2(kX86PsllqRI, r_dest_hi, 32);
          NewLIR2(kX86OrpsRR, r_dest_lo, r_dest_hi);
        }
      }
    } else {
      res = LoadConstantNoClobber(r_dest_lo, val_lo);
      LoadConstantNoClobber(r_dest_hi, val_hi);
    }
    return res;
}

LIR* X86Mir2Lir::LoadBaseIndexedDisp(int rBase, int r_index, int scale,
                                     int displacement, int r_dest, int r_dest_hi, OpSize size,
                                     int s_reg) {
  LIR *load = NULL;
  LIR *load2 = NULL;
  bool is_array = r_index != INVALID_REG;
  bool pair = false;
  bool is64bit = false;
  X86OpCode opcode = kX86Nop;
  switch (size) {
    case kLong:
    case kDouble:
      is64bit = true;
      if (X86_FPREG(r_dest)) {
        opcode = is_array ? kX86MovsdRA : kX86MovsdRM;
        if (X86_SINGLEREG(r_dest)) {
          DCHECK(X86_FPREG(r_dest_hi));
          DCHECK_EQ(r_dest, (r_dest_hi - 1));
          r_dest = S2d(r_dest, r_dest_hi);
        }
        r_dest_hi = r_dest + 1;
      } else {
        pair = true;
        opcode = is_array ? kX86Mov32RA  : kX86Mov32RM;
      }
      // TODO: double store is to unaligned address
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kWord:
    case kSingle:
      opcode = is_array ? kX86Mov32RA : kX86Mov32RM;
      if (X86_FPREG(r_dest)) {
        opcode = is_array ? kX86MovssRA : kX86MovssRM;
        DCHECK(X86_SINGLEREG(r_dest));
      }
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kUnsignedHalf:
      opcode = is_array ? kX86Movzx16RA : kX86Movzx16RM;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kSignedHalf:
      opcode = is_array ? kX86Movsx16RA : kX86Movsx16RM;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kUnsignedByte:
      opcode = is_array ? kX86Movzx8RA : kX86Movzx8RM;
      break;
    case kSignedByte:
      opcode = is_array ? kX86Movsx8RA : kX86Movsx8RM;
      break;
    default:
      LOG(FATAL) << "Bad case in LoadBaseIndexedDispBody";
  }

  if (!is_array) {
    if (!pair) {
      load = NewLIR3(opcode, r_dest, rBase, displacement + LOWORD_OFFSET);
    } else {
      if (rBase == r_dest) {
        load2 = NewLIR3(opcode, r_dest_hi, rBase,
                        displacement + HIWORD_OFFSET);
        load = NewLIR3(opcode, r_dest, rBase, displacement + LOWORD_OFFSET);
      } else {
        load = NewLIR3(opcode, r_dest, rBase, displacement + LOWORD_OFFSET);
        load2 = NewLIR3(opcode, r_dest_hi, rBase,
                        displacement + HIWORD_OFFSET);
      }
    }
    if (rBase == rX86_SP) {
      AnnotateDalvikRegAccess(load, (displacement + (pair ? LOWORD_OFFSET : 0)) >> 2,
                              true /* is_load */, is64bit);
      if (pair) {
        AnnotateDalvikRegAccess(load2, (displacement + HIWORD_OFFSET) >> 2,
                                true /* is_load */, is64bit);
      }
    }
  } else {
    if (!pair) {
      load = NewLIR5(opcode, r_dest, rBase, r_index, scale,
                     displacement + LOWORD_OFFSET);
    } else {
      if (rBase == r_dest) {
        load2 = NewLIR5(opcode, r_dest_hi, rBase, r_index, scale,
                        displacement + HIWORD_OFFSET);
        load = NewLIR5(opcode, r_dest, rBase, r_index, scale,
                       displacement + LOWORD_OFFSET);
      } else {
        load = NewLIR5(opcode, r_dest, rBase, r_index, scale,
                       displacement + LOWORD_OFFSET);
        load2 = NewLIR5(opcode, r_dest_hi, rBase, r_index, scale,
                        displacement + HIWORD_OFFSET);
      }
    }
  }

  return load;
}

/* Load value from base + scaled index. */
LIR* X86Mir2Lir::LoadBaseIndexed(int rBase,
                     int r_index, int r_dest, int scale, OpSize size) {
  return LoadBaseIndexedDisp(rBase, r_index, scale, 0,
                             r_dest, INVALID_REG, size, INVALID_SREG);
}

LIR* X86Mir2Lir::LoadBaseDisp(int rBase, int displacement,
                  int r_dest, OpSize size, int s_reg) {
  return LoadBaseIndexedDisp(rBase, INVALID_REG, 0, displacement,
                             r_dest, INVALID_REG, size, s_reg);
}

LIR* X86Mir2Lir::LoadBaseDispWide(int rBase, int displacement,
                      int r_dest_lo, int r_dest_hi, int s_reg) {
  return LoadBaseIndexedDisp(rBase, INVALID_REG, 0, displacement,
                             r_dest_lo, r_dest_hi, kLong, s_reg);
}

LIR* X86Mir2Lir::StoreBaseIndexedDisp(int rBase, int r_index, int scale,
                                      int displacement, int r_src, int r_src_hi, OpSize size,
                                      int s_reg) {
  LIR *store = NULL;
  LIR *store2 = NULL;
  bool is_array = r_index != INVALID_REG;
  bool pair = false;
  bool is64bit = false;
  X86OpCode opcode = kX86Nop;
  switch (size) {
    case kLong:
    case kDouble:
      is64bit = true;
      if (X86_FPREG(r_src)) {
        opcode = is_array ? kX86MovsdAR : kX86MovsdMR;
        if (X86_SINGLEREG(r_src)) {
          DCHECK(X86_FPREG(r_src_hi));
          DCHECK_EQ(r_src, (r_src_hi - 1));
          r_src = S2d(r_src, r_src_hi);
        }
        r_src_hi = r_src + 1;
      } else {
        pair = true;
        opcode = is_array ? kX86Mov32AR  : kX86Mov32MR;
      }
      // TODO: double store is to unaligned address
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kWord:
    case kSingle:
      opcode = is_array ? kX86Mov32AR : kX86Mov32MR;
      if (X86_FPREG(r_src)) {
        opcode = is_array ? kX86MovssAR : kX86MovssMR;
        DCHECK(X86_SINGLEREG(r_src));
      }
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kUnsignedHalf:
    case kSignedHalf:
      opcode = is_array ? kX86Mov16AR : kX86Mov16MR;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kUnsignedByte:
    case kSignedByte:
      opcode = is_array ? kX86Mov8AR : kX86Mov8MR;
      break;
    default:
      LOG(FATAL) << "Bad case in LoadBaseIndexedDispBody";
  }

  if (!is_array) {
    if (!pair) {
      store = NewLIR3(opcode, rBase, displacement + LOWORD_OFFSET, r_src);
    } else {
      store = NewLIR3(opcode, rBase, displacement + LOWORD_OFFSET, r_src);
      store2 = NewLIR3(opcode, rBase, displacement + HIWORD_OFFSET, r_src_hi);
    }
    if (rBase == rX86_SP) {
      AnnotateDalvikRegAccess(store, (displacement + (pair ? LOWORD_OFFSET : 0)) >> 2,
                              false /* is_load */, is64bit);
      if (pair) {
        AnnotateDalvikRegAccess(store2, (displacement + HIWORD_OFFSET) >> 2,
                                false /* is_load */, is64bit);
      }
    }
  } else {
    if (!pair) {
      store = NewLIR5(opcode, rBase, r_index, scale,
                      displacement + LOWORD_OFFSET, r_src);
    } else {
      store = NewLIR5(opcode, rBase, r_index, scale,
                      displacement + LOWORD_OFFSET, r_src);
      store2 = NewLIR5(opcode, rBase, r_index, scale,
                       displacement + HIWORD_OFFSET, r_src_hi);
    }
  }

  return store;
}

/* store value base base + scaled index. */
LIR* X86Mir2Lir::StoreBaseIndexed(int rBase, int r_index, int r_src,
                      int scale, OpSize size) {
  return StoreBaseIndexedDisp(rBase, r_index, scale, 0,
                              r_src, INVALID_REG, size, INVALID_SREG);
}

LIR* X86Mir2Lir::StoreBaseDisp(int rBase, int displacement,
                               int r_src, OpSize size) {
    return StoreBaseIndexedDisp(rBase, INVALID_REG, 0,
                                displacement, r_src, INVALID_REG, size,
                                INVALID_SREG);
}

LIR* X86Mir2Lir::StoreBaseDispWide(int rBase, int displacement,
                                   int r_src_lo, int r_src_hi) {
  return StoreBaseIndexedDisp(rBase, INVALID_REG, 0, displacement,
                              r_src_lo, r_src_hi, kLong, INVALID_SREG);
}

}  // namespace art
