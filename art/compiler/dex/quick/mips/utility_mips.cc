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

#include "codegen_mips.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "mips_lir.h"

namespace art {

/* This file contains codegen for the MIPS32 ISA. */
LIR* MipsMir2Lir::OpFpRegCopy(int r_dest, int r_src) {
  int opcode;
  /* must be both DOUBLE or both not DOUBLE */
  DCHECK_EQ(MIPS_DOUBLEREG(r_dest), MIPS_DOUBLEREG(r_src));
  if (MIPS_DOUBLEREG(r_dest)) {
    opcode = kMipsFmovd;
  } else {
    if (MIPS_SINGLEREG(r_dest)) {
      if (MIPS_SINGLEREG(r_src)) {
        opcode = kMipsFmovs;
      } else {
        /* note the operands are swapped for the mtc1 instr */
        int t_opnd = r_src;
        r_src = r_dest;
        r_dest = t_opnd;
        opcode = kMipsMtc1;
      }
    } else {
      DCHECK(MIPS_SINGLEREG(r_src));
      opcode = kMipsMfc1;
    }
  }
  LIR* res = RawLIR(current_dalvik_offset_, opcode, r_src, r_dest);
  if (!(cu_->disable_opt & (1 << kSafeOptimizations)) && r_dest == r_src) {
    res->flags.is_nop = true;
  }
  return res;
}

bool MipsMir2Lir::InexpensiveConstantInt(int32_t value) {
  return ((value == 0) || IsUint(16, value) || ((value < 0) && (value >= -32768)));
}

bool MipsMir2Lir::InexpensiveConstantFloat(int32_t value) {
  return false;  // TUNING
}

bool MipsMir2Lir::InexpensiveConstantLong(int64_t value) {
  return false;  // TUNING
}

bool MipsMir2Lir::InexpensiveConstantDouble(int64_t value) {
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
LIR* MipsMir2Lir::LoadConstantNoClobber(int r_dest, int value) {
  LIR *res;

  int r_dest_save = r_dest;
  int is_fp_reg = MIPS_FPREG(r_dest);
  if (is_fp_reg) {
    DCHECK(MIPS_SINGLEREG(r_dest));
    r_dest = AllocTemp();
  }

  /* See if the value can be constructed cheaply */
  if (value == 0) {
    res = NewLIR2(kMipsMove, r_dest, r_ZERO);
  } else if ((value > 0) && (value <= 65535)) {
    res = NewLIR3(kMipsOri, r_dest, r_ZERO, value);
  } else if ((value < 0) && (value >= -32768)) {
    res = NewLIR3(kMipsAddiu, r_dest, r_ZERO, value);
  } else {
    res = NewLIR2(kMipsLui, r_dest, value>>16);
    if (value & 0xffff)
      NewLIR3(kMipsOri, r_dest, r_dest, value);
  }

  if (is_fp_reg) {
    NewLIR2(kMipsMtc1, r_dest, r_dest_save);
    FreeTemp(r_dest);
  }

  return res;
}

LIR* MipsMir2Lir::OpUnconditionalBranch(LIR* target) {
  LIR* res = NewLIR1(kMipsB, 0 /* offset to be patched during assembly*/);
  res->target = target;
  return res;
}

LIR* MipsMir2Lir::OpReg(OpKind op, int r_dest_src) {
  MipsOpCode opcode = kMipsNop;
  switch (op) {
    case kOpBlx:
      opcode = kMipsJalr;
      break;
    case kOpBx:
      return NewLIR1(kMipsJr, r_dest_src);
      break;
    default:
      LOG(FATAL) << "Bad case in OpReg";
  }
  return NewLIR2(opcode, r_RA, r_dest_src);
}

LIR* MipsMir2Lir::OpRegImm(OpKind op, int r_dest_src1,
          int value) {
  LIR *res;
  bool neg = (value < 0);
  int abs_value = (neg) ? -value : value;
  bool short_form = (abs_value & 0xff) == abs_value;
  MipsOpCode opcode = kMipsNop;
  switch (op) {
    case kOpAdd:
      return OpRegRegImm(op, r_dest_src1, r_dest_src1, value);
      break;
    case kOpSub:
      return OpRegRegImm(op, r_dest_src1, r_dest_src1, value);
      break;
    default:
      LOG(FATAL) << "Bad case in OpRegImm";
      break;
  }
  if (short_form) {
    res = NewLIR2(opcode, r_dest_src1, abs_value);
  } else {
    int r_scratch = AllocTemp();
    res = LoadConstant(r_scratch, value);
    if (op == kOpCmp)
      NewLIR2(opcode, r_dest_src1, r_scratch);
    else
      NewLIR3(opcode, r_dest_src1, r_dest_src1, r_scratch);
  }
  return res;
}

LIR* MipsMir2Lir::OpRegRegReg(OpKind op, int r_dest, int r_src1, int r_src2) {
  MipsOpCode opcode = kMipsNop;
  switch (op) {
    case kOpAdd:
      opcode = kMipsAddu;
      break;
    case kOpSub:
      opcode = kMipsSubu;
      break;
    case kOpAnd:
      opcode = kMipsAnd;
      break;
    case kOpMul:
      opcode = kMipsMul;
      break;
    case kOpOr:
      opcode = kMipsOr;
      break;
    case kOpXor:
      opcode = kMipsXor;
      break;
    case kOpLsl:
      opcode = kMipsSllv;
      break;
    case kOpLsr:
      opcode = kMipsSrlv;
      break;
    case kOpAsr:
      opcode = kMipsSrav;
      break;
    case kOpAdc:
    case kOpSbc:
      LOG(FATAL) << "No carry bit on MIPS";
      break;
    default:
      LOG(FATAL) << "bad case in OpRegRegReg";
      break;
  }
  return NewLIR3(opcode, r_dest, r_src1, r_src2);
}

LIR* MipsMir2Lir::OpRegRegImm(OpKind op, int r_dest, int r_src1, int value) {
  LIR *res;
  MipsOpCode opcode = kMipsNop;
  bool short_form = true;

  switch (op) {
    case kOpAdd:
      if (IS_SIMM16(value)) {
        opcode = kMipsAddiu;
      } else {
        short_form = false;
        opcode = kMipsAddu;
      }
      break;
    case kOpSub:
      if (IS_SIMM16((-value))) {
        value = -value;
        opcode = kMipsAddiu;
      } else {
        short_form = false;
        opcode = kMipsSubu;
      }
      break;
    case kOpLsl:
        DCHECK(value >= 0 && value <= 31);
        opcode = kMipsSll;
        break;
    case kOpLsr:
        DCHECK(value >= 0 && value <= 31);
        opcode = kMipsSrl;
        break;
    case kOpAsr:
        DCHECK(value >= 0 && value <= 31);
        opcode = kMipsSra;
        break;
    case kOpAnd:
      if (IS_UIMM16((value))) {
        opcode = kMipsAndi;
      } else {
        short_form = false;
        opcode = kMipsAnd;
      }
      break;
    case kOpOr:
      if (IS_UIMM16((value))) {
        opcode = kMipsOri;
      } else {
        short_form = false;
        opcode = kMipsOr;
      }
      break;
    case kOpXor:
      if (IS_UIMM16((value))) {
        opcode = kMipsXori;
      } else {
        short_form = false;
        opcode = kMipsXor;
      }
      break;
    case kOpMul:
      short_form = false;
      opcode = kMipsMul;
      break;
    default:
      LOG(FATAL) << "Bad case in OpRegRegImm";
      break;
  }

  if (short_form) {
    res = NewLIR3(opcode, r_dest, r_src1, value);
  } else {
    if (r_dest != r_src1) {
      res = LoadConstant(r_dest, value);
      NewLIR3(opcode, r_dest, r_src1, r_dest);
    } else {
      int r_scratch = AllocTemp();
      res = LoadConstant(r_scratch, value);
      NewLIR3(opcode, r_dest, r_src1, r_scratch);
    }
  }
  return res;
}

LIR* MipsMir2Lir::OpRegReg(OpKind op, int r_dest_src1, int r_src2) {
  MipsOpCode opcode = kMipsNop;
  LIR *res;
  switch (op) {
    case kOpMov:
      opcode = kMipsMove;
      break;
    case kOpMvn:
      return NewLIR3(kMipsNor, r_dest_src1, r_src2, r_ZERO);
    case kOpNeg:
      return NewLIR3(kMipsSubu, r_dest_src1, r_ZERO, r_src2);
    case kOpAdd:
    case kOpAnd:
    case kOpMul:
    case kOpOr:
    case kOpSub:
    case kOpXor:
      return OpRegRegReg(op, r_dest_src1, r_dest_src1, r_src2);
    case kOp2Byte:
#if __mips_isa_rev >= 2
      res = NewLIR2(kMipsSeb, r_dest_src1, r_src2);
#else
      res = OpRegRegImm(kOpLsl, r_dest_src1, r_src2, 24);
      OpRegRegImm(kOpAsr, r_dest_src1, r_dest_src1, 24);
#endif
      return res;
    case kOp2Short:
#if __mips_isa_rev >= 2
      res = NewLIR2(kMipsSeh, r_dest_src1, r_src2);
#else
      res = OpRegRegImm(kOpLsl, r_dest_src1, r_src2, 16);
      OpRegRegImm(kOpAsr, r_dest_src1, r_dest_src1, 16);
#endif
      return res;
    case kOp2Char:
       return NewLIR3(kMipsAndi, r_dest_src1, r_src2, 0xFFFF);
    default:
      LOG(FATAL) << "Bad case in OpRegReg";
      break;
  }
  return NewLIR2(opcode, r_dest_src1, r_src2);
}

LIR* MipsMir2Lir::LoadConstantWide(int r_dest_lo, int r_dest_hi, int64_t value) {
  LIR *res;
  res = LoadConstantNoClobber(r_dest_lo, Low32Bits(value));
  LoadConstantNoClobber(r_dest_hi, High32Bits(value));
  return res;
}

/* Load value from base + scaled index. */
LIR* MipsMir2Lir::LoadBaseIndexed(int rBase, int r_index, int r_dest,
                                  int scale, OpSize size) {
  LIR *first = NULL;
  LIR *res;
  MipsOpCode opcode = kMipsNop;
  int t_reg = AllocTemp();

  if (MIPS_FPREG(r_dest)) {
    DCHECK(MIPS_SINGLEREG(r_dest));
    DCHECK((size == kWord) || (size == kSingle));
    size = kSingle;
  } else {
    if (size == kSingle)
      size = kWord;
  }

  if (!scale) {
    first = NewLIR3(kMipsAddu, t_reg , rBase, r_index);
  } else {
    first = OpRegRegImm(kOpLsl, t_reg, r_index, scale);
    NewLIR3(kMipsAddu, t_reg , rBase, t_reg);
  }

  switch (size) {
    case kSingle:
      opcode = kMipsFlwc1;
      break;
    case kWord:
      opcode = kMipsLw;
      break;
    case kUnsignedHalf:
      opcode = kMipsLhu;
      break;
    case kSignedHalf:
      opcode = kMipsLh;
      break;
    case kUnsignedByte:
      opcode = kMipsLbu;
      break;
    case kSignedByte:
      opcode = kMipsLb;
      break;
    default:
      LOG(FATAL) << "Bad case in LoadBaseIndexed";
  }

  res = NewLIR3(opcode, r_dest, 0, t_reg);
  FreeTemp(t_reg);
  return (first) ? first : res;
}

/* store value base base + scaled index. */
LIR* MipsMir2Lir::StoreBaseIndexed(int rBase, int r_index, int r_src,
                                   int scale, OpSize size) {
  LIR *first = NULL;
  MipsOpCode opcode = kMipsNop;
  int r_new_index = r_index;
  int t_reg = AllocTemp();

  if (MIPS_FPREG(r_src)) {
    DCHECK(MIPS_SINGLEREG(r_src));
    DCHECK((size == kWord) || (size == kSingle));
    size = kSingle;
  } else {
    if (size == kSingle)
      size = kWord;
  }

  if (!scale) {
    first = NewLIR3(kMipsAddu, t_reg , rBase, r_index);
  } else {
    first = OpRegRegImm(kOpLsl, t_reg, r_index, scale);
    NewLIR3(kMipsAddu, t_reg , rBase, t_reg);
  }

  switch (size) {
    case kSingle:
      opcode = kMipsFswc1;
      break;
    case kWord:
      opcode = kMipsSw;
      break;
    case kUnsignedHalf:
    case kSignedHalf:
      opcode = kMipsSh;
      break;
    case kUnsignedByte:
    case kSignedByte:
      opcode = kMipsSb;
      break;
    default:
      LOG(FATAL) << "Bad case in StoreBaseIndexed";
  }
  NewLIR3(opcode, r_src, 0, t_reg);
  FreeTemp(r_new_index);
  return first;
}

LIR* MipsMir2Lir::LoadBaseDispBody(int rBase, int displacement, int r_dest,
                                   int r_dest_hi, OpSize size, int s_reg) {
/*
 * Load value from base + displacement.  Optionally perform null check
 * on base (which must have an associated s_reg and MIR).  If not
 * performing null check, incoming MIR can be null. IMPORTANT: this
 * code must not allocate any new temps.  If a new register is needed
 * and base and dest are the same, spill some other register to
 * rlp and then restore.
 */
  LIR *res;
  LIR *load = NULL;
  LIR *load2 = NULL;
  MipsOpCode opcode = kMipsNop;
  bool short_form = IS_SIMM16(displacement);
  bool pair = false;

  switch (size) {
    case kLong:
    case kDouble:
      pair = true;
      opcode = kMipsLw;
      if (MIPS_FPREG(r_dest)) {
        opcode = kMipsFlwc1;
        if (MIPS_DOUBLEREG(r_dest)) {
          r_dest = r_dest - MIPS_FP_DOUBLE;
        } else {
          DCHECK(MIPS_FPREG(r_dest_hi));
          DCHECK(r_dest == (r_dest_hi - 1));
        }
        r_dest_hi = r_dest + 1;
      }
      short_form = IS_SIMM16_2WORD(displacement);
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kWord:
    case kSingle:
      opcode = kMipsLw;
      if (MIPS_FPREG(r_dest)) {
        opcode = kMipsFlwc1;
        DCHECK(MIPS_SINGLEREG(r_dest));
      }
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kUnsignedHalf:
      opcode = kMipsLhu;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kSignedHalf:
      opcode = kMipsLh;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kUnsignedByte:
      opcode = kMipsLbu;
      break;
    case kSignedByte:
      opcode = kMipsLb;
      break;
    default:
      LOG(FATAL) << "Bad case in LoadBaseIndexedBody";
  }

  if (short_form) {
    if (!pair) {
      load = res = NewLIR3(opcode, r_dest, displacement, rBase);
    } else {
      load = res = NewLIR3(opcode, r_dest,
                           displacement + LOWORD_OFFSET, rBase);
      load2 = NewLIR3(opcode, r_dest_hi,
                      displacement + HIWORD_OFFSET, rBase);
    }
  } else {
    if (pair) {
      int r_tmp = AllocFreeTemp();
      res = OpRegRegImm(kOpAdd, r_tmp, rBase, displacement);
      load = NewLIR3(opcode, r_dest, LOWORD_OFFSET, r_tmp);
      load2 = NewLIR3(opcode, r_dest_hi, HIWORD_OFFSET, r_tmp);
      FreeTemp(r_tmp);
    } else {
      int r_tmp = (rBase == r_dest) ? AllocFreeTemp() : r_dest;
      res = OpRegRegImm(kOpAdd, r_tmp, rBase, displacement);
      load = NewLIR3(opcode, r_dest, 0, r_tmp);
      if (r_tmp != r_dest)
        FreeTemp(r_tmp);
    }
  }

  if (rBase == rMIPS_SP) {
    AnnotateDalvikRegAccess(load, (displacement + (pair ? LOWORD_OFFSET : 0)) >> 2,
                            true /* is_load */, pair /* is64bit */);
    if (pair) {
      AnnotateDalvikRegAccess(load2, (displacement + HIWORD_OFFSET) >> 2,
                              true /* is_load */, pair /* is64bit */);
    }
  }
  return load;
}

LIR* MipsMir2Lir::LoadBaseDisp(int rBase, int displacement, int r_dest,
                               OpSize size, int s_reg) {
  return LoadBaseDispBody(rBase, displacement, r_dest, -1,
                          size, s_reg);
}

LIR* MipsMir2Lir::LoadBaseDispWide(int rBase, int displacement,
                                   int r_dest_lo, int r_dest_hi, int s_reg) {
  return LoadBaseDispBody(rBase, displacement, r_dest_lo, r_dest_hi, kLong, s_reg);
}

LIR* MipsMir2Lir::StoreBaseDispBody(int rBase, int displacement,
                                    int r_src, int r_src_hi, OpSize size) {
  LIR *res;
  LIR *store = NULL;
  LIR *store2 = NULL;
  MipsOpCode opcode = kMipsNop;
  bool short_form = IS_SIMM16(displacement);
  bool pair = false;

  switch (size) {
    case kLong:
    case kDouble:
      pair = true;
      opcode = kMipsSw;
      if (MIPS_FPREG(r_src)) {
        opcode = kMipsFswc1;
        if (MIPS_DOUBLEREG(r_src)) {
          r_src = r_src - MIPS_FP_DOUBLE;
        } else {
          DCHECK(MIPS_FPREG(r_src_hi));
          DCHECK_EQ(r_src, (r_src_hi - 1));
        }
        r_src_hi = r_src + 1;
      }
      short_form = IS_SIMM16_2WORD(displacement);
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kWord:
    case kSingle:
      opcode = kMipsSw;
      if (MIPS_FPREG(r_src)) {
        opcode = kMipsFswc1;
        DCHECK(MIPS_SINGLEREG(r_src));
      }
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kUnsignedHalf:
    case kSignedHalf:
      opcode = kMipsSh;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kUnsignedByte:
    case kSignedByte:
      opcode = kMipsSb;
      break;
    default:
      LOG(FATAL) << "Bad case in StoreBaseIndexedBody";
  }

  if (short_form) {
    if (!pair) {
      store = res = NewLIR3(opcode, r_src, displacement, rBase);
    } else {
      store = res = NewLIR3(opcode, r_src, displacement + LOWORD_OFFSET,
                            rBase);
      store2 = NewLIR3(opcode, r_src_hi, displacement + HIWORD_OFFSET,
                       rBase);
    }
  } else {
    int r_scratch = AllocTemp();
    res = OpRegRegImm(kOpAdd, r_scratch, rBase, displacement);
    if (!pair) {
      store =  NewLIR3(opcode, r_src, 0, r_scratch);
    } else {
      store =  NewLIR3(opcode, r_src, LOWORD_OFFSET, r_scratch);
      store2 = NewLIR3(opcode, r_src_hi, HIWORD_OFFSET, r_scratch);
    }
    FreeTemp(r_scratch);
  }

  if (rBase == rMIPS_SP) {
    AnnotateDalvikRegAccess(store, (displacement + (pair ? LOWORD_OFFSET : 0)) >> 2,
                            false /* is_load */, pair /* is64bit */);
    if (pair) {
      AnnotateDalvikRegAccess(store2, (displacement + HIWORD_OFFSET) >> 2,
                              false /* is_load */, pair /* is64bit */);
    }
  }

  return res;
}

LIR* MipsMir2Lir::StoreBaseDisp(int rBase, int displacement, int r_src,
                                OpSize size) {
  return StoreBaseDispBody(rBase, displacement, r_src, -1, size);
}

LIR* MipsMir2Lir::StoreBaseDispWide(int rBase, int displacement,
                                    int r_src_lo, int r_src_hi) {
  return StoreBaseDispBody(rBase, displacement, r_src_lo, r_src_hi, kLong);
}

LIR* MipsMir2Lir::OpThreadMem(OpKind op, ThreadOffset thread_offset) {
  LOG(FATAL) << "Unexpected use of OpThreadMem for MIPS";
  return NULL;
}

LIR* MipsMir2Lir::OpMem(OpKind op, int rBase, int disp) {
  LOG(FATAL) << "Unexpected use of OpMem for MIPS";
  return NULL;
}

LIR* MipsMir2Lir::StoreBaseIndexedDisp(int rBase, int r_index, int scale, int displacement,
                                       int r_src, int r_src_hi, OpSize size, int s_reg) {
  LOG(FATAL) << "Unexpected use of StoreBaseIndexedDisp for MIPS";
  return NULL;
}

LIR* MipsMir2Lir::OpRegMem(OpKind op, int r_dest, int rBase,
              int offset) {
  LOG(FATAL) << "Unexpected use of OpRegMem for MIPS";
  return NULL;
}

LIR* MipsMir2Lir::LoadBaseIndexedDisp(int rBase, int r_index, int scale, int displacement,
                                      int r_dest, int r_dest_hi, OpSize size, int s_reg) {
  LOG(FATAL) << "Unexpected use of LoadBaseIndexedDisp for MIPS";
  return NULL;
}

LIR* MipsMir2Lir::OpCondBranch(ConditionCode cc, LIR* target) {
  LOG(FATAL) << "Unexpected use of OpCondBranch for MIPS";
  return NULL;
}

}  // namespace art
