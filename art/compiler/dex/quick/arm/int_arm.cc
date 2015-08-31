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

/* This file contains codegen for the Thumb2 ISA. */

#include "arm_lir.h"
#include "codegen_arm.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "mirror/array.h"

namespace art {

LIR* ArmMir2Lir::OpCmpBranch(ConditionCode cond, int src1,
         int src2, LIR* target) {
  OpRegReg(kOpCmp, src1, src2);
  return OpCondBranch(cond, target);
}

/*
 * Generate a Thumb2 IT instruction, which can nullify up to
 * four subsequent instructions based on a condition and its
 * inverse.  The condition applies to the first instruction, which
 * is executed if the condition is met.  The string "guide" consists
 * of 0 to 3 chars, and applies to the 2nd through 4th instruction.
 * A "T" means the instruction is executed if the condition is
 * met, and an "E" means the instruction is executed if the condition
 * is not met.
 */
LIR* ArmMir2Lir::OpIT(ConditionCode ccode, const char* guide) {
  int mask;
  int mask3 = 0;
  int mask2 = 0;
  int mask1 = 0;
  ArmConditionCode code = ArmConditionEncoding(ccode);
  int cond_bit = code & 1;
  int alt_bit = cond_bit ^ 1;

  // Note: case fallthroughs intentional
  switch (strlen(guide)) {
    case 3:
      mask1 = (guide[2] == 'T') ? cond_bit : alt_bit;
    case 2:
      mask2 = (guide[1] == 'T') ? cond_bit : alt_bit;
    case 1:
      mask3 = (guide[0] == 'T') ? cond_bit : alt_bit;
      break;
    case 0:
      break;
    default:
      LOG(FATAL) << "OAT: bad case in OpIT";
  }
  mask = (mask3 << 3) | (mask2 << 2) | (mask1 << 1) |
       (1 << (3 - strlen(guide)));
  return NewLIR2(kThumb2It, code, mask);
}

/*
 * 64-bit 3way compare function.
 *     mov   rX, #-1
 *     cmp   op1hi, op2hi
 *     blt   done
 *     bgt   flip
 *     sub   rX, op1lo, op2lo (treat as unsigned)
 *     beq   done
 *     ite   hi
 *     mov(hi)   rX, #-1
 *     mov(!hi)  rX, #1
 * flip:
 *     neg   rX
 * done:
 */
void ArmMir2Lir::GenCmpLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  LIR* target1;
  LIR* target2;
  rl_src1 = LoadValueWide(rl_src1, kCoreReg);
  rl_src2 = LoadValueWide(rl_src2, kCoreReg);
  int t_reg = AllocTemp();
  LoadConstant(t_reg, -1);
  OpRegReg(kOpCmp, rl_src1.high_reg, rl_src2.high_reg);
  LIR* branch1 = OpCondBranch(kCondLt, NULL);
  LIR* branch2 = OpCondBranch(kCondGt, NULL);
  OpRegRegReg(kOpSub, t_reg, rl_src1.low_reg, rl_src2.low_reg);
  LIR* branch3 = OpCondBranch(kCondEq, NULL);

  OpIT(kCondHi, "E");
  NewLIR2(kThumb2MovImmShift, t_reg, ModifiedImmediate(-1));
  LoadConstant(t_reg, 1);
  GenBarrier();

  target2 = NewLIR0(kPseudoTargetLabel);
  OpRegReg(kOpNeg, t_reg, t_reg);

  target1 = NewLIR0(kPseudoTargetLabel);

  RegLocation rl_temp = LocCReturn();  // Just using as template, will change
  rl_temp.low_reg = t_reg;
  StoreValue(rl_dest, rl_temp);
  FreeTemp(t_reg);

  branch1->target = target1;
  branch2->target = target2;
  branch3->target = branch1->target;
}

void ArmMir2Lir::GenFusedLongCmpImmBranch(BasicBlock* bb, RegLocation rl_src1,
                                          int64_t val, ConditionCode ccode) {
  int32_t val_lo = Low32Bits(val);
  int32_t val_hi = High32Bits(val);
  DCHECK_GE(ModifiedImmediate(val_lo), 0);
  DCHECK_GE(ModifiedImmediate(val_hi), 0);
  LIR* taken = &block_label_list_[bb->taken->id];
  LIR* not_taken = &block_label_list_[bb->fall_through->id];
  rl_src1 = LoadValueWide(rl_src1, kCoreReg);
  int32_t low_reg = rl_src1.low_reg;
  int32_t high_reg = rl_src1.high_reg;

  switch (ccode) {
    case kCondEq:
    case kCondNe:
      LIR* target;
      ConditionCode condition;
      if (ccode == kCondEq) {
        target = not_taken;
        condition = kCondEq;
      } else {
        target = taken;
        condition = kCondNe;
      }
      if (val == 0) {
        int t_reg = AllocTemp();
        NewLIR4(kThumb2OrrRRRs, t_reg, low_reg, high_reg, 0);
        FreeTemp(t_reg);
        OpCondBranch(condition, taken);
        return;
      }
      OpCmpImmBranch(kCondNe, high_reg, val_hi, target);
      break;
    case kCondLt:
      OpCmpImmBranch(kCondLt, high_reg, val_hi, taken);
      OpCmpImmBranch(kCondGt, high_reg, val_hi, not_taken);
      ccode = kCondCc;
      break;
    case kCondLe:
      OpCmpImmBranch(kCondLt, high_reg, val_hi, taken);
      OpCmpImmBranch(kCondGt, high_reg, val_hi, not_taken);
      ccode = kCondLs;
      break;
    case kCondGt:
      OpCmpImmBranch(kCondGt, high_reg, val_hi, taken);
      OpCmpImmBranch(kCondLt, high_reg, val_hi, not_taken);
      ccode = kCondHi;
      break;
    case kCondGe:
      OpCmpImmBranch(kCondGt, high_reg, val_hi, taken);
      OpCmpImmBranch(kCondLt, high_reg, val_hi, not_taken);
      ccode = kCondCs;
      break;
    default:
      LOG(FATAL) << "Unexpected ccode: " << ccode;
  }
  OpCmpImmBranch(ccode, low_reg, val_lo, taken);
}

void ArmMir2Lir::GenSelect(BasicBlock* bb, MIR* mir) {
  RegLocation rl_result;
  RegLocation rl_src = mir_graph_->GetSrc(mir, 0);
  // Temporary debugging code
  int dest_sreg = mir->ssa_rep->defs[0];
  if ((dest_sreg < 0) || (dest_sreg >= mir_graph_->GetNumSSARegs())) {
    LOG(INFO) << "Bad target sreg: " << dest_sreg << ", in "
              << PrettyMethod(cu_->method_idx, *cu_->dex_file);
    LOG(INFO) << "at dex offset 0x" << std::hex << mir->offset;
    LOG(INFO) << "vreg = " << mir_graph_->SRegToVReg(dest_sreg);
    LOG(INFO) << "num uses = " << mir->ssa_rep->num_uses;
    if (mir->ssa_rep->num_uses == 1) {
      LOG(INFO) << "CONST case, vals = " << mir->dalvikInsn.vB << ", " << mir->dalvikInsn.vC;
    } else {
      LOG(INFO) << "MOVE case, operands = " << mir->ssa_rep->uses[1] << ", "
                << mir->ssa_rep->uses[2];
    }
    CHECK(false) << "Invalid target sreg on Select.";
  }
  // End temporary debugging code
  RegLocation rl_dest = mir_graph_->GetDest(mir);
  rl_src = LoadValue(rl_src, kCoreReg);
  if (mir->ssa_rep->num_uses == 1) {
    // CONST case
    int true_val = mir->dalvikInsn.vB;
    int false_val = mir->dalvikInsn.vC;
    rl_result = EvalLoc(rl_dest, kCoreReg, true);
    if ((true_val == 1) && (false_val == 0)) {
      OpRegRegImm(kOpRsub, rl_result.low_reg, rl_src.low_reg, 1);
      OpIT(kCondCc, "");
      LoadConstant(rl_result.low_reg, 0);
      GenBarrier();  // Add a scheduling barrier to keep the IT shadow intact
    } else if (InexpensiveConstantInt(true_val) && InexpensiveConstantInt(false_val)) {
      OpRegImm(kOpCmp, rl_src.low_reg, 0);
      OpIT(kCondEq, "E");
      LoadConstant(rl_result.low_reg, true_val);
      LoadConstant(rl_result.low_reg, false_val);
      GenBarrier();  // Add a scheduling barrier to keep the IT shadow intact
    } else {
      // Unlikely case - could be tuned.
      int t_reg1 = AllocTemp();
      int t_reg2 = AllocTemp();
      LoadConstant(t_reg1, true_val);
      LoadConstant(t_reg2, false_val);
      OpRegImm(kOpCmp, rl_src.low_reg, 0);
      OpIT(kCondEq, "E");
      OpRegCopy(rl_result.low_reg, t_reg1);
      OpRegCopy(rl_result.low_reg, t_reg2);
      GenBarrier();  // Add a scheduling barrier to keep the IT shadow intact
    }
  } else {
    // MOVE case
    RegLocation rl_true = mir_graph_->reg_location_[mir->ssa_rep->uses[1]];
    RegLocation rl_false = mir_graph_->reg_location_[mir->ssa_rep->uses[2]];
    rl_true = LoadValue(rl_true, kCoreReg);
    rl_false = LoadValue(rl_false, kCoreReg);
    rl_result = EvalLoc(rl_dest, kCoreReg, true);
    OpRegImm(kOpCmp, rl_src.low_reg, 0);
    OpIT(kCondEq, "E");
    LIR* l1 = OpRegCopy(rl_result.low_reg, rl_true.low_reg);
    l1->flags.is_nop = false;  // Make sure this instruction isn't optimized away
    LIR* l2 = OpRegCopy(rl_result.low_reg, rl_false.low_reg);
    l2->flags.is_nop = false;  // Make sure this instruction isn't optimized away
    GenBarrier();  // Add a scheduling barrier to keep the IT shadow intact
  }
  StoreValue(rl_dest, rl_result);
}

void ArmMir2Lir::GenFusedLongCmpBranch(BasicBlock* bb, MIR* mir) {
  RegLocation rl_src1 = mir_graph_->GetSrcWide(mir, 0);
  RegLocation rl_src2 = mir_graph_->GetSrcWide(mir, 2);
  // Normalize such that if either operand is constant, src2 will be constant.
  ConditionCode ccode = static_cast<ConditionCode>(mir->dalvikInsn.arg[0]);
  if (rl_src1.is_const) {
    RegLocation rl_temp = rl_src1;
    rl_src1 = rl_src2;
    rl_src2 = rl_temp;
    ccode = FlipComparisonOrder(ccode);
  }
  if (rl_src2.is_const) {
    RegLocation rl_temp = UpdateLocWide(rl_src2);
    // Do special compare/branch against simple const operand if not already in registers.
    int64_t val = mir_graph_->ConstantValueWide(rl_src2);
    if ((rl_temp.location != kLocPhysReg) &&
        ((ModifiedImmediate(Low32Bits(val)) >= 0) && (ModifiedImmediate(High32Bits(val)) >= 0))) {
      GenFusedLongCmpImmBranch(bb, rl_src1, val, ccode);
      return;
    }
  }
  LIR* taken = &block_label_list_[bb->taken->id];
  LIR* not_taken = &block_label_list_[bb->fall_through->id];
  rl_src1 = LoadValueWide(rl_src1, kCoreReg);
  rl_src2 = LoadValueWide(rl_src2, kCoreReg);
  OpRegReg(kOpCmp, rl_src1.high_reg, rl_src2.high_reg);
  switch (ccode) {
    case kCondEq:
      OpCondBranch(kCondNe, not_taken);
      break;
    case kCondNe:
      OpCondBranch(kCondNe, taken);
      break;
    case kCondLt:
      OpCondBranch(kCondLt, taken);
      OpCondBranch(kCondGt, not_taken);
      ccode = kCondCc;
      break;
    case kCondLe:
      OpCondBranch(kCondLt, taken);
      OpCondBranch(kCondGt, not_taken);
      ccode = kCondLs;
      break;
    case kCondGt:
      OpCondBranch(kCondGt, taken);
      OpCondBranch(kCondLt, not_taken);
      ccode = kCondHi;
      break;
    case kCondGe:
      OpCondBranch(kCondGt, taken);
      OpCondBranch(kCondLt, not_taken);
      ccode = kCondCs;
      break;
    default:
      LOG(FATAL) << "Unexpected ccode: " << ccode;
  }
  OpRegReg(kOpCmp, rl_src1.low_reg, rl_src2.low_reg);
  OpCondBranch(ccode, taken);
}

/*
 * Generate a register comparison to an immediate and branch.  Caller
 * is responsible for setting branch target field.
 */
LIR* ArmMir2Lir::OpCmpImmBranch(ConditionCode cond, int reg, int check_value,
                                LIR* target) {
  LIR* branch;
  int mod_imm;
  ArmConditionCode arm_cond = ArmConditionEncoding(cond);
  if ((ARM_LOWREG(reg)) && (check_value == 0) &&
     ((arm_cond == kArmCondEq) || (arm_cond == kArmCondNe))) {
    branch = NewLIR2((arm_cond == kArmCondEq) ? kThumb2Cbz : kThumb2Cbnz,
                     reg, 0);
  } else {
    mod_imm = ModifiedImmediate(check_value);
    if (ARM_LOWREG(reg) && ((check_value & 0xff) == check_value)) {
      NewLIR2(kThumbCmpRI8, reg, check_value);
    } else if (mod_imm >= 0) {
      NewLIR2(kThumb2CmpRI12, reg, mod_imm);
    } else {
      int t_reg = AllocTemp();
      LoadConstant(t_reg, check_value);
      OpRegReg(kOpCmp, reg, t_reg);
    }
    branch = NewLIR2(kThumbBCond, 0, arm_cond);
  }
  branch->target = target;
  return branch;
}

LIR* ArmMir2Lir::OpRegCopyNoInsert(int r_dest, int r_src) {
  LIR* res;
  int opcode;
  if (ARM_FPREG(r_dest) || ARM_FPREG(r_src))
    return OpFpRegCopy(r_dest, r_src);
  if (ARM_LOWREG(r_dest) && ARM_LOWREG(r_src))
    opcode = kThumbMovRR;
  else if (!ARM_LOWREG(r_dest) && !ARM_LOWREG(r_src))
     opcode = kThumbMovRR_H2H;
  else if (ARM_LOWREG(r_dest))
     opcode = kThumbMovRR_H2L;
  else
     opcode = kThumbMovRR_L2H;
  res = RawLIR(current_dalvik_offset_, opcode, r_dest, r_src);
  if (!(cu_->disable_opt & (1 << kSafeOptimizations)) && r_dest == r_src) {
    res->flags.is_nop = true;
  }
  return res;
}

LIR* ArmMir2Lir::OpRegCopy(int r_dest, int r_src) {
  LIR* res = OpRegCopyNoInsert(r_dest, r_src);
  AppendLIR(res);
  return res;
}

void ArmMir2Lir::OpRegCopyWide(int dest_lo, int dest_hi, int src_lo,
                               int src_hi) {
  bool dest_fp = ARM_FPREG(dest_lo) && ARM_FPREG(dest_hi);
  bool src_fp = ARM_FPREG(src_lo) && ARM_FPREG(src_hi);
  DCHECK_EQ(ARM_FPREG(src_lo), ARM_FPREG(src_hi));
  DCHECK_EQ(ARM_FPREG(dest_lo), ARM_FPREG(dest_hi));
  if (dest_fp) {
    if (src_fp) {
      OpRegCopy(S2d(dest_lo, dest_hi), S2d(src_lo, src_hi));
    } else {
      NewLIR3(kThumb2Fmdrr, S2d(dest_lo, dest_hi), src_lo, src_hi);
    }
  } else {
    if (src_fp) {
      NewLIR3(kThumb2Fmrrd, dest_lo, dest_hi, S2d(src_lo, src_hi));
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

// Table of magic divisors
struct MagicTable {
  uint32_t magic;
  uint32_t shift;
  DividePattern pattern;
};

static const MagicTable magic_table[] = {
  {0, 0, DivideNone},        // 0
  {0, 0, DivideNone},        // 1
  {0, 0, DivideNone},        // 2
  {0x55555556, 0, Divide3},  // 3
  {0, 0, DivideNone},        // 4
  {0x66666667, 1, Divide5},  // 5
  {0x2AAAAAAB, 0, Divide3},  // 6
  {0x92492493, 2, Divide7},  // 7
  {0, 0, DivideNone},        // 8
  {0x38E38E39, 1, Divide5},  // 9
  {0x66666667, 2, Divide5},  // 10
  {0x2E8BA2E9, 1, Divide5},  // 11
  {0x2AAAAAAB, 1, Divide5},  // 12
  {0x4EC4EC4F, 2, Divide5},  // 13
  {0x92492493, 3, Divide7},  // 14
  {0x88888889, 3, Divide7},  // 15
};

// Integer division by constant via reciprocal multiply (Hacker's Delight, 10-4)
bool ArmMir2Lir::SmallLiteralDivRem(Instruction::Code dalvik_opcode, bool is_div,
                                    RegLocation rl_src, RegLocation rl_dest, int lit) {
  if ((lit < 0) || (lit >= static_cast<int>(sizeof(magic_table)/sizeof(magic_table[0])))) {
    return false;
  }
  DividePattern pattern = magic_table[lit].pattern;
  if (pattern == DivideNone) {
    return false;
  }
  // Tuning: add rem patterns
  if (!is_div) {
    return false;
  }

  int r_magic = AllocTemp();
  LoadConstant(r_magic, magic_table[lit].magic);
  rl_src = LoadValue(rl_src, kCoreReg);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  int r_hi = AllocTemp();
  int r_lo = AllocTemp();
  NewLIR4(kThumb2Smull, r_lo, r_hi, r_magic, rl_src.low_reg);
  switch (pattern) {
    case Divide3:
      OpRegRegRegShift(kOpSub, rl_result.low_reg, r_hi,
               rl_src.low_reg, EncodeShift(kArmAsr, 31));
      break;
    case Divide5:
      OpRegRegImm(kOpAsr, r_lo, rl_src.low_reg, 31);
      OpRegRegRegShift(kOpRsub, rl_result.low_reg, r_lo, r_hi,
               EncodeShift(kArmAsr, magic_table[lit].shift));
      break;
    case Divide7:
      OpRegReg(kOpAdd, r_hi, rl_src.low_reg);
      OpRegRegImm(kOpAsr, r_lo, rl_src.low_reg, 31);
      OpRegRegRegShift(kOpRsub, rl_result.low_reg, r_lo, r_hi,
               EncodeShift(kArmAsr, magic_table[lit].shift));
      break;
    default:
      LOG(FATAL) << "Unexpected pattern: " << pattern;
  }
  StoreValue(rl_dest, rl_result);
  return true;
}

LIR* ArmMir2Lir::GenRegMemCheck(ConditionCode c_code,
                    int reg1, int base, int offset, ThrowKind kind) {
  LOG(FATAL) << "Unexpected use of GenRegMemCheck for Arm";
  return NULL;
}

RegLocation ArmMir2Lir::GenDivRemLit(RegLocation rl_dest, int reg1, int lit,
                                     bool is_div) {
  LOG(FATAL) << "Unexpected use of GenDivRemLit for Arm";
  return rl_dest;
}

RegLocation ArmMir2Lir::GenDivRem(RegLocation rl_dest, int reg1, int reg2,
                                  bool is_div) {
  LOG(FATAL) << "Unexpected use of GenDivRem for Arm";
  return rl_dest;
}

bool ArmMir2Lir::GenInlinedMinMaxInt(CallInfo* info, bool is_min) {
  DCHECK_EQ(cu_->instruction_set, kThumb2);
  RegLocation rl_src1 = info->args[0];
  RegLocation rl_src2 = info->args[1];
  rl_src1 = LoadValue(rl_src1, kCoreReg);
  rl_src2 = LoadValue(rl_src2, kCoreReg);
  RegLocation rl_dest = InlineTarget(info);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  OpRegReg(kOpCmp, rl_src1.low_reg, rl_src2.low_reg);
  OpIT((is_min) ? kCondGt : kCondLt, "E");
  OpRegReg(kOpMov, rl_result.low_reg, rl_src2.low_reg);
  OpRegReg(kOpMov, rl_result.low_reg, rl_src1.low_reg);
  GenBarrier();
  StoreValue(rl_dest, rl_result);
  return true;
}

void ArmMir2Lir::OpLea(int rBase, int reg1, int reg2, int scale, int offset) {
  LOG(FATAL) << "Unexpected use of OpLea for Arm";
}

void ArmMir2Lir::OpTlsCmp(ThreadOffset offset, int val) {
  LOG(FATAL) << "Unexpected use of OpTlsCmp for Arm";
}

bool ArmMir2Lir::GenInlinedCas32(CallInfo* info, bool need_write_barrier) {
  DCHECK_EQ(cu_->instruction_set, kThumb2);
  // Unused - RegLocation rl_src_unsafe = info->args[0];
  RegLocation rl_src_obj= info->args[1];  // Object - known non-null
  RegLocation rl_src_offset= info->args[2];  // long low
  rl_src_offset.wide = 0;  // ignore high half in info->args[3]
  RegLocation rl_src_expected= info->args[4];  // int or Object
  RegLocation rl_src_new_value= info->args[5];  // int or Object
  RegLocation rl_dest = InlineTarget(info);  // boolean place for result


  // Release store semantics, get the barrier out of the way.  TODO: revisit
  GenMemBarrier(kStoreLoad);

  RegLocation rl_object = LoadValue(rl_src_obj, kCoreReg);
  RegLocation rl_new_value = LoadValue(rl_src_new_value, kCoreReg);

  if (need_write_barrier && !mir_graph_->IsConstantNullRef(rl_new_value)) {
    // Mark card for object assuming new value is stored.
    MarkGCCard(rl_new_value.low_reg, rl_object.low_reg);
  }

  RegLocation rl_offset = LoadValue(rl_src_offset, kCoreReg);

  int r_ptr = AllocTemp();
  OpRegRegReg(kOpAdd, r_ptr, rl_object.low_reg, rl_offset.low_reg);

  // Free now unneeded rl_object and rl_offset to give more temps.
  ClobberSReg(rl_object.s_reg_low);
  FreeTemp(rl_object.low_reg);
  ClobberSReg(rl_offset.s_reg_low);
  FreeTemp(rl_offset.low_reg);

  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  LoadConstant(rl_result.low_reg, 0);  // r_result := 0

  // while ([r_ptr] == rExpected && r_result == 0) {
  //   [r_ptr] <- r_new_value && r_result := success ? 0 : 1
  //   r_result ^= 1
  // }
  int r_old_value = AllocTemp();
  LIR* target = NewLIR0(kPseudoTargetLabel);
  NewLIR3(kThumb2Ldrex, r_old_value, r_ptr, 0);

  RegLocation rl_expected = LoadValue(rl_src_expected, kCoreReg);
  OpRegReg(kOpCmp, r_old_value, rl_expected.low_reg);
  FreeTemp(r_old_value);  // Now unneeded.
  OpIT(kCondEq, "TT");
  NewLIR4(kThumb2Strex /* eq */, rl_result.low_reg, rl_new_value.low_reg, r_ptr, 0);
  FreeTemp(r_ptr);  // Now unneeded.
  OpRegImm(kOpXor /* eq */, rl_result.low_reg, 1);
  OpRegImm(kOpCmp /* eq */, rl_result.low_reg, 0);
  OpCondBranch(kCondEq, target);

  StoreValue(rl_dest, rl_result);

  return true;
}

LIR* ArmMir2Lir::OpPcRelLoad(int reg, LIR* target) {
  return RawLIR(current_dalvik_offset_, kThumb2LdrPcRel12, reg, 0, 0, 0, 0, target);
}

LIR* ArmMir2Lir::OpVldm(int rBase, int count) {
  return NewLIR3(kThumb2Vldms, rBase, fr0, count);
}

LIR* ArmMir2Lir::OpVstm(int rBase, int count) {
  return NewLIR3(kThumb2Vstms, rBase, fr0, count);
}

void ArmMir2Lir::GenMultiplyByTwoBitMultiplier(RegLocation rl_src,
                                               RegLocation rl_result, int lit,
                                               int first_bit, int second_bit) {
  OpRegRegRegShift(kOpAdd, rl_result.low_reg, rl_src.low_reg, rl_src.low_reg,
                   EncodeShift(kArmLsl, second_bit - first_bit));
  if (first_bit != 0) {
    OpRegRegImm(kOpLsl, rl_result.low_reg, rl_result.low_reg, first_bit);
  }
}

void ArmMir2Lir::GenDivZeroCheck(int reg_lo, int reg_hi) {
  int t_reg = AllocTemp();
  NewLIR4(kThumb2OrrRRRs, t_reg, reg_lo, reg_hi, 0);
  FreeTemp(t_reg);
  GenCheck(kCondEq, kThrowDivZero);
}

// Test suspend flag, return target of taken suspend branch
LIR* ArmMir2Lir::OpTestSuspend(LIR* target) {
  NewLIR2(kThumbSubRI8, rARM_SUSPEND, 1);
  return OpCondBranch((target == NULL) ? kCondEq : kCondNe, target);
}

// Decrement register and branch on condition
LIR* ArmMir2Lir::OpDecAndBranch(ConditionCode c_code, int reg, LIR* target) {
  // Combine sub & test using sub setflags encoding here
  NewLIR3(kThumb2SubsRRI12, reg, reg, 1);
  return OpCondBranch(c_code, target);
}

void ArmMir2Lir::GenMemBarrier(MemBarrierKind barrier_kind) {
#if ANDROID_SMP != 0
  int dmb_flavor;
  // TODO: revisit Arm barrier kinds
  switch (barrier_kind) {
    case kLoadStore: dmb_flavor = kSY; break;
    case kLoadLoad: dmb_flavor = kSY; break;
    case kStoreStore: dmb_flavor = kST; break;
    case kStoreLoad: dmb_flavor = kSY; break;
    default:
      LOG(FATAL) << "Unexpected MemBarrierKind: " << barrier_kind;
      dmb_flavor = kSY;  // quiet gcc.
      break;
  }
  LIR* dmb = NewLIR1(kThumb2Dmb, dmb_flavor);
  dmb->def_mask = ENCODE_ALL;
#endif
}

void ArmMir2Lir::GenNegLong(RegLocation rl_dest, RegLocation rl_src) {
  rl_src = LoadValueWide(rl_src, kCoreReg);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  int z_reg = AllocTemp();
  LoadConstantNoClobber(z_reg, 0);
  // Check for destructive overlap
  if (rl_result.low_reg == rl_src.high_reg) {
    int t_reg = AllocTemp();
    OpRegRegReg(kOpSub, rl_result.low_reg, z_reg, rl_src.low_reg);
    OpRegRegReg(kOpSbc, rl_result.high_reg, z_reg, t_reg);
    FreeTemp(t_reg);
  } else {
    OpRegRegReg(kOpSub, rl_result.low_reg, z_reg, rl_src.low_reg);
    OpRegRegReg(kOpSbc, rl_result.high_reg, z_reg, rl_src.high_reg);
  }
  FreeTemp(z_reg);
  StoreValueWide(rl_dest, rl_result);
}


 /*
  * Check to see if a result pair has a misaligned overlap with an operand pair.  This
  * is not usual for dx to generate, but it is legal (for now).  In a future rev of
  * dex, we'll want to make this case illegal.
  */
bool ArmMir2Lir::BadOverlap(RegLocation rl_src, RegLocation rl_dest) {
  DCHECK(rl_src.wide);
  DCHECK(rl_dest.wide);
  return (abs(mir_graph_->SRegToVReg(rl_src.s_reg_low) - mir_graph_->SRegToVReg(rl_dest.s_reg_low)) == 1);
}

void ArmMir2Lir::GenMulLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
    /*
     * To pull off inline multiply, we have a worst-case requirement of 8 temporary
     * registers.  Normally for Arm, we get 5.  We can get to 6 by including
     * lr in the temp set.  The only problematic case is all operands and result are
     * distinct, and none have been promoted.  In that case, we can succeed by aggressively
     * freeing operand temp registers after they are no longer needed.  All other cases
     * can proceed normally.  We'll just punt on the case of the result having a misaligned
     * overlap with either operand and send that case to a runtime handler.
     */
    RegLocation rl_result;
    if (BadOverlap(rl_src1, rl_dest) || (BadOverlap(rl_src2, rl_dest))) {
      ThreadOffset func_offset = QUICK_ENTRYPOINT_OFFSET(pLmul);
      FlushAllRegs();
      CallRuntimeHelperRegLocationRegLocation(func_offset, rl_src1, rl_src2, false);
      rl_result = GetReturnWide(false);
      StoreValueWide(rl_dest, rl_result);
      return;
    }
    // Temporarily add LR to the temp pool, and assign it to tmp1
    MarkTemp(rARM_LR);
    FreeTemp(rARM_LR);
    int tmp1 = rARM_LR;
    LockTemp(rARM_LR);

    rl_src1 = LoadValueWide(rl_src1, kCoreReg);
    rl_src2 = LoadValueWide(rl_src2, kCoreReg);

    bool special_case = true;
    // If operands are the same, or any pair has been promoted we're not the special case.
    if ((rl_src1.s_reg_low == rl_src2.s_reg_low) ||
        (!IsTemp(rl_src1.low_reg) && !IsTemp(rl_src1.high_reg)) ||
        (!IsTemp(rl_src2.low_reg) && !IsTemp(rl_src2.high_reg))) {
      special_case = false;
    }
    // Tuning: if rl_dest has been promoted and is *not* either operand, could use directly.
    int res_lo = AllocTemp();
    int res_hi;
    if (rl_src1.low_reg == rl_src2.low_reg) {
      res_hi = AllocTemp();
      NewLIR3(kThumb2MulRRR, tmp1, rl_src1.low_reg, rl_src1.high_reg);
      NewLIR4(kThumb2Umull, res_lo, res_hi, rl_src1.low_reg, rl_src1.low_reg);
      OpRegRegRegShift(kOpAdd, res_hi, res_hi, tmp1, EncodeShift(kArmLsl, 1));
    } else {
      // In the special case, all temps are now allocated
      NewLIR3(kThumb2MulRRR, tmp1, rl_src2.low_reg, rl_src1.high_reg);
      if (special_case) {
        DCHECK_NE(rl_src1.low_reg, rl_src2.low_reg);
        DCHECK_NE(rl_src1.high_reg, rl_src2.high_reg);
        FreeTemp(rl_src1.high_reg);
      }
      res_hi = AllocTemp();

      NewLIR4(kThumb2Umull, res_lo, res_hi, rl_src2.low_reg, rl_src1.low_reg);
      NewLIR4(kThumb2Mla, tmp1, rl_src1.low_reg, rl_src2.high_reg, tmp1);
      NewLIR4(kThumb2AddRRR, res_hi, tmp1, res_hi, 0);
      if (special_case) {
        FreeTemp(rl_src1.low_reg);
        Clobber(rl_src1.low_reg);
        Clobber(rl_src1.high_reg);
      }
    }
    FreeTemp(tmp1);
    rl_result = GetReturnWide(false);  // Just using as a template.
    rl_result.low_reg = res_lo;
    rl_result.high_reg = res_hi;
    StoreValueWide(rl_dest, rl_result);
    // Now, restore lr to its non-temp status.
    Clobber(rARM_LR);
    UnmarkTemp(rARM_LR);
}

void ArmMir2Lir::GenAddLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  LOG(FATAL) << "Unexpected use of GenAddLong for Arm";
}

void ArmMir2Lir::GenSubLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  LOG(FATAL) << "Unexpected use of GenSubLong for Arm";
}

void ArmMir2Lir::GenAndLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  LOG(FATAL) << "Unexpected use of GenAndLong for Arm";
}

void ArmMir2Lir::GenOrLong(RegLocation rl_dest, RegLocation rl_src1,
                           RegLocation rl_src2) {
  LOG(FATAL) << "Unexpected use of GenOrLong for Arm";
}

void ArmMir2Lir::GenXorLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) {
  LOG(FATAL) << "Unexpected use of genXoLong for Arm";
}

/*
 * Generate array load
 */
void ArmMir2Lir::GenArrayGet(int opt_flags, OpSize size, RegLocation rl_array,
                          RegLocation rl_index, RegLocation rl_dest, int scale) {
  RegisterClass reg_class = oat_reg_class_by_size(size);
  int len_offset = mirror::Array::LengthOffset().Int32Value();
  int data_offset;
  RegLocation rl_result;
  bool constant_index = rl_index.is_const;
  rl_array = LoadValue(rl_array, kCoreReg);
  if (!constant_index) {
    rl_index = LoadValue(rl_index, kCoreReg);
  }

  if (rl_dest.wide) {
    data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Int32Value();
  } else {
    data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Int32Value();
  }

  // If index is constant, just fold it into the data offset
  if (constant_index) {
    data_offset += mir_graph_->ConstantValue(rl_index) << scale;
  }

  /* null object? */
  GenNullCheck(rl_array.s_reg_low, rl_array.low_reg, opt_flags);

  bool needs_range_check = (!(opt_flags & MIR_IGNORE_RANGE_CHECK));
  int reg_len = INVALID_REG;
  if (needs_range_check) {
    reg_len = AllocTemp();
    /* Get len */
    LoadWordDisp(rl_array.low_reg, len_offset, reg_len);
  }
  if (rl_dest.wide || rl_dest.fp || constant_index) {
    int reg_ptr;
    if (constant_index) {
      reg_ptr = rl_array.low_reg;  // NOTE: must not alter reg_ptr in constant case.
    } else {
      // No special indexed operation, lea + load w/ displacement
      reg_ptr = AllocTemp();
      OpRegRegRegShift(kOpAdd, reg_ptr, rl_array.low_reg, rl_index.low_reg,
                       EncodeShift(kArmLsl, scale));
      FreeTemp(rl_index.low_reg);
    }
    rl_result = EvalLoc(rl_dest, reg_class, true);

    if (needs_range_check) {
      if (constant_index) {
        GenImmedCheck(kCondLs, reg_len, mir_graph_->ConstantValue(rl_index), kThrowConstantArrayBounds);
      } else {
        GenRegRegCheck(kCondLs, reg_len, rl_index.low_reg, kThrowArrayBounds);
      }
      FreeTemp(reg_len);
    }
    if (rl_dest.wide) {
      LoadBaseDispWide(reg_ptr, data_offset, rl_result.low_reg, rl_result.high_reg, INVALID_SREG);
      if (!constant_index) {
        FreeTemp(reg_ptr);
      }
      StoreValueWide(rl_dest, rl_result);
    } else {
      LoadBaseDisp(reg_ptr, data_offset, rl_result.low_reg, size, INVALID_SREG);
      if (!constant_index) {
        FreeTemp(reg_ptr);
      }
      StoreValue(rl_dest, rl_result);
    }
  } else {
    // Offset base, then use indexed load
    int reg_ptr = AllocTemp();
    OpRegRegImm(kOpAdd, reg_ptr, rl_array.low_reg, data_offset);
    FreeTemp(rl_array.low_reg);
    rl_result = EvalLoc(rl_dest, reg_class, true);

    if (needs_range_check) {
      // TODO: change kCondCS to a more meaningful name, is the sense of
      // carry-set/clear flipped?
      GenRegRegCheck(kCondCs, rl_index.low_reg, reg_len, kThrowArrayBounds);
      FreeTemp(reg_len);
    }
    LoadBaseIndexed(reg_ptr, rl_index.low_reg, rl_result.low_reg, scale, size);
    FreeTemp(reg_ptr);
    StoreValue(rl_dest, rl_result);
  }
}

/*
 * Generate array store
 *
 */
void ArmMir2Lir::GenArrayPut(int opt_flags, OpSize size, RegLocation rl_array,
                          RegLocation rl_index, RegLocation rl_src, int scale) {
  RegisterClass reg_class = oat_reg_class_by_size(size);
  int len_offset = mirror::Array::LengthOffset().Int32Value();
  int data_offset;
  bool constant_index = rl_index.is_const;

  if (rl_src.wide) {
    data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Int32Value();
  } else {
    data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Int32Value();
  }

  // If index is constant, just fold it into the data offset.
  if (constant_index) {
    data_offset += mir_graph_->ConstantValue(rl_index) << scale;
  }

  rl_array = LoadValue(rl_array, kCoreReg);
  if (!constant_index) {
    rl_index = LoadValue(rl_index, kCoreReg);
  }

  int reg_ptr;
  if (constant_index) {
    reg_ptr = rl_array.low_reg;
  } else if (IsTemp(rl_array.low_reg)) {
    Clobber(rl_array.low_reg);
    reg_ptr = rl_array.low_reg;
  } else {
    reg_ptr = AllocTemp();
  }

  /* null object? */
  GenNullCheck(rl_array.s_reg_low, rl_array.low_reg, opt_flags);

  bool needs_range_check = (!(opt_flags & MIR_IGNORE_RANGE_CHECK));
  int reg_len = INVALID_REG;
  if (needs_range_check) {
    reg_len = AllocTemp();
    // NOTE: max live temps(4) here.
    /* Get len */
    LoadWordDisp(rl_array.low_reg, len_offset, reg_len);
  }
  /* at this point, reg_ptr points to array, 2 live temps */
  if (rl_src.wide || rl_src.fp || constant_index) {
    if (rl_src.wide) {
      rl_src = LoadValueWide(rl_src, reg_class);
    } else {
      rl_src = LoadValue(rl_src, reg_class);
    }
    if (!constant_index) {
      OpRegRegRegShift(kOpAdd, reg_ptr, rl_array.low_reg, rl_index.low_reg,
                       EncodeShift(kArmLsl, scale));
    }
    if (needs_range_check) {
      if (constant_index) {
        GenImmedCheck(kCondLs, reg_len, mir_graph_->ConstantValue(rl_index), kThrowConstantArrayBounds);
      } else {
        GenRegRegCheck(kCondLs, reg_len, rl_index.low_reg, kThrowArrayBounds);
      }
      FreeTemp(reg_len);
    }

    if (rl_src.wide) {
      StoreBaseDispWide(reg_ptr, data_offset, rl_src.low_reg, rl_src.high_reg);
    } else {
      StoreBaseDisp(reg_ptr, data_offset, rl_src.low_reg, size);
    }
  } else {
    /* reg_ptr -> array data */
    OpRegRegImm(kOpAdd, reg_ptr, rl_array.low_reg, data_offset);
    rl_src = LoadValue(rl_src, reg_class);
    if (needs_range_check) {
      GenRegRegCheck(kCondCs, rl_index.low_reg, reg_len, kThrowArrayBounds);
      FreeTemp(reg_len);
    }
    StoreBaseIndexed(reg_ptr, rl_index.low_reg, rl_src.low_reg,
                     scale, size);
  }
  if (!constant_index) {
    FreeTemp(reg_ptr);
  }
}

/*
 * Generate array store
 *
 */
void ArmMir2Lir::GenArrayObjPut(int opt_flags, RegLocation rl_array,
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

  bool needs_range_check = (!(opt_flags & MIR_IGNORE_RANGE_CHECK));
  int reg_len = INVALID_REG;
  if (needs_range_check) {
    reg_len = TargetReg(kArg1);
    LoadWordDisp(r_array, len_offset, reg_len);  // Get len
  }
  /* r_ptr -> array data */
  int r_ptr = AllocTemp();
  OpRegRegImm(kOpAdd, r_ptr, r_array, data_offset);
  if (needs_range_check) {
    GenRegRegCheck(kCondCs, r_index, reg_len, kThrowArrayBounds);
  }
  StoreBaseIndexed(r_ptr, r_index, r_value, scale, kWord);
  FreeTemp(r_ptr);
  FreeTemp(r_index);
  if (!mir_graph_->IsConstantNullRef(rl_src)) {
    MarkGCCard(r_value, r_array);
  }
}

void ArmMir2Lir::GenShiftImmOpLong(Instruction::Code opcode,
                                   RegLocation rl_dest, RegLocation rl_src, RegLocation rl_shift) {
  rl_src = LoadValueWide(rl_src, kCoreReg);
  // Per spec, we only care about low 6 bits of shift amount.
  int shift_amount = mir_graph_->ConstantValue(rl_shift) & 0x3f;
  if (shift_amount == 0) {
    StoreValueWide(rl_dest, rl_src);
    return;
  }
  if (BadOverlap(rl_src, rl_dest)) {
    GenShiftOpLong(opcode, rl_dest, rl_src, rl_shift);
    return;
  }
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  switch (opcode) {
    case Instruction::SHL_LONG:
    case Instruction::SHL_LONG_2ADDR:
      if (shift_amount == 1) {
        OpRegRegReg(kOpAdd, rl_result.low_reg, rl_src.low_reg, rl_src.low_reg);
        OpRegRegReg(kOpAdc, rl_result.high_reg, rl_src.high_reg, rl_src.high_reg);
      } else if (shift_amount == 32) {
        OpRegCopy(rl_result.high_reg, rl_src.low_reg);
        LoadConstant(rl_result.low_reg, 0);
      } else if (shift_amount > 31) {
        OpRegRegImm(kOpLsl, rl_result.high_reg, rl_src.low_reg, shift_amount - 32);
        LoadConstant(rl_result.low_reg, 0);
      } else {
        OpRegRegImm(kOpLsl, rl_result.high_reg, rl_src.high_reg, shift_amount);
        OpRegRegRegShift(kOpOr, rl_result.high_reg, rl_result.high_reg, rl_src.low_reg,
                         EncodeShift(kArmLsr, 32 - shift_amount));
        OpRegRegImm(kOpLsl, rl_result.low_reg, rl_src.low_reg, shift_amount);
      }
      break;
    case Instruction::SHR_LONG:
    case Instruction::SHR_LONG_2ADDR:
      if (shift_amount == 32) {
        OpRegCopy(rl_result.low_reg, rl_src.high_reg);
        OpRegRegImm(kOpAsr, rl_result.high_reg, rl_src.high_reg, 31);
      } else if (shift_amount > 31) {
        OpRegRegImm(kOpAsr, rl_result.low_reg, rl_src.high_reg, shift_amount - 32);
        OpRegRegImm(kOpAsr, rl_result.high_reg, rl_src.high_reg, 31);
      } else {
        int t_reg = AllocTemp();
        OpRegRegImm(kOpLsr, t_reg, rl_src.low_reg, shift_amount);
        OpRegRegRegShift(kOpOr, rl_result.low_reg, t_reg, rl_src.high_reg,
                         EncodeShift(kArmLsl, 32 - shift_amount));
        FreeTemp(t_reg);
        OpRegRegImm(kOpAsr, rl_result.high_reg, rl_src.high_reg, shift_amount);
      }
      break;
    case Instruction::USHR_LONG:
    case Instruction::USHR_LONG_2ADDR:
      if (shift_amount == 32) {
        OpRegCopy(rl_result.low_reg, rl_src.high_reg);
        LoadConstant(rl_result.high_reg, 0);
      } else if (shift_amount > 31) {
        OpRegRegImm(kOpLsr, rl_result.low_reg, rl_src.high_reg, shift_amount - 32);
        LoadConstant(rl_result.high_reg, 0);
      } else {
        int t_reg = AllocTemp();
        OpRegRegImm(kOpLsr, t_reg, rl_src.low_reg, shift_amount);
        OpRegRegRegShift(kOpOr, rl_result.low_reg, t_reg, rl_src.high_reg,
                         EncodeShift(kArmLsl, 32 - shift_amount));
        FreeTemp(t_reg);
        OpRegRegImm(kOpLsr, rl_result.high_reg, rl_src.high_reg, shift_amount);
      }
      break;
    default:
      LOG(FATAL) << "Unexpected case";
  }
  StoreValueWide(rl_dest, rl_result);
}

void ArmMir2Lir::GenArithImmOpLong(Instruction::Code opcode,
                                   RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2) {
  if ((opcode == Instruction::SUB_LONG_2ADDR) || (opcode == Instruction::SUB_LONG)) {
    if (!rl_src2.is_const) {
      // Don't bother with special handling for subtract from immediate.
      GenArithOpLong(opcode, rl_dest, rl_src1, rl_src2);
      return;
    }
  } else {
    // Normalize
    if (!rl_src2.is_const) {
      DCHECK(rl_src1.is_const);
      RegLocation rl_temp = rl_src1;
      rl_src1 = rl_src2;
      rl_src2 = rl_temp;
    }
  }
  if (BadOverlap(rl_src1, rl_dest)) {
    GenArithOpLong(opcode, rl_dest, rl_src1, rl_src2);
    return;
  }
  DCHECK(rl_src2.is_const);
  int64_t val = mir_graph_->ConstantValueWide(rl_src2);
  uint32_t val_lo = Low32Bits(val);
  uint32_t val_hi = High32Bits(val);
  int32_t mod_imm_lo = ModifiedImmediate(val_lo);
  int32_t mod_imm_hi = ModifiedImmediate(val_hi);

  // Only a subset of add/sub immediate instructions set carry - so bail if we don't fit
  switch (opcode) {
    case Instruction::ADD_LONG:
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::SUB_LONG:
    case Instruction::SUB_LONG_2ADDR:
      if ((mod_imm_lo < 0) || (mod_imm_hi < 0)) {
        GenArithOpLong(opcode, rl_dest, rl_src1, rl_src2);
        return;
      }
      break;
    default:
      break;
  }
  rl_src1 = LoadValueWide(rl_src1, kCoreReg);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  // NOTE: once we've done the EvalLoc on dest, we can no longer bail.
  switch (opcode) {
    case Instruction::ADD_LONG:
    case Instruction::ADD_LONG_2ADDR:
      NewLIR3(kThumb2AddRRI8, rl_result.low_reg, rl_src1.low_reg, mod_imm_lo);
      NewLIR3(kThumb2AdcRRI8, rl_result.high_reg, rl_src1.high_reg, mod_imm_hi);
      break;
    case Instruction::OR_LONG:
    case Instruction::OR_LONG_2ADDR:
      if ((val_lo != 0) || (rl_result.low_reg != rl_src1.low_reg)) {
        OpRegRegImm(kOpOr, rl_result.low_reg, rl_src1.low_reg, val_lo);
      }
      if ((val_hi != 0) || (rl_result.high_reg != rl_src1.high_reg)) {
        OpRegRegImm(kOpOr, rl_result.high_reg, rl_src1.high_reg, val_hi);
      }
      break;
    case Instruction::XOR_LONG:
    case Instruction::XOR_LONG_2ADDR:
      OpRegRegImm(kOpXor, rl_result.low_reg, rl_src1.low_reg, val_lo);
      OpRegRegImm(kOpXor, rl_result.high_reg, rl_src1.high_reg, val_hi);
      break;
    case Instruction::AND_LONG:
    case Instruction::AND_LONG_2ADDR:
      if ((val_lo != 0xffffffff) || (rl_result.low_reg != rl_src1.low_reg)) {
        OpRegRegImm(kOpAnd, rl_result.low_reg, rl_src1.low_reg, val_lo);
      }
      if ((val_hi != 0xffffffff) || (rl_result.high_reg != rl_src1.high_reg)) {
        OpRegRegImm(kOpAnd, rl_result.high_reg, rl_src1.high_reg, val_hi);
      }
      break;
    case Instruction::SUB_LONG_2ADDR:
    case Instruction::SUB_LONG:
      NewLIR3(kThumb2SubRRI8, rl_result.low_reg, rl_src1.low_reg, mod_imm_lo);
      NewLIR3(kThumb2SbcRRI8, rl_result.high_reg, rl_src1.high_reg, mod_imm_hi);
      break;
    default:
      LOG(FATAL) << "Unexpected opcode " << opcode;
  }
  StoreValueWide(rl_dest, rl_result);
}

}  // namespace art
