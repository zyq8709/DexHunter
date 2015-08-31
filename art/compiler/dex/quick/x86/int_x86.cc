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

/* This file contains codegen for the X86 ISA */

#include "codegen_x86.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "mirror/array.h"
#include "x86_lir.h"

namespace art {

/*
 * Perform register memory operation.
 */
LIR* X86Mir2Lir::GenRegMemCheck(ConditionCode c_code,
                                int reg1, int base, int offset, ThrowKind kind) {
  LIR* tgt = RawLIR(0, kPseudoThrowTarget, kind,
                    current_dalvik_offset_, reg1, base, offset);
  OpRegMem(kOpCmp, reg1, base, offset);
  LIR* branch = OpCondBranch(c_code, tgt);
  // Remember branch target - will process later
  throw_launchpads_.Insert(tgt);
  return branch;
}

/*
 * Compare two 64-bit values
 *    x = y     return  0
 *    x < y     return -1
 *    x > y     return  1
 */
void X86Mir2Lir::GenCmpLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  LoadValueDirectWideFixed(rl_src1, r0, r1);
  LoadValueDirectWideFixed(rl_src2, r2, r3);
  // Compute (r1:r0) = (r1:r0) - (r3:r2)
  OpRegReg(kOpSub, r0, r2);  // r0 = r0 - r2
  OpRegReg(kOpSbc, r1, r3);  // r1 = r1 - r3 - CF
  NewLIR2(kX86Set8R, r2, kX86CondL);  // r2 = (r1:r0) < (r3:r2) ? 1 : 0
  NewLIR2(kX86Movzx8RR, r2, r2);
  OpReg(kOpNeg, r2);         // r2 = -r2
  OpRegReg(kOpOr, r0, r1);   // r0 = high | low - sets ZF
  NewLIR2(kX86Set8R, r0, kX86CondNz);  // r0 = (r1:r0) != (r3:r2) ? 1 : 0
  NewLIR2(kX86Movzx8RR, r0, r0);
  OpRegReg(kOpOr, r0, r2);   // r0 = r0 | r2
  RegLocation rl_result = LocCReturn();
  StoreValue(rl_dest, rl_result);
}

X86ConditionCode X86ConditionEncoding(ConditionCode cond) {
  switch (cond) {
    case kCondEq: return kX86CondEq;
    case kCondNe: return kX86CondNe;
    case kCondCs: return kX86CondC;
    case kCondCc: return kX86CondNc;
    case kCondMi: return kX86CondS;
    case kCondPl: return kX86CondNs;
    case kCondVs: return kX86CondO;
    case kCondVc: return kX86CondNo;
    case kCondHi: return kX86CondA;
    case kCondLs: return kX86CondBe;
    case kCondGe: return kX86CondGe;
    case kCondLt: return kX86CondL;
    case kCondGt: return kX86CondG;
    case kCondLe: return kX86CondLe;
    case kCondAl:
    case kCondNv: LOG(FATAL) << "Should not reach here";
  }
  return kX86CondO;
}

LIR* X86Mir2Lir::OpCmpBranch(ConditionCode cond, int src1, int src2,
                             LIR* target) {
  NewLIR2(kX86Cmp32RR, src1, src2);
  X86ConditionCode cc = X86ConditionEncoding(cond);
  LIR* branch = NewLIR2(kX86Jcc8, 0 /* lir operand for Jcc offset */ ,
                        cc);
  branch->target = target;
  return branch;
}

LIR* X86Mir2Lir::OpCmpImmBranch(ConditionCode cond, int reg,
                                int check_value, LIR* target) {
  if ((check_value == 0) && (cond == kCondEq || cond == kCondNe)) {
    // TODO: when check_value == 0 and reg is rCX, use the jcxz/nz opcode
    NewLIR2(kX86Test32RR, reg, reg);
  } else {
    NewLIR2(IS_SIMM8(check_value) ? kX86Cmp32RI8 : kX86Cmp32RI, reg, check_value);
  }
  X86ConditionCode cc = X86ConditionEncoding(cond);
  LIR* branch = NewLIR2(kX86Jcc8, 0 /* lir operand for Jcc offset */ , cc);
  branch->target = target;
  return branch;
}

LIR* X86Mir2Lir::OpRegCopyNoInsert(int r_dest, int r_src) {
  if (X86_FPREG(r_dest) || X86_FPREG(r_src))
    return OpFpRegCopy(r_dest, r_src);
  LIR* res = RawLIR(current_dalvik_offset_, kX86Mov32RR,
                    r_dest, r_src);
  if (r_dest == r_src) {
    res->flags.is_nop = true;
  }
  return res;
}

LIR* X86Mir2Lir::OpRegCopy(int r_dest, int r_src) {
  LIR *res = OpRegCopyNoInsert(r_dest, r_src);
  AppendLIR(res);
  return res;
}

void X86Mir2Lir::OpRegCopyWide(int dest_lo, int dest_hi,
                               int src_lo, int src_hi) {
  bool dest_fp = X86_FPREG(dest_lo) && X86_FPREG(dest_hi);
  bool src_fp = X86_FPREG(src_lo) && X86_FPREG(src_hi);
  assert(X86_FPREG(src_lo) == X86_FPREG(src_hi));
  assert(X86_FPREG(dest_lo) == X86_FPREG(dest_hi));
  if (dest_fp) {
    if (src_fp) {
      OpRegCopy(S2d(dest_lo, dest_hi), S2d(src_lo, src_hi));
    } else {
      // TODO: Prevent this from happening in the code. The result is often
      // unused or could have been loaded more easily from memory.
      NewLIR2(kX86MovdxrRR, dest_lo, src_lo);
      NewLIR2(kX86MovdxrRR, dest_hi, src_hi);
      NewLIR2(kX86PsllqRI, dest_hi, 32);
      NewLIR2(kX86OrpsRR, dest_lo, dest_hi);
    }
  } else {
    if (src_fp) {
      NewLIR2(kX86MovdrxRR, dest_lo, src_lo);
      NewLIR2(kX86PsrlqRI, src_lo, 32);
      NewLIR2(kX86MovdrxRR, dest_hi, src_lo);
    } else {
      // Handle overlap
      if (src_hi == dest_lo) {
        OpRegCopy(dest_hi, src_hi);
        OpRegCopy(dest_lo, src_lo);
      } else {
        OpRegCopy(dest_lo, src_lo);
        OpRegCopy(dest_hi, src_hi);
      }
    }
  }
}

void X86Mir2Lir::GenSelect(BasicBlock* bb, MIR* mir) {
  UNIMPLEMENTED(FATAL) << "Need codegen for GenSelect";
}

void X86Mir2Lir::GenFusedLongCmpBranch(BasicBlock* bb, MIR* mir) {
  LIR* taken = &block_label_list_[bb->taken->id];
  RegLocation rl_src1 = mir_graph_->GetSrcWide(mir, 0);
  RegLocation rl_src2 = mir_graph_->GetSrcWide(mir, 2);
  FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  LoadValueDirectWideFixed(rl_src1, r0, r1);
  LoadValueDirectWideFixed(rl_src2, r2, r3);
  ConditionCode ccode = static_cast<ConditionCode>(mir->dalvikInsn.arg[0]);
  // Swap operands and condition code to prevent use of zero flag.
  if (ccode == kCondLe || ccode == kCondGt) {
    // Compute (r3:r2) = (r3:r2) - (r1:r0)
    OpRegReg(kOpSub, r2, r0);  // r2 = r2 - r0
    OpRegReg(kOpSbc, r3, r1);  // r3 = r3 - r1 - CF
  } else {
    // Compute (r1:r0) = (r1:r0) - (r3:r2)
    OpRegReg(kOpSub, r0, r2);  // r0 = r0 - r2
    OpRegReg(kOpSbc, r1, r3);  // r1 = r1 - r3 - CF
  }
  switch (ccode) {
    case kCondEq:
    case kCondNe:
      OpRegReg(kOpOr, r0, r1);  // r0 = r0 | r1
      break;
    case kCondLe:
      ccode = kCondGe;
      break;
    case kCondGt:
      ccode = kCondLt;
      break;
    case kCondLt:
    case kCondGe:
      break;
    default:
      LOG(FATAL) << "Unexpected ccode: " << ccode;
  }
  OpCondBranch(ccode, taken);
}

RegLocation X86Mir2Lir::GenDivRemLit(RegLocation rl_dest, int reg_lo,
                                     int lit, bool is_div) {
  LOG(FATAL) << "Unexpected use of GenDivRemLit for x86";
  return rl_dest;
}

RegLocation X86Mir2Lir::GenDivRem(RegLocation rl_dest, int reg_lo,
                                  int reg_hi, bool is_div) {
  LOG(FATAL) << "Unexpected use of GenDivRem for x86";
  return rl_dest;
}

bool X86Mir2Lir::GenInlinedMinMaxInt(CallInfo* info, bool is_min) {
  DCHECK_EQ(cu_->instruction_set, kX86);
  RegLocation rl_src1 = info->args[0];
  RegLocation rl_src2 = info->args[1];
  rl_src1 = LoadValue(rl_src1, kCoreReg);
  rl_src2 = LoadValue(rl_src2, kCoreReg);
  RegLocation rl_dest = InlineTarget(info);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  OpRegReg(kOpCmp, rl_src1.low_reg, rl_src2.low_reg);
  DCHECK_EQ(cu_->instruction_set, kX86);
  LIR* branch = NewLIR2(kX86Jcc8, 0, is_min ? kX86CondG : kX86CondL);
  OpRegReg(kOpMov, rl_result.low_reg, rl_src1.low_reg);
  LIR* branch2 = NewLIR1(kX86Jmp8, 0);
  branch->target = NewLIR0(kPseudoTargetLabel);
  OpRegReg(kOpMov, rl_result.low_reg, rl_src2.low_reg);
  branch2->target = NewLIR0(kPseudoTargetLabel);
  StoreValue(rl_dest, rl_result);
  return true;
}

void X86Mir2Lir::OpLea(int rBase, int reg1, int reg2, int scale, int offset) {
  NewLIR5(kX86Lea32RA, rBase, reg1, reg2, scale, offset);
}

void X86Mir2Lir::OpTlsCmp(ThreadOffset offset, int val) {
  NewLIR2(kX86Cmp16TI8, offset.Int32Value(), val);
}

bool X86Mir2Lir::GenInlinedCas32(CallInfo* info, bool need_write_barrier) {
  DCHECK_NE(cu_->instruction_set, kThumb2);
  return false;
}

LIR* X86Mir2Lir::OpPcRelLoad(int reg, LIR* target) {
  LOG(FATAL) << "Unexpected use of OpPcRelLoad for x86";
  return NULL;
}

LIR* X86Mir2Lir::OpVldm(int rBase, int count) {
  LOG(FATAL) << "Unexpected use of OpVldm for x86";
  return NULL;
}

LIR* X86Mir2Lir::OpVstm(int rBase, int count) {
  LOG(FATAL) << "Unexpected use of OpVstm for x86";
  return NULL;
}

void X86Mir2Lir::GenMultiplyByTwoBitMultiplier(RegLocation rl_src,
                                               RegLocation rl_result, int lit,
                                               int first_bit, int second_bit) {
  int t_reg = AllocTemp();
  OpRegRegImm(kOpLsl, t_reg, rl_src.low_reg, second_bit - first_bit);
  OpRegRegReg(kOpAdd, rl_result.low_reg, rl_src.low_reg, t_reg);
  FreeTemp(t_reg);
  if (first_bit != 0) {
    OpRegRegImm(kOpLsl, rl_result.low_reg, rl_result.low_reg, first_bit);
  }
}

void X86Mir2Lir::GenDivZeroCheck(int reg_lo, int reg_hi) {
  int t_reg = AllocTemp();
  OpRegRegReg(kOpOr, t_reg, reg_lo, reg_hi);
  GenImmedCheck(kCondEq, t_reg, 0, kThrowDivZero);
  FreeTemp(t_reg);
}

// Test suspend flag, return target of taken suspend branch
LIR* X86Mir2Lir::OpTestSuspend(LIR* target) {
  OpTlsCmp(Thread::ThreadFlagsOffset(), 0);
  return OpCondBranch((target == NULL) ? kCondNe : kCondEq, target);
}

// Decrement register and branch on condition
LIR* X86Mir2Lir::OpDecAndBranch(ConditionCode c_code, int reg, LIR* target) {
  OpRegImm(kOpSub, reg, 1);
  return OpCmpImmBranch(c_code, reg, 0, target);
}

bool X86Mir2Lir::SmallLiteralDivRem(Instruction::Code dalvik_opcode, bool is_div,
                                    RegLocation rl_src, RegLocation rl_dest, int lit) {
  LOG(FATAL) << "Unexpected use of smallLiteralDive in x86";
  return false;
}

LIR* X86Mir2Lir::OpIT(ConditionCode cond, const char* guide) {
  LOG(FATAL) << "Unexpected use of OpIT in x86";
  return NULL;
}

void X86Mir2Lir::GenMulLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  LOG(FATAL) << "Unexpected use of GenX86Long for x86";
}
void X86Mir2Lir::GenAddLong(RegLocation rl_dest, RegLocation rl_src1,
                         RegLocation rl_src2) {
  // TODO: fixed register usage here as we only have 4 temps and temporary allocation isn't smart
  // enough.
  FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  LoadValueDirectWideFixed(rl_src1, r0, r1);
  LoadValueDirectWideFixed(rl_src2, r2, r3);
  // Compute (r1:r0) = (r1:r0) + (r2:r3)
  OpRegReg(kOpAdd, r0, r2);  // r0 = r0 + r2
  OpRegReg(kOpAdc, r1, r3);  // r1 = r1 + r3 + CF
  RegLocation rl_result = {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r0, r1,
                          INVALID_SREG, INVALID_SREG};
  StoreValueWide(rl_dest, rl_result);
}

void X86Mir2Lir::GenSubLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  // TODO: fixed register usage here as we only have 4 temps and temporary allocation isn't smart
  // enough.
  FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  LoadValueDirectWideFixed(rl_src1, r0, r1);
  LoadValueDirectWideFixed(rl_src2, r2, r3);
  // Compute (r1:r0) = (r1:r0) + (r2:r3)
  OpRegReg(kOpSub, r0, r2);  // r0 = r0 - r2
  OpRegReg(kOpSbc, r1, r3);  // r1 = r1 - r3 - CF
  RegLocation rl_result = {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r0, r1,
                          INVALID_SREG, INVALID_SREG};
  StoreValueWide(rl_dest, rl_result);
}

void X86Mir2Lir::GenAndLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  // TODO: fixed register usage here as we only have 4 temps and temporary allocation isn't smart
  // enough.
  FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  LoadValueDirectWideFixed(rl_src1, r0, r1);
  LoadValueDirectWideFixed(rl_src2, r2, r3);
  // Compute (r1:r0) = (r1:r0) & (r2:r3)
  OpRegReg(kOpAnd, r0, r2);  // r0 = r0 & r2
  OpRegReg(kOpAnd, r1, r3);  // r1 = r1 & r3
  RegLocation rl_result = {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r0, r1,
                          INVALID_SREG, INVALID_SREG};
  StoreValueWide(rl_dest, rl_result);
}

void X86Mir2Lir::GenOrLong(RegLocation rl_dest,
                           RegLocation rl_src1, RegLocation rl_src2) {
  // TODO: fixed register usage here as we only have 4 temps and temporary allocation isn't smart
  // enough.
  FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  LoadValueDirectWideFixed(rl_src1, r0, r1);
  LoadValueDirectWideFixed(rl_src2, r2, r3);
  // Compute (r1:r0) = (r1:r0) | (r2:r3)
  OpRegReg(kOpOr, r0, r2);  // r0 = r0 | r2
  OpRegReg(kOpOr, r1, r3);  // r1 = r1 | r3
  RegLocation rl_result = {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r0, r1,
                          INVALID_SREG, INVALID_SREG};
  StoreValueWide(rl_dest, rl_result);
}

void X86Mir2Lir::GenXorLong(RegLocation rl_dest,
                            RegLocation rl_src1, RegLocation rl_src2) {
  // TODO: fixed register usage here as we only have 4 temps and temporary allocation isn't smart
  // enough.
  FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  LoadValueDirectWideFixed(rl_src1, r0, r1);
  LoadValueDirectWideFixed(rl_src2, r2, r3);
  // Compute (r1:r0) = (r1:r0) ^ (r2:r3)
  OpRegReg(kOpXor, r0, r2);  // r0 = r0 ^ r2
  OpRegReg(kOpXor, r1, r3);  // r1 = r1 ^ r3
  RegLocation rl_result = {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r0, r1,
                          INVALID_SREG, INVALID_SREG};
  StoreValueWide(rl_dest, rl_result);
}

void X86Mir2Lir::GenNegLong(RegLocation rl_dest, RegLocation rl_src) {
  FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  LoadValueDirectWideFixed(rl_src, r0, r1);
  // Compute (r1:r0) = -(r1:r0)
  OpRegReg(kOpNeg, r0, r0);  // r0 = -r0
  OpRegImm(kOpAdc, r1, 0);   // r1 = r1 + CF
  OpRegReg(kOpNeg, r1, r1);  // r1 = -r1
  RegLocation rl_result = {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, r0, r1,
                          INVALID_SREG, INVALID_SREG};
  StoreValueWide(rl_dest, rl_result);
}

void X86Mir2Lir::OpRegThreadMem(OpKind op, int r_dest, ThreadOffset thread_offset) {
  X86OpCode opcode = kX86Bkpt;
  switch (op) {
  case kOpCmp: opcode = kX86Cmp32RT;  break;
  case kOpMov: opcode = kX86Mov32RT;  break;
  default:
    LOG(FATAL) << "Bad opcode: " << op;
    break;
  }
  NewLIR2(opcode, r_dest, thread_offset.Int32Value());
}

/*
 * Generate array load
 */
void X86Mir2Lir::GenArrayGet(int opt_flags, OpSize size, RegLocation rl_array,
                          RegLocation rl_index, RegLocation rl_dest, int scale) {
  RegisterClass reg_class = oat_reg_class_by_size(size);
  int len_offset = mirror::Array::LengthOffset().Int32Value();
  int data_offset;
  RegLocation rl_result;
  rl_array = LoadValue(rl_array, kCoreReg);
  rl_index = LoadValue(rl_index, kCoreReg);

  if (size == kLong || size == kDouble) {
    data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Int32Value();
  } else {
    data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Int32Value();
  }

  /* null object? */
  GenNullCheck(rl_array.s_reg_low, rl_array.low_reg, opt_flags);

  if (!(opt_flags & MIR_IGNORE_RANGE_CHECK)) {
    /* if (rl_index >= [rl_array + len_offset]) goto kThrowArrayBounds */
    GenRegMemCheck(kCondUge, rl_index.low_reg, rl_array.low_reg,
                   len_offset, kThrowArrayBounds);
  }
  if ((size == kLong) || (size == kDouble)) {
    int reg_addr = AllocTemp();
    OpLea(reg_addr, rl_array.low_reg, rl_index.low_reg, scale, data_offset);
    FreeTemp(rl_array.low_reg);
    FreeTemp(rl_index.low_reg);
    rl_result = EvalLoc(rl_dest, reg_class, true);
    LoadBaseIndexedDisp(reg_addr, INVALID_REG, 0, 0, rl_result.low_reg,
                        rl_result.high_reg, size, INVALID_SREG);
    StoreValueWide(rl_dest, rl_result);
  } else {
    rl_result = EvalLoc(rl_dest, reg_class, true);

    LoadBaseIndexedDisp(rl_array.low_reg, rl_index.low_reg, scale,
                        data_offset, rl_result.low_reg, INVALID_REG, size,
                        INVALID_SREG);

    StoreValue(rl_dest, rl_result);
  }
}

/*
 * Generate array store
 *
 */
void X86Mir2Lir::GenArrayPut(int opt_flags, OpSize size, RegLocation rl_array,
                             RegLocation rl_index, RegLocation rl_src, int scale) {
  RegisterClass reg_class = oat_reg_class_by_size(size);
  int len_offset = mirror::Array::LengthOffset().Int32Value();
  int data_offset;

  if (size == kLong || size == kDouble) {
    data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Int32Value();
  } else {
    data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Int32Value();
  }

  rl_array = LoadValue(rl_array, kCoreReg);
  rl_index = LoadValue(rl_index, kCoreReg);

  /* null object? */
  GenNullCheck(rl_array.s_reg_low, rl_array.low_reg, opt_flags);

  if (!(opt_flags & MIR_IGNORE_RANGE_CHECK)) {
    /* if (rl_index >= [rl_array + len_offset]) goto kThrowArrayBounds */
    GenRegMemCheck(kCondUge, rl_index.low_reg, rl_array.low_reg, len_offset, kThrowArrayBounds);
  }
  if ((size == kLong) || (size == kDouble)) {
    rl_src = LoadValueWide(rl_src, reg_class);
  } else {
    rl_src = LoadValue(rl_src, reg_class);
  }
  // If the src reg can't be byte accessed, move it to a temp first.
  if ((size == kSignedByte || size == kUnsignedByte) && rl_src.low_reg >= 4) {
    int temp = AllocTemp();
    OpRegCopy(temp, rl_src.low_reg);
    StoreBaseIndexedDisp(rl_array.low_reg, rl_index.low_reg, scale, data_offset, temp,
                         INVALID_REG, size, INVALID_SREG);
  } else {
    StoreBaseIndexedDisp(rl_array.low_reg, rl_index.low_reg, scale, data_offset, rl_src.low_reg,
                         rl_src.high_reg, size, INVALID_SREG);
  }
}

/*
 * Generate array store
 *
 */
void X86Mir2Lir::GenArrayObjPut(int opt_flags, RegLocation rl_array,
                             RegLocation rl_index, RegLocation rl_src, int scale) {
  int len_offset = mirror::Array::LengthOffset().Int32Value();
  int data_offset = mirror::Array::DataOffset(sizeof(mirror::Object*)).Int32Value();

  FlushAllRegs();  // Use explicit registers
  LockCallTemps();

  int r_value = TargetReg(kArg0);  // Register holding value
  int r_array_class = TargetReg(kArg1);  // Register holding array's Class
  int r_array = TargetReg(kArg2);  // Register holding array
  int r_index = TargetReg(kArg3);  // Register holding index into array

  LoadValueDirectFixed(rl_array, r_array);  // Grab array
  LoadValueDirectFixed(rl_src, r_value);  // Grab value
  LoadValueDirectFixed(rl_index, r_index);  // Grab index

  GenNullCheck(rl_array.s_reg_low, r_array, opt_flags);  // NPE?

  // Store of null?
  LIR* null_value_check = OpCmpImmBranch(kCondEq, r_value, 0, NULL);

  // Get the array's class.
  LoadWordDisp(r_array, mirror::Object::ClassOffset().Int32Value(), r_array_class);
  CallRuntimeHelperRegReg(QUICK_ENTRYPOINT_OFFSET(pCanPutArrayElement), r_value,
                          r_array_class, true);
  // Redo LoadValues in case they didn't survive the call.
  LoadValueDirectFixed(rl_array, r_array);  // Reload array
  LoadValueDirectFixed(rl_index, r_index);  // Reload index
  LoadValueDirectFixed(rl_src, r_value);  // Reload value
  r_array_class = INVALID_REG;

  // Branch here if value to be stored == null
  LIR* target = NewLIR0(kPseudoTargetLabel);
  null_value_check->target = target;

  // make an extra temp available for card mark below
  FreeTemp(TargetReg(kArg1));
  if (!(opt_flags & MIR_IGNORE_RANGE_CHECK)) {
    /* if (rl_index >= [rl_array + len_offset]) goto kThrowArrayBounds */
    GenRegMemCheck(kCondUge, r_index, r_array, len_offset, kThrowArrayBounds);
  }
  StoreBaseIndexedDisp(r_array, r_index, scale,
                       data_offset, r_value, INVALID_REG, kWord, INVALID_SREG);
  FreeTemp(r_index);
  if (!mir_graph_->IsConstantNullRef(rl_src)) {
    MarkGCCard(r_value, r_array);
  }
}

void X86Mir2Lir::GenShiftImmOpLong(Instruction::Code opcode, RegLocation rl_dest,
                                   RegLocation rl_src1, RegLocation rl_shift) {
  // Default implementation is just to ignore the constant case.
  GenShiftOpLong(opcode, rl_dest, rl_src1, rl_shift);
}

void X86Mir2Lir::GenArithImmOpLong(Instruction::Code opcode,
                                   RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2) {
  // Default - bail to non-const handler.
  GenArithOpLong(opcode, rl_dest, rl_src1, rl_src2);
}

}  // namespace art
