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

#include "arm_lir.h"
#include "codegen_arm.h"
#include "dex/quick/mir_to_lir-inl.h"

namespace art {

void ArmMir2Lir::GenArithOpFloat(Instruction::Code opcode, RegLocation rl_dest,
                                 RegLocation rl_src1, RegLocation rl_src2) {
  int op = kThumbBkpt;
  RegLocation rl_result;

  /*
   * Don't attempt to optimize register usage since these opcodes call out to
   * the handlers.
   */
  switch (opcode) {
    case Instruction::ADD_FLOAT_2ADDR:
    case Instruction::ADD_FLOAT:
      op = kThumb2Vadds;
      break;
    case Instruction::SUB_FLOAT_2ADDR:
    case Instruction::SUB_FLOAT:
      op = kThumb2Vsubs;
      break;
    case Instruction::DIV_FLOAT_2ADDR:
    case Instruction::DIV_FLOAT:
      op = kThumb2Vdivs;
      break;
    case Instruction::MUL_FLOAT_2ADDR:
    case Instruction::MUL_FLOAT:
      op = kThumb2Vmuls;
      break;
    case Instruction::REM_FLOAT_2ADDR:
    case Instruction::REM_FLOAT:
      FlushAllRegs();   // Send everything to home location
      CallRuntimeHelperRegLocationRegLocation(QUICK_ENTRYPOINT_OFFSET(pFmodf), rl_src1, rl_src2,
                                              false);
      rl_result = GetReturn(true);
      StoreValue(rl_dest, rl_result);
      return;
    case Instruction::NEG_FLOAT:
      GenNegFloat(rl_dest, rl_src1);
      return;
    default:
      LOG(FATAL) << "Unexpected opcode: " << opcode;
  }
  rl_src1 = LoadValue(rl_src1, kFPReg);
  rl_src2 = LoadValue(rl_src2, kFPReg);
  rl_result = EvalLoc(rl_dest, kFPReg, true);
  NewLIR3(op, rl_result.low_reg, rl_src1.low_reg, rl_src2.low_reg);
  StoreValue(rl_dest, rl_result);
}

void ArmMir2Lir::GenArithOpDouble(Instruction::Code opcode,
                                  RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2) {
  int op = kThumbBkpt;
  RegLocation rl_result;

  switch (opcode) {
    case Instruction::ADD_DOUBLE_2ADDR:
    case Instruction::ADD_DOUBLE:
      op = kThumb2Vaddd;
      break;
    case Instruction::SUB_DOUBLE_2ADDR:
    case Instruction::SUB_DOUBLE:
      op = kThumb2Vsubd;
      break;
    case Instruction::DIV_DOUBLE_2ADDR:
    case Instruction::DIV_DOUBLE:
      op = kThumb2Vdivd;
      break;
    case Instruction::MUL_DOUBLE_2ADDR:
    case Instruction::MUL_DOUBLE:
      op = kThumb2Vmuld;
      break;
    case Instruction::REM_DOUBLE_2ADDR:
    case Instruction::REM_DOUBLE:
      FlushAllRegs();   // Send everything to home location
      CallRuntimeHelperRegLocationRegLocation(QUICK_ENTRYPOINT_OFFSET(pFmod), rl_src1, rl_src2,
                                              false);
      rl_result = GetReturnWide(true);
      StoreValueWide(rl_dest, rl_result);
      return;
    case Instruction::NEG_DOUBLE:
      GenNegDouble(rl_dest, rl_src1);
      return;
    default:
      LOG(FATAL) << "Unexpected opcode: " << opcode;
  }

  rl_src1 = LoadValueWide(rl_src1, kFPReg);
  DCHECK(rl_src1.wide);
  rl_src2 = LoadValueWide(rl_src2, kFPReg);
  DCHECK(rl_src2.wide);
  rl_result = EvalLoc(rl_dest, kFPReg, true);
  DCHECK(rl_dest.wide);
  DCHECK(rl_result.wide);
  NewLIR3(op, S2d(rl_result.low_reg, rl_result.high_reg), S2d(rl_src1.low_reg, rl_src1.high_reg),
          S2d(rl_src2.low_reg, rl_src2.high_reg));
  StoreValueWide(rl_dest, rl_result);
}

void ArmMir2Lir::GenConversion(Instruction::Code opcode,
                               RegLocation rl_dest, RegLocation rl_src) {
  int op = kThumbBkpt;
  int src_reg;
  RegLocation rl_result;

  switch (opcode) {
    case Instruction::INT_TO_FLOAT:
      op = kThumb2VcvtIF;
      break;
    case Instruction::FLOAT_TO_INT:
      op = kThumb2VcvtFI;
      break;
    case Instruction::DOUBLE_TO_FLOAT:
      op = kThumb2VcvtDF;
      break;
    case Instruction::FLOAT_TO_DOUBLE:
      op = kThumb2VcvtFd;
      break;
    case Instruction::INT_TO_DOUBLE:
      op = kThumb2VcvtID;
      break;
    case Instruction::DOUBLE_TO_INT:
      op = kThumb2VcvtDI;
      break;
    case Instruction::LONG_TO_DOUBLE:
      GenConversionCall(QUICK_ENTRYPOINT_OFFSET(pL2d), rl_dest, rl_src);
      return;
    case Instruction::FLOAT_TO_LONG:
      GenConversionCall(QUICK_ENTRYPOINT_OFFSET(pF2l), rl_dest, rl_src);
      return;
    case Instruction::LONG_TO_FLOAT:
      GenConversionCall(QUICK_ENTRYPOINT_OFFSET(pL2f), rl_dest, rl_src);
      return;
    case Instruction::DOUBLE_TO_LONG:
      GenConversionCall(QUICK_ENTRYPOINT_OFFSET(pD2l), rl_dest, rl_src);
      return;
    default:
      LOG(FATAL) << "Unexpected opcode: " << opcode;
  }
  if (rl_src.wide) {
    rl_src = LoadValueWide(rl_src, kFPReg);
    src_reg = S2d(rl_src.low_reg, rl_src.high_reg);
  } else {
    rl_src = LoadValue(rl_src, kFPReg);
    src_reg = rl_src.low_reg;
  }
  if (rl_dest.wide) {
    rl_result = EvalLoc(rl_dest, kFPReg, true);
    NewLIR2(op, S2d(rl_result.low_reg, rl_result.high_reg), src_reg);
    StoreValueWide(rl_dest, rl_result);
  } else {
    rl_result = EvalLoc(rl_dest, kFPReg, true);
    NewLIR2(op, rl_result.low_reg, src_reg);
    StoreValue(rl_dest, rl_result);
  }
}

void ArmMir2Lir::GenFusedFPCmpBranch(BasicBlock* bb, MIR* mir, bool gt_bias,
                                     bool is_double) {
  LIR* target = &block_label_list_[bb->taken->id];
  RegLocation rl_src1;
  RegLocation rl_src2;
  if (is_double) {
    rl_src1 = mir_graph_->GetSrcWide(mir, 0);
    rl_src2 = mir_graph_->GetSrcWide(mir, 2);
    rl_src1 = LoadValueWide(rl_src1, kFPReg);
    rl_src2 = LoadValueWide(rl_src2, kFPReg);
    NewLIR2(kThumb2Vcmpd, S2d(rl_src1.low_reg, rl_src2.high_reg),
            S2d(rl_src2.low_reg, rl_src2.high_reg));
  } else {
    rl_src1 = mir_graph_->GetSrc(mir, 0);
    rl_src2 = mir_graph_->GetSrc(mir, 1);
    rl_src1 = LoadValue(rl_src1, kFPReg);
    rl_src2 = LoadValue(rl_src2, kFPReg);
    NewLIR2(kThumb2Vcmps, rl_src1.low_reg, rl_src2.low_reg);
  }
  NewLIR0(kThumb2Fmstat);
  ConditionCode ccode = static_cast<ConditionCode>(mir->dalvikInsn.arg[0]);
  switch (ccode) {
    case kCondEq:
    case kCondNe:
      break;
    case kCondLt:
      if (gt_bias) {
        ccode = kCondMi;
      }
      break;
    case kCondLe:
      if (gt_bias) {
        ccode = kCondLs;
      }
      break;
    case kCondGt:
      if (gt_bias) {
        ccode = kCondHi;
      }
      break;
    case kCondGe:
      if (gt_bias) {
        ccode = kCondCs;
      }
      break;
    default:
      LOG(FATAL) << "Unexpected ccode: " << ccode;
  }
  OpCondBranch(ccode, target);
}


void ArmMir2Lir::GenCmpFP(Instruction::Code opcode, RegLocation rl_dest,
                          RegLocation rl_src1, RegLocation rl_src2) {
  bool is_double = false;
  int default_result = -1;
  RegLocation rl_result;

  switch (opcode) {
    case Instruction::CMPL_FLOAT:
      is_double = false;
      default_result = -1;
      break;
    case Instruction::CMPG_FLOAT:
      is_double = false;
      default_result = 1;
      break;
    case Instruction::CMPL_DOUBLE:
      is_double = true;
      default_result = -1;
      break;
    case Instruction::CMPG_DOUBLE:
      is_double = true;
      default_result = 1;
      break;
    default:
      LOG(FATAL) << "Unexpected opcode: " << opcode;
  }
  if (is_double) {
    rl_src1 = LoadValueWide(rl_src1, kFPReg);
    rl_src2 = LoadValueWide(rl_src2, kFPReg);
    // In case result vreg is also a src vreg, break association to avoid useless copy by EvalLoc()
    ClobberSReg(rl_dest.s_reg_low);
    rl_result = EvalLoc(rl_dest, kCoreReg, true);
    LoadConstant(rl_result.low_reg, default_result);
    NewLIR2(kThumb2Vcmpd, S2d(rl_src1.low_reg, rl_src2.high_reg),
            S2d(rl_src2.low_reg, rl_src2.high_reg));
  } else {
    rl_src1 = LoadValue(rl_src1, kFPReg);
    rl_src2 = LoadValue(rl_src2, kFPReg);
    // In case result vreg is also a srcvreg, break association to avoid useless copy by EvalLoc()
    ClobberSReg(rl_dest.s_reg_low);
    rl_result = EvalLoc(rl_dest, kCoreReg, true);
    LoadConstant(rl_result.low_reg, default_result);
    NewLIR2(kThumb2Vcmps, rl_src1.low_reg, rl_src2.low_reg);
  }
  DCHECK(!ARM_FPREG(rl_result.low_reg));
  NewLIR0(kThumb2Fmstat);

  OpIT((default_result == -1) ? kCondGt : kCondMi, "");
  NewLIR2(kThumb2MovImmShift, rl_result.low_reg,
          ModifiedImmediate(-default_result));  // Must not alter ccodes
  GenBarrier();

  OpIT(kCondEq, "");
  LoadConstant(rl_result.low_reg, 0);
  GenBarrier();

  StoreValue(rl_dest, rl_result);
}

void ArmMir2Lir::GenNegFloat(RegLocation rl_dest, RegLocation rl_src) {
  RegLocation rl_result;
  rl_src = LoadValue(rl_src, kFPReg);
  rl_result = EvalLoc(rl_dest, kFPReg, true);
  NewLIR2(kThumb2Vnegs, rl_result.low_reg, rl_src.low_reg);
  StoreValue(rl_dest, rl_result);
}

void ArmMir2Lir::GenNegDouble(RegLocation rl_dest, RegLocation rl_src) {
  RegLocation rl_result;
  rl_src = LoadValueWide(rl_src, kFPReg);
  rl_result = EvalLoc(rl_dest, kFPReg, true);
  NewLIR2(kThumb2Vnegd, S2d(rl_result.low_reg, rl_result.high_reg),
          S2d(rl_src.low_reg, rl_src.high_reg));
  StoreValueWide(rl_dest, rl_result);
}

bool ArmMir2Lir::GenInlinedSqrt(CallInfo* info) {
  DCHECK_EQ(cu_->instruction_set, kThumb2);
  LIR *branch;
  RegLocation rl_src = info->args[0];
  RegLocation rl_dest = InlineTargetWide(info);  // double place for result
  rl_src = LoadValueWide(rl_src, kFPReg);
  RegLocation rl_result = EvalLoc(rl_dest, kFPReg, true);
  NewLIR2(kThumb2Vsqrtd, S2d(rl_result.low_reg, rl_result.high_reg),
          S2d(rl_src.low_reg, rl_src.high_reg));
  NewLIR2(kThumb2Vcmpd, S2d(rl_result.low_reg, rl_result.high_reg),
          S2d(rl_result.low_reg, rl_result.high_reg));
  NewLIR0(kThumb2Fmstat);
  branch = NewLIR2(kThumbBCond, 0, kArmCondEq);
  ClobberCalleeSave();
  LockCallTemps();  // Using fixed registers
  int r_tgt = LoadHelper(QUICK_ENTRYPOINT_OFFSET(pSqrt));
  NewLIR3(kThumb2Fmrrd, r0, r1, S2d(rl_src.low_reg, rl_src.high_reg));
  NewLIR1(kThumbBlxR, r_tgt);
  NewLIR3(kThumb2Fmdrr, S2d(rl_result.low_reg, rl_result.high_reg), r0, r1);
  branch->target = NewLIR0(kPseudoTargetLabel);
  StoreValueWide(rl_dest, rl_result);
  return true;
}


}  // namespace art
