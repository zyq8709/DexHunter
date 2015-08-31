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

#include "dex/compiler_ir.h"
#include "dex/compiler_internals.h"
#include "dex/quick/arm/arm_lir.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "mirror/array.h"
#include "verifier/method_verifier.h"

namespace art {

/*
 * This source files contains "gen" codegen routines that should
 * be applicable to most targets.  Only mid-level support utilities
 * and "op" calls may be used here.
 */

/*
 * Generate an kPseudoBarrier marker to indicate the boundary of special
 * blocks.
 */
void Mir2Lir::GenBarrier() {
  LIR* barrier = NewLIR0(kPseudoBarrier);
  /* Mark all resources as being clobbered */
  barrier->def_mask = -1;
}

// FIXME: need to do some work to split out targets with
// condition codes and those without
LIR* Mir2Lir::GenCheck(ConditionCode c_code, ThrowKind kind) {
  DCHECK_NE(cu_->instruction_set, kMips);
  LIR* tgt = RawLIR(0, kPseudoThrowTarget, kind, current_dalvik_offset_);
  LIR* branch = OpCondBranch(c_code, tgt);
  // Remember branch target - will process later
  throw_launchpads_.Insert(tgt);
  return branch;
}

LIR* Mir2Lir::GenImmedCheck(ConditionCode c_code, int reg, int imm_val, ThrowKind kind) {
  LIR* tgt = RawLIR(0, kPseudoThrowTarget, kind, current_dalvik_offset_, reg, imm_val);
  LIR* branch;
  if (c_code == kCondAl) {
    branch = OpUnconditionalBranch(tgt);
  } else {
    branch = OpCmpImmBranch(c_code, reg, imm_val, tgt);
  }
  // Remember branch target - will process later
  throw_launchpads_.Insert(tgt);
  return branch;
}

/* Perform null-check on a register.  */
LIR* Mir2Lir::GenNullCheck(int s_reg, int m_reg, int opt_flags) {
  if (!(cu_->disable_opt & (1 << kNullCheckElimination)) &&
    opt_flags & MIR_IGNORE_NULL_CHECK) {
    return NULL;
  }
  return GenImmedCheck(kCondEq, m_reg, 0, kThrowNullPointer);
}

/* Perform check on two registers */
LIR* Mir2Lir::GenRegRegCheck(ConditionCode c_code, int reg1, int reg2,
                             ThrowKind kind) {
  LIR* tgt = RawLIR(0, kPseudoThrowTarget, kind, current_dalvik_offset_, reg1, reg2);
  LIR* branch = OpCmpBranch(c_code, reg1, reg2, tgt);
  // Remember branch target - will process later
  throw_launchpads_.Insert(tgt);
  return branch;
}

void Mir2Lir::GenCompareAndBranch(Instruction::Code opcode, RegLocation rl_src1,
                                  RegLocation rl_src2, LIR* taken,
                                  LIR* fall_through) {
  ConditionCode cond;
  switch (opcode) {
    case Instruction::IF_EQ:
      cond = kCondEq;
      break;
    case Instruction::IF_NE:
      cond = kCondNe;
      break;
    case Instruction::IF_LT:
      cond = kCondLt;
      break;
    case Instruction::IF_GE:
      cond = kCondGe;
      break;
    case Instruction::IF_GT:
      cond = kCondGt;
      break;
    case Instruction::IF_LE:
      cond = kCondLe;
      break;
    default:
      cond = static_cast<ConditionCode>(0);
      LOG(FATAL) << "Unexpected opcode " << opcode;
  }

  // Normalize such that if either operand is constant, src2 will be constant
  if (rl_src1.is_const) {
    RegLocation rl_temp = rl_src1;
    rl_src1 = rl_src2;
    rl_src2 = rl_temp;
    cond = FlipComparisonOrder(cond);
  }

  rl_src1 = LoadValue(rl_src1, kCoreReg);
  // Is this really an immediate comparison?
  if (rl_src2.is_const) {
    // If it's already live in a register or not easily materialized, just keep going
    RegLocation rl_temp = UpdateLoc(rl_src2);
    if ((rl_temp.location == kLocDalvikFrame) &&
        InexpensiveConstantInt(mir_graph_->ConstantValue(rl_src2))) {
      // OK - convert this to a compare immediate and branch
      OpCmpImmBranch(cond, rl_src1.low_reg, mir_graph_->ConstantValue(rl_src2), taken);
      OpUnconditionalBranch(fall_through);
      return;
    }
  }
  rl_src2 = LoadValue(rl_src2, kCoreReg);
  OpCmpBranch(cond, rl_src1.low_reg, rl_src2.low_reg, taken);
  OpUnconditionalBranch(fall_through);
}

void Mir2Lir::GenCompareZeroAndBranch(Instruction::Code opcode, RegLocation rl_src, LIR* taken,
                                      LIR* fall_through) {
  ConditionCode cond;
  rl_src = LoadValue(rl_src, kCoreReg);
  switch (opcode) {
    case Instruction::IF_EQZ:
      cond = kCondEq;
      break;
    case Instruction::IF_NEZ:
      cond = kCondNe;
      break;
    case Instruction::IF_LTZ:
      cond = kCondLt;
      break;
    case Instruction::IF_GEZ:
      cond = kCondGe;
      break;
    case Instruction::IF_GTZ:
      cond = kCondGt;
      break;
    case Instruction::IF_LEZ:
      cond = kCondLe;
      break;
    default:
      cond = static_cast<ConditionCode>(0);
      LOG(FATAL) << "Unexpected opcode " << opcode;
  }
  OpCmpImmBranch(cond, rl_src.low_reg, 0, taken);
  OpUnconditionalBranch(fall_through);
}

void Mir2Lir::GenIntToLong(RegLocation rl_dest, RegLocation rl_src) {
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  if (rl_src.location == kLocPhysReg) {
    OpRegCopy(rl_result.low_reg, rl_src.low_reg);
  } else {
    LoadValueDirect(rl_src, rl_result.low_reg);
  }
  OpRegRegImm(kOpAsr, rl_result.high_reg, rl_result.low_reg, 31);
  StoreValueWide(rl_dest, rl_result);
}

void Mir2Lir::GenIntNarrowing(Instruction::Code opcode, RegLocation rl_dest,
                              RegLocation rl_src) {
  rl_src = LoadValue(rl_src, kCoreReg);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  OpKind op = kOpInvalid;
  switch (opcode) {
    case Instruction::INT_TO_BYTE:
      op = kOp2Byte;
      break;
    case Instruction::INT_TO_SHORT:
       op = kOp2Short;
       break;
    case Instruction::INT_TO_CHAR:
       op = kOp2Char;
       break;
    default:
      LOG(ERROR) << "Bad int conversion type";
  }
  OpRegReg(op, rl_result.low_reg, rl_src.low_reg);
  StoreValue(rl_dest, rl_result);
}

/*
 * Let helper function take care of everything.  Will call
 * Array::AllocFromCode(type_idx, method, count);
 * Note: AllocFromCode will handle checks for errNegativeArraySize.
 */
void Mir2Lir::GenNewArray(uint32_t type_idx, RegLocation rl_dest,
                          RegLocation rl_src) {
  FlushAllRegs();  /* Everything to home location */
  ThreadOffset func_offset(-1);
  if (cu_->compiler_driver->CanAccessTypeWithoutChecks(cu_->method_idx, *cu_->dex_file,
                                                       type_idx)) {
    func_offset = QUICK_ENTRYPOINT_OFFSET(pAllocArray);
  } else {
    func_offset= QUICK_ENTRYPOINT_OFFSET(pAllocArrayWithAccessCheck);
  }
  CallRuntimeHelperImmMethodRegLocation(func_offset, type_idx, rl_src, true);
  RegLocation rl_result = GetReturn(false);
  StoreValue(rl_dest, rl_result);
}

/*
 * Similar to GenNewArray, but with post-allocation initialization.
 * Verifier guarantees we're dealing with an array class.  Current
 * code throws runtime exception "bad Filled array req" for 'D' and 'J'.
 * Current code also throws internal unimp if not 'L', '[' or 'I'.
 */
void Mir2Lir::GenFilledNewArray(CallInfo* info) {
  int elems = info->num_arg_words;
  int type_idx = info->index;
  FlushAllRegs();  /* Everything to home location */
  ThreadOffset func_offset(-1);
  if (cu_->compiler_driver->CanAccessTypeWithoutChecks(cu_->method_idx, *cu_->dex_file,
                                                       type_idx)) {
    func_offset = QUICK_ENTRYPOINT_OFFSET(pCheckAndAllocArray);
  } else {
    func_offset = QUICK_ENTRYPOINT_OFFSET(pCheckAndAllocArrayWithAccessCheck);
  }
  CallRuntimeHelperImmMethodImm(func_offset, type_idx, elems, true);
  FreeTemp(TargetReg(kArg2));
  FreeTemp(TargetReg(kArg1));
  /*
   * NOTE: the implicit target for Instruction::FILLED_NEW_ARRAY is the
   * return region.  Because AllocFromCode placed the new array
   * in kRet0, we'll just lock it into place.  When debugger support is
   * added, it may be necessary to additionally copy all return
   * values to a home location in thread-local storage
   */
  LockTemp(TargetReg(kRet0));

  // TODO: use the correct component size, currently all supported types
  // share array alignment with ints (see comment at head of function)
  size_t component_size = sizeof(int32_t);

  // Having a range of 0 is legal
  if (info->is_range && (elems > 0)) {
    /*
     * Bit of ugliness here.  We're going generate a mem copy loop
     * on the register range, but it is possible that some regs
     * in the range have been promoted.  This is unlikely, but
     * before generating the copy, we'll just force a flush
     * of any regs in the source range that have been promoted to
     * home location.
     */
    for (int i = 0; i < elems; i++) {
      RegLocation loc = UpdateLoc(info->args[i]);
      if (loc.location == kLocPhysReg) {
        StoreBaseDisp(TargetReg(kSp), SRegOffset(loc.s_reg_low),
                      loc.low_reg, kWord);
      }
    }
    /*
     * TUNING note: generated code here could be much improved, but
     * this is an uncommon operation and isn't especially performance
     * critical.
     */
    int r_src = AllocTemp();
    int r_dst = AllocTemp();
    int r_idx = AllocTemp();
    int r_val = INVALID_REG;
    switch (cu_->instruction_set) {
      case kThumb2:
        r_val = TargetReg(kLr);
        break;
      case kX86:
        FreeTemp(TargetReg(kRet0));
        r_val = AllocTemp();
        break;
      case kMips:
        r_val = AllocTemp();
        break;
      default: LOG(FATAL) << "Unexpected instruction set: " << cu_->instruction_set;
    }
    // Set up source pointer
    RegLocation rl_first = info->args[0];
    OpRegRegImm(kOpAdd, r_src, TargetReg(kSp), SRegOffset(rl_first.s_reg_low));
    // Set up the target pointer
    OpRegRegImm(kOpAdd, r_dst, TargetReg(kRet0),
                mirror::Array::DataOffset(component_size).Int32Value());
    // Set up the loop counter (known to be > 0)
    LoadConstant(r_idx, elems - 1);
    // Generate the copy loop.  Going backwards for convenience
    LIR* target = NewLIR0(kPseudoTargetLabel);
    // Copy next element
    LoadBaseIndexed(r_src, r_idx, r_val, 2, kWord);
    StoreBaseIndexed(r_dst, r_idx, r_val, 2, kWord);
    FreeTemp(r_val);
    OpDecAndBranch(kCondGe, r_idx, target);
    if (cu_->instruction_set == kX86) {
      // Restore the target pointer
      OpRegRegImm(kOpAdd, TargetReg(kRet0), r_dst,
                  -mirror::Array::DataOffset(component_size).Int32Value());
    }
  } else if (!info->is_range) {
    // TUNING: interleave
    for (int i = 0; i < elems; i++) {
      RegLocation rl_arg = LoadValue(info->args[i], kCoreReg);
      StoreBaseDisp(TargetReg(kRet0),
                    mirror::Array::DataOffset(component_size).Int32Value() +
                    i * 4, rl_arg.low_reg, kWord);
      // If the LoadValue caused a temp to be allocated, free it
      if (IsTemp(rl_arg.low_reg)) {
        FreeTemp(rl_arg.low_reg);
      }
    }
  }
  if (info->result.location != kLocInvalid) {
    StoreValue(info->result, GetReturn(false /* not fp */));
  }
}

void Mir2Lir::GenSput(uint32_t field_idx, RegLocation rl_src, bool is_long_or_double,
                      bool is_object) {
  int field_offset;
  int ssb_index;
  bool is_volatile;
  bool is_referrers_class;
  bool fast_path = cu_->compiler_driver->ComputeStaticFieldInfo(
      field_idx, mir_graph_->GetCurrentDexCompilationUnit(), field_offset, ssb_index,
      is_referrers_class, is_volatile, true);
  if (fast_path && !SLOW_FIELD_PATH) {
    DCHECK_GE(field_offset, 0);
    int rBase;
    if (is_referrers_class) {
      // Fast path, static storage base is this method's class
      RegLocation rl_method  = LoadCurrMethod();
      rBase = AllocTemp();
      LoadWordDisp(rl_method.low_reg,
                   mirror::ArtMethod::DeclaringClassOffset().Int32Value(), rBase);
      if (IsTemp(rl_method.low_reg)) {
        FreeTemp(rl_method.low_reg);
      }
    } else {
      // Medium path, static storage base in a different class which requires checks that the other
      // class is initialized.
      // TODO: remove initialized check now that we are initializing classes in the compiler driver.
      DCHECK_GE(ssb_index, 0);
      // May do runtime call so everything to home locations.
      FlushAllRegs();
      // Using fixed register to sync with possible call to runtime support.
      int r_method = TargetReg(kArg1);
      LockTemp(r_method);
      LoadCurrMethodDirect(r_method);
      rBase = TargetReg(kArg0);
      LockTemp(rBase);
      LoadWordDisp(r_method,
                   mirror::ArtMethod::DexCacheInitializedStaticStorageOffset().Int32Value(),
                   rBase);
      LoadWordDisp(rBase,
                   mirror::Array::DataOffset(sizeof(mirror::Object*)).Int32Value() +
                   sizeof(int32_t*) * ssb_index, rBase);
      // rBase now points at appropriate static storage base (Class*)
      // or NULL if not initialized. Check for NULL and call helper if NULL.
      // TUNING: fast path should fall through
      LIR* branch_over = OpCmpImmBranch(kCondNe, rBase, 0, NULL);
      LoadConstant(TargetReg(kArg0), ssb_index);
      CallRuntimeHelperImm(QUICK_ENTRYPOINT_OFFSET(pInitializeStaticStorage), ssb_index, true);
      if (cu_->instruction_set == kMips) {
        // For Arm, kRet0 = kArg0 = rBase, for Mips, we need to copy
        OpRegCopy(rBase, TargetReg(kRet0));
      }
      LIR* skip_target = NewLIR0(kPseudoTargetLabel);
      branch_over->target = skip_target;
      FreeTemp(r_method);
    }
    // rBase now holds static storage base
    if (is_long_or_double) {
      rl_src = LoadValueWide(rl_src, kAnyReg);
    } else {
      rl_src = LoadValue(rl_src, kAnyReg);
    }
    if (is_volatile) {
      GenMemBarrier(kStoreStore);
    }
    if (is_long_or_double) {
      StoreBaseDispWide(rBase, field_offset, rl_src.low_reg,
                        rl_src.high_reg);
    } else {
      StoreWordDisp(rBase, field_offset, rl_src.low_reg);
    }
    if (is_volatile) {
      GenMemBarrier(kStoreLoad);
    }
    if (is_object && !mir_graph_->IsConstantNullRef(rl_src)) {
      MarkGCCard(rl_src.low_reg, rBase);
    }
    FreeTemp(rBase);
  } else {
    FlushAllRegs();  // Everything to home locations
    ThreadOffset setter_offset =
        is_long_or_double ? QUICK_ENTRYPOINT_OFFSET(pSet64Static)
                          : (is_object ? QUICK_ENTRYPOINT_OFFSET(pSetObjStatic)
                                       : QUICK_ENTRYPOINT_OFFSET(pSet32Static));
    CallRuntimeHelperImmRegLocation(setter_offset, field_idx, rl_src, true);
  }
}

void Mir2Lir::GenSget(uint32_t field_idx, RegLocation rl_dest,
                      bool is_long_or_double, bool is_object) {
  int field_offset;
  int ssb_index;
  bool is_volatile;
  bool is_referrers_class;
  bool fast_path = cu_->compiler_driver->ComputeStaticFieldInfo(
      field_idx, mir_graph_->GetCurrentDexCompilationUnit(), field_offset, ssb_index,
      is_referrers_class, is_volatile, false);
  if (fast_path && !SLOW_FIELD_PATH) {
    DCHECK_GE(field_offset, 0);
    int rBase;
    if (is_referrers_class) {
      // Fast path, static storage base is this method's class
      RegLocation rl_method  = LoadCurrMethod();
      rBase = AllocTemp();
      LoadWordDisp(rl_method.low_reg,
                   mirror::ArtMethod::DeclaringClassOffset().Int32Value(), rBase);
    } else {
      // Medium path, static storage base in a different class which requires checks that the other
      // class is initialized
      // TODO: remove initialized check now that we are initializing classes in the compiler driver.
      DCHECK_GE(ssb_index, 0);
      // May do runtime call so everything to home locations.
      FlushAllRegs();
      // Using fixed register to sync with possible call to runtime support.
      int r_method = TargetReg(kArg1);
      LockTemp(r_method);
      LoadCurrMethodDirect(r_method);
      rBase = TargetReg(kArg0);
      LockTemp(rBase);
      LoadWordDisp(r_method,
                   mirror::ArtMethod::DexCacheInitializedStaticStorageOffset().Int32Value(),
                   rBase);
      LoadWordDisp(rBase, mirror::Array::DataOffset(sizeof(mirror::Object*)).Int32Value() +
                   sizeof(int32_t*) * ssb_index, rBase);
      // rBase now points at appropriate static storage base (Class*)
      // or NULL if not initialized. Check for NULL and call helper if NULL.
      // TUNING: fast path should fall through
      LIR* branch_over = OpCmpImmBranch(kCondNe, rBase, 0, NULL);
      CallRuntimeHelperImm(QUICK_ENTRYPOINT_OFFSET(pInitializeStaticStorage), ssb_index, true);
      if (cu_->instruction_set == kMips) {
        // For Arm, kRet0 = kArg0 = rBase, for Mips, we need to copy
        OpRegCopy(rBase, TargetReg(kRet0));
      }
      LIR* skip_target = NewLIR0(kPseudoTargetLabel);
      branch_over->target = skip_target;
      FreeTemp(r_method);
    }
    // rBase now holds static storage base
    RegLocation rl_result = EvalLoc(rl_dest, kAnyReg, true);
    if (is_volatile) {
      GenMemBarrier(kLoadLoad);
    }
    if (is_long_or_double) {
      LoadBaseDispWide(rBase, field_offset, rl_result.low_reg,
                       rl_result.high_reg, INVALID_SREG);
    } else {
      LoadWordDisp(rBase, field_offset, rl_result.low_reg);
    }
    FreeTemp(rBase);
    if (is_long_or_double) {
      StoreValueWide(rl_dest, rl_result);
    } else {
      StoreValue(rl_dest, rl_result);
    }
  } else {
    FlushAllRegs();  // Everything to home locations
    ThreadOffset getterOffset =
        is_long_or_double ? QUICK_ENTRYPOINT_OFFSET(pGet64Static)
                          :(is_object ? QUICK_ENTRYPOINT_OFFSET(pGetObjStatic)
                                      : QUICK_ENTRYPOINT_OFFSET(pGet32Static));
    CallRuntimeHelperImm(getterOffset, field_idx, true);
    if (is_long_or_double) {
      RegLocation rl_result = GetReturnWide(rl_dest.fp);
      StoreValueWide(rl_dest, rl_result);
    } else {
      RegLocation rl_result = GetReturn(rl_dest.fp);
      StoreValue(rl_dest, rl_result);
    }
  }
}

void Mir2Lir::HandleSuspendLaunchPads() {
  int num_elems = suspend_launchpads_.Size();
  ThreadOffset helper_offset = QUICK_ENTRYPOINT_OFFSET(pTestSuspend);
  for (int i = 0; i < num_elems; i++) {
    ResetRegPool();
    ResetDefTracking();
    LIR* lab = suspend_launchpads_.Get(i);
    LIR* resume_lab = reinterpret_cast<LIR*>(lab->operands[0]);
    current_dalvik_offset_ = lab->operands[1];
    AppendLIR(lab);
    int r_tgt = CallHelperSetup(helper_offset);
    CallHelper(r_tgt, helper_offset, true /* MarkSafepointPC */);
    OpUnconditionalBranch(resume_lab);
  }
}

void Mir2Lir::HandleIntrinsicLaunchPads() {
  int num_elems = intrinsic_launchpads_.Size();
  for (int i = 0; i < num_elems; i++) {
    ResetRegPool();
    ResetDefTracking();
    LIR* lab = intrinsic_launchpads_.Get(i);
    CallInfo* info = reinterpret_cast<CallInfo*>(lab->operands[0]);
    current_dalvik_offset_ = info->offset;
    AppendLIR(lab);
    // NOTE: GenInvoke handles MarkSafepointPC
    GenInvoke(info);
    LIR* resume_lab = reinterpret_cast<LIR*>(lab->operands[2]);
    if (resume_lab != NULL) {
      OpUnconditionalBranch(resume_lab);
    }
  }
}

void Mir2Lir::HandleThrowLaunchPads() {
  int num_elems = throw_launchpads_.Size();
  for (int i = 0; i < num_elems; i++) {
    ResetRegPool();
    ResetDefTracking();
    LIR* lab = throw_launchpads_.Get(i);
    current_dalvik_offset_ = lab->operands[1];
    AppendLIR(lab);
    ThreadOffset func_offset(-1);
    int v1 = lab->operands[2];
    int v2 = lab->operands[3];
    const bool target_x86 = cu_->instruction_set == kX86;
    const bool target_arm = cu_->instruction_set == kArm || cu_->instruction_set == kThumb2;
    const bool target_mips = cu_->instruction_set == kMips;
    switch (lab->operands[0]) {
      case kThrowNullPointer:
        func_offset = QUICK_ENTRYPOINT_OFFSET(pThrowNullPointer);
        break;
      case kThrowConstantArrayBounds:  // v1 is length reg (for Arm/Mips), v2 constant index
        // v1 holds the constant array index.  Mips/Arm uses v2 for length, x86 reloads.
        if (target_x86) {
          OpRegMem(kOpMov, TargetReg(kArg1), v1, mirror::Array::LengthOffset().Int32Value());
        } else {
          OpRegCopy(TargetReg(kArg1), v1);
        }
        // Make sure the following LoadConstant doesn't mess with kArg1.
        LockTemp(TargetReg(kArg1));
        LoadConstant(TargetReg(kArg0), v2);
        func_offset = QUICK_ENTRYPOINT_OFFSET(pThrowArrayBounds);
        break;
      case kThrowArrayBounds:
        // Move v1 (array index) to kArg0 and v2 (array length) to kArg1
        if (v2 != TargetReg(kArg0)) {
          OpRegCopy(TargetReg(kArg0), v1);
          if (target_x86) {
            // x86 leaves the array pointer in v2, so load the array length that the handler expects
            OpRegMem(kOpMov, TargetReg(kArg1), v2, mirror::Array::LengthOffset().Int32Value());
          } else {
            OpRegCopy(TargetReg(kArg1), v2);
          }
        } else {
          if (v1 == TargetReg(kArg1)) {
            // Swap v1 and v2, using kArg2 as a temp
            OpRegCopy(TargetReg(kArg2), v1);
            if (target_x86) {
              // x86 leaves the array pointer in v2; load the array length that the handler expects
              OpRegMem(kOpMov, TargetReg(kArg1), v2, mirror::Array::LengthOffset().Int32Value());
            } else {
              OpRegCopy(TargetReg(kArg1), v2);
            }
            OpRegCopy(TargetReg(kArg0), TargetReg(kArg2));
          } else {
            if (target_x86) {
              // x86 leaves the array pointer in v2; load the array length that the handler expects
              OpRegMem(kOpMov, TargetReg(kArg1), v2, mirror::Array::LengthOffset().Int32Value());
            } else {
              OpRegCopy(TargetReg(kArg1), v2);
            }
            OpRegCopy(TargetReg(kArg0), v1);
          }
        }
        func_offset = QUICK_ENTRYPOINT_OFFSET(pThrowArrayBounds);
        break;
      case kThrowDivZero:
        func_offset = QUICK_ENTRYPOINT_OFFSET(pThrowDivZero);
        break;
      case kThrowNoSuchMethod:
        OpRegCopy(TargetReg(kArg0), v1);
        func_offset =
          QUICK_ENTRYPOINT_OFFSET(pThrowNoSuchMethod);
        break;
      case kThrowStackOverflow: {
        func_offset = QUICK_ENTRYPOINT_OFFSET(pThrowStackOverflow);
        // Restore stack alignment
        int r_tgt = 0;
        const int spill_size = (num_core_spills_ + num_fp_spills_) * 4;
        if (target_x86) {
          // - 4 to leave link register on stack.
          OpRegImm(kOpAdd, TargetReg(kSp), frame_size_ - 4);
          ClobberCalleeSave();
        } else if (target_arm) {
          r_tgt = r12;
          LoadWordDisp(TargetReg(kSp), spill_size - 4, TargetReg(kLr));
          OpRegImm(kOpAdd, TargetReg(kSp), spill_size);
          ClobberCalleeSave();
          LoadWordDisp(rARM_SELF, func_offset.Int32Value(), r_tgt);
        } else {
          DCHECK(target_mips);
          DCHECK_EQ(num_fp_spills_, 0);  // FP spills currently don't happen on mips.
          // LR is offset 0 since we push in reverse order.
          LoadWordDisp(TargetReg(kSp), 0, TargetReg(kLr));
          OpRegImm(kOpAdd, TargetReg(kSp), spill_size);
          ClobberCalleeSave();
          r_tgt = CallHelperSetup(func_offset);  // Doesn't clobber LR.
          DCHECK_NE(r_tgt, TargetReg(kLr));
        }
        CallHelper(r_tgt, func_offset, false /* MarkSafepointPC */, false /* UseLink */);
        continue;
      }
      default:
        LOG(FATAL) << "Unexpected throw kind: " << lab->operands[0];
    }
    ClobberCalleeSave();
    int r_tgt = CallHelperSetup(func_offset);
    CallHelper(r_tgt, func_offset, true /* MarkSafepointPC */, true /* UseLink */);
  }
}

void Mir2Lir::GenIGet(uint32_t field_idx, int opt_flags, OpSize size,
                      RegLocation rl_dest, RegLocation rl_obj, bool is_long_or_double,
                      bool is_object) {
  int field_offset;
  bool is_volatile;

  bool fast_path = FastInstance(field_idx, field_offset, is_volatile, false);

  if (fast_path && !SLOW_FIELD_PATH) {
    RegLocation rl_result;
    RegisterClass reg_class = oat_reg_class_by_size(size);
    DCHECK_GE(field_offset, 0);
    rl_obj = LoadValue(rl_obj, kCoreReg);
    if (is_long_or_double) {
      DCHECK(rl_dest.wide);
      GenNullCheck(rl_obj.s_reg_low, rl_obj.low_reg, opt_flags);
      if (cu_->instruction_set == kX86) {
        rl_result = EvalLoc(rl_dest, reg_class, true);
        GenNullCheck(rl_obj.s_reg_low, rl_obj.low_reg, opt_flags);
        LoadBaseDispWide(rl_obj.low_reg, field_offset, rl_result.low_reg,
                         rl_result.high_reg, rl_obj.s_reg_low);
        if (is_volatile) {
          GenMemBarrier(kLoadLoad);
        }
      } else {
        int reg_ptr = AllocTemp();
        OpRegRegImm(kOpAdd, reg_ptr, rl_obj.low_reg, field_offset);
        rl_result = EvalLoc(rl_dest, reg_class, true);
        LoadBaseDispWide(reg_ptr, 0, rl_result.low_reg, rl_result.high_reg, INVALID_SREG);
        if (is_volatile) {
          GenMemBarrier(kLoadLoad);
        }
        FreeTemp(reg_ptr);
      }
      StoreValueWide(rl_dest, rl_result);
    } else {
      rl_result = EvalLoc(rl_dest, reg_class, true);
      GenNullCheck(rl_obj.s_reg_low, rl_obj.low_reg, opt_flags);
      LoadBaseDisp(rl_obj.low_reg, field_offset, rl_result.low_reg,
                   kWord, rl_obj.s_reg_low);
      if (is_volatile) {
        GenMemBarrier(kLoadLoad);
      }
      StoreValue(rl_dest, rl_result);
    }
  } else {
    ThreadOffset getterOffset =
        is_long_or_double ? QUICK_ENTRYPOINT_OFFSET(pGet64Instance)
                          : (is_object ? QUICK_ENTRYPOINT_OFFSET(pGetObjInstance)
                                       : QUICK_ENTRYPOINT_OFFSET(pGet32Instance));
    CallRuntimeHelperImmRegLocation(getterOffset, field_idx, rl_obj, true);
    if (is_long_or_double) {
      RegLocation rl_result = GetReturnWide(rl_dest.fp);
      StoreValueWide(rl_dest, rl_result);
    } else {
      RegLocation rl_result = GetReturn(rl_dest.fp);
      StoreValue(rl_dest, rl_result);
    }
  }
}

void Mir2Lir::GenIPut(uint32_t field_idx, int opt_flags, OpSize size,
                      RegLocation rl_src, RegLocation rl_obj, bool is_long_or_double,
                      bool is_object) {
  int field_offset;
  bool is_volatile;

  bool fast_path = FastInstance(field_idx, field_offset, is_volatile,
                 true);
  if (fast_path && !SLOW_FIELD_PATH) {
    RegisterClass reg_class = oat_reg_class_by_size(size);
    DCHECK_GE(field_offset, 0);
    rl_obj = LoadValue(rl_obj, kCoreReg);
    if (is_long_or_double) {
      int reg_ptr;
      rl_src = LoadValueWide(rl_src, kAnyReg);
      GenNullCheck(rl_obj.s_reg_low, rl_obj.low_reg, opt_flags);
      reg_ptr = AllocTemp();
      OpRegRegImm(kOpAdd, reg_ptr, rl_obj.low_reg, field_offset);
      if (is_volatile) {
        GenMemBarrier(kStoreStore);
      }
      StoreBaseDispWide(reg_ptr, 0, rl_src.low_reg, rl_src.high_reg);
      if (is_volatile) {
        GenMemBarrier(kLoadLoad);
      }
      FreeTemp(reg_ptr);
    } else {
      rl_src = LoadValue(rl_src, reg_class);
      GenNullCheck(rl_obj.s_reg_low, rl_obj.low_reg, opt_flags);
      if (is_volatile) {
        GenMemBarrier(kStoreStore);
      }
      StoreBaseDisp(rl_obj.low_reg, field_offset, rl_src.low_reg, kWord);
      if (is_volatile) {
        GenMemBarrier(kLoadLoad);
      }
      if (is_object && !mir_graph_->IsConstantNullRef(rl_src)) {
        MarkGCCard(rl_src.low_reg, rl_obj.low_reg);
      }
    }
  } else {
    ThreadOffset setter_offset =
        is_long_or_double ? QUICK_ENTRYPOINT_OFFSET(pSet64Instance)
                          : (is_object ? QUICK_ENTRYPOINT_OFFSET(pSetObjInstance)
                                       : QUICK_ENTRYPOINT_OFFSET(pSet32Instance));
    CallRuntimeHelperImmRegLocationRegLocation(setter_offset, field_idx, rl_obj, rl_src, true);
  }
}

void Mir2Lir::GenConstClass(uint32_t type_idx, RegLocation rl_dest) {
  RegLocation rl_method = LoadCurrMethod();
  int res_reg = AllocTemp();
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  if (!cu_->compiler_driver->CanAccessTypeWithoutChecks(cu_->method_idx,
                                                   *cu_->dex_file,
                                                   type_idx)) {
    // Call out to helper which resolves type and verifies access.
    // Resolved type returned in kRet0.
    CallRuntimeHelperImmReg(QUICK_ENTRYPOINT_OFFSET(pInitializeTypeAndVerifyAccess),
                            type_idx, rl_method.low_reg, true);
    RegLocation rl_result = GetReturn(false);
    StoreValue(rl_dest, rl_result);
  } else {
    // We're don't need access checks, load type from dex cache
    int32_t dex_cache_offset =
        mirror::ArtMethod::DexCacheResolvedTypesOffset().Int32Value();
    LoadWordDisp(rl_method.low_reg, dex_cache_offset, res_reg);
    int32_t offset_of_type =
        mirror::Array::DataOffset(sizeof(mirror::Class*)).Int32Value() + (sizeof(mirror::Class*)
                          * type_idx);
    LoadWordDisp(res_reg, offset_of_type, rl_result.low_reg);
    if (!cu_->compiler_driver->CanAssumeTypeIsPresentInDexCache(*cu_->dex_file,
        type_idx) || SLOW_TYPE_PATH) {
      // Slow path, at runtime test if type is null and if so initialize
      FlushAllRegs();
      LIR* branch1 = OpCmpImmBranch(kCondEq, rl_result.low_reg, 0, NULL);
      // Resolved, store and hop over following code
      StoreValue(rl_dest, rl_result);
      /*
       * Because we have stores of the target value on two paths,
       * clobber temp tracking for the destination using the ssa name
       */
      ClobberSReg(rl_dest.s_reg_low);
      LIR* branch2 = OpUnconditionalBranch(0);
      // TUNING: move slow path to end & remove unconditional branch
      LIR* target1 = NewLIR0(kPseudoTargetLabel);
      // Call out to helper, which will return resolved type in kArg0
      CallRuntimeHelperImmReg(QUICK_ENTRYPOINT_OFFSET(pInitializeType), type_idx,
                              rl_method.low_reg, true);
      RegLocation rl_result = GetReturn(false);
      StoreValue(rl_dest, rl_result);
      /*
       * Because we have stores of the target value on two paths,
       * clobber temp tracking for the destination using the ssa name
       */
      ClobberSReg(rl_dest.s_reg_low);
      // Rejoin code paths
      LIR* target2 = NewLIR0(kPseudoTargetLabel);
      branch1->target = target1;
      branch2->target = target2;
    } else {
      // Fast path, we're done - just store result
      StoreValue(rl_dest, rl_result);
    }
  }
}

void Mir2Lir::GenConstString(uint32_t string_idx, RegLocation rl_dest) {
  /* NOTE: Most strings should be available at compile time */
  int32_t offset_of_string = mirror::Array::DataOffset(sizeof(mirror::String*)).Int32Value() +
                 (sizeof(mirror::String*) * string_idx);
  if (!cu_->compiler_driver->CanAssumeStringIsPresentInDexCache(
      *cu_->dex_file, string_idx) || SLOW_STRING_PATH) {
    // slow path, resolve string if not in dex cache
    FlushAllRegs();
    LockCallTemps();  // Using explicit registers
    LoadCurrMethodDirect(TargetReg(kArg2));
    LoadWordDisp(TargetReg(kArg2),
                 mirror::ArtMethod::DexCacheStringsOffset().Int32Value(), TargetReg(kArg0));
    // Might call out to helper, which will return resolved string in kRet0
    int r_tgt = CallHelperSetup(QUICK_ENTRYPOINT_OFFSET(pResolveString));
    LoadWordDisp(TargetReg(kArg0), offset_of_string, TargetReg(kRet0));
    LoadConstant(TargetReg(kArg1), string_idx);
    if (cu_->instruction_set == kThumb2) {
      OpRegImm(kOpCmp, TargetReg(kRet0), 0);  // Is resolved?
      GenBarrier();
      // For testing, always force through helper
      if (!EXERCISE_SLOWEST_STRING_PATH) {
        OpIT(kCondEq, "T");
      }
      OpRegCopy(TargetReg(kArg0), TargetReg(kArg2));   // .eq
      LIR* call_inst = OpReg(kOpBlx, r_tgt);    // .eq, helper(Method*, string_idx)
      MarkSafepointPC(call_inst);
      FreeTemp(r_tgt);
    } else if (cu_->instruction_set == kMips) {
      LIR* branch = OpCmpImmBranch(kCondNe, TargetReg(kRet0), 0, NULL);
      OpRegCopy(TargetReg(kArg0), TargetReg(kArg2));   // .eq
      LIR* call_inst = OpReg(kOpBlx, r_tgt);
      MarkSafepointPC(call_inst);
      FreeTemp(r_tgt);
      LIR* target = NewLIR0(kPseudoTargetLabel);
      branch->target = target;
    } else {
      DCHECK_EQ(cu_->instruction_set, kX86);
      CallRuntimeHelperRegReg(QUICK_ENTRYPOINT_OFFSET(pResolveString), TargetReg(kArg2),
                              TargetReg(kArg1), true);
    }
    GenBarrier();
    StoreValue(rl_dest, GetReturn(false));
  } else {
    RegLocation rl_method = LoadCurrMethod();
    int res_reg = AllocTemp();
    RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
    LoadWordDisp(rl_method.low_reg,
                 mirror::ArtMethod::DexCacheStringsOffset().Int32Value(), res_reg);
    LoadWordDisp(res_reg, offset_of_string, rl_result.low_reg);
    StoreValue(rl_dest, rl_result);
  }
}

/*
 * Let helper function take care of everything.  Will
 * call Class::NewInstanceFromCode(type_idx, method);
 */
void Mir2Lir::GenNewInstance(uint32_t type_idx, RegLocation rl_dest) {
  FlushAllRegs();  /* Everything to home location */
  // alloc will always check for resolution, do we also need to verify
  // access because the verifier was unable to?
  ThreadOffset func_offset(-1);
  if (cu_->compiler_driver->CanAccessInstantiableTypeWithoutChecks(
      cu_->method_idx, *cu_->dex_file, type_idx)) {
    func_offset = QUICK_ENTRYPOINT_OFFSET(pAllocObject);
  } else {
    func_offset = QUICK_ENTRYPOINT_OFFSET(pAllocObjectWithAccessCheck);
  }
  CallRuntimeHelperImmMethod(func_offset, type_idx, true);
  RegLocation rl_result = GetReturn(false);
  StoreValue(rl_dest, rl_result);
}

void Mir2Lir::GenThrow(RegLocation rl_src) {
  FlushAllRegs();
  CallRuntimeHelperRegLocation(QUICK_ENTRYPOINT_OFFSET(pDeliverException), rl_src, true);
}

// For final classes there are no sub-classes to check and so we can answer the instance-of
// question with simple comparisons.
void Mir2Lir::GenInstanceofFinal(bool use_declaring_class, uint32_t type_idx, RegLocation rl_dest,
                                 RegLocation rl_src) {
  RegLocation object = LoadValue(rl_src, kCoreReg);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  int result_reg = rl_result.low_reg;
  if (result_reg == object.low_reg) {
    result_reg = AllocTypedTemp(false, kCoreReg);
  }
  LoadConstant(result_reg, 0);     // assume false
  LIR* null_branchover = OpCmpImmBranch(kCondEq, object.low_reg, 0, NULL);

  int check_class = AllocTypedTemp(false, kCoreReg);
  int object_class = AllocTypedTemp(false, kCoreReg);

  LoadCurrMethodDirect(check_class);
  if (use_declaring_class) {
    LoadWordDisp(check_class, mirror::ArtMethod::DeclaringClassOffset().Int32Value(),
                 check_class);
    LoadWordDisp(object.low_reg,  mirror::Object::ClassOffset().Int32Value(), object_class);
  } else {
    LoadWordDisp(check_class, mirror::ArtMethod::DexCacheResolvedTypesOffset().Int32Value(),
                 check_class);
    LoadWordDisp(object.low_reg,  mirror::Object::ClassOffset().Int32Value(), object_class);
    int32_t offset_of_type =
      mirror::Array::DataOffset(sizeof(mirror::Class*)).Int32Value() +
      (sizeof(mirror::Class*) * type_idx);
    LoadWordDisp(check_class, offset_of_type, check_class);
  }

  LIR* ne_branchover = NULL;
  if (cu_->instruction_set == kThumb2) {
    OpRegReg(kOpCmp, check_class, object_class);  // Same?
    OpIT(kCondEq, "");   // if-convert the test
    LoadConstant(result_reg, 1);     // .eq case - load true
  } else {
    ne_branchover = OpCmpBranch(kCondNe, check_class, object_class, NULL);
    LoadConstant(result_reg, 1);     // eq case - load true
  }
  LIR* target = NewLIR0(kPseudoTargetLabel);
  null_branchover->target = target;
  if (ne_branchover != NULL) {
    ne_branchover->target = target;
  }
  FreeTemp(object_class);
  FreeTemp(check_class);
  if (IsTemp(result_reg)) {
    OpRegCopy(rl_result.low_reg, result_reg);
    FreeTemp(result_reg);
  }
  StoreValue(rl_dest, rl_result);
}

void Mir2Lir::GenInstanceofCallingHelper(bool needs_access_check, bool type_known_final,
                                         bool type_known_abstract, bool use_declaring_class,
                                         bool can_assume_type_is_in_dex_cache,
                                         uint32_t type_idx, RegLocation rl_dest,
                                         RegLocation rl_src) {
  FlushAllRegs();
  // May generate a call - use explicit registers
  LockCallTemps();
  LoadCurrMethodDirect(TargetReg(kArg1));  // kArg1 <= current Method*
  int class_reg = TargetReg(kArg2);  // kArg2 will hold the Class*
  if (needs_access_check) {
    // Check we have access to type_idx and if not throw IllegalAccessError,
    // returns Class* in kArg0
    CallRuntimeHelperImm(QUICK_ENTRYPOINT_OFFSET(pInitializeTypeAndVerifyAccess),
                         type_idx, true);
    OpRegCopy(class_reg, TargetReg(kRet0));  // Align usage with fast path
    LoadValueDirectFixed(rl_src, TargetReg(kArg0));  // kArg0 <= ref
  } else if (use_declaring_class) {
    LoadValueDirectFixed(rl_src, TargetReg(kArg0));  // kArg0 <= ref
    LoadWordDisp(TargetReg(kArg1),
                 mirror::ArtMethod::DeclaringClassOffset().Int32Value(), class_reg);
  } else {
    // Load dex cache entry into class_reg (kArg2)
    LoadValueDirectFixed(rl_src, TargetReg(kArg0));  // kArg0 <= ref
    LoadWordDisp(TargetReg(kArg1),
                 mirror::ArtMethod::DexCacheResolvedTypesOffset().Int32Value(), class_reg);
    int32_t offset_of_type =
        mirror::Array::DataOffset(sizeof(mirror::Class*)).Int32Value() + (sizeof(mirror::Class*)
        * type_idx);
    LoadWordDisp(class_reg, offset_of_type, class_reg);
    if (!can_assume_type_is_in_dex_cache) {
      // Need to test presence of type in dex cache at runtime
      LIR* hop_branch = OpCmpImmBranch(kCondNe, class_reg, 0, NULL);
      // Not resolved
      // Call out to helper, which will return resolved type in kRet0
      CallRuntimeHelperImm(QUICK_ENTRYPOINT_OFFSET(pInitializeType), type_idx, true);
      OpRegCopy(TargetReg(kArg2), TargetReg(kRet0));  // Align usage with fast path
      LoadValueDirectFixed(rl_src, TargetReg(kArg0));  /* reload Ref */
      // Rejoin code paths
      LIR* hop_target = NewLIR0(kPseudoTargetLabel);
      hop_branch->target = hop_target;
    }
  }
  /* kArg0 is ref, kArg2 is class. If ref==null, use directly as bool result */
  RegLocation rl_result = GetReturn(false);
  if (cu_->instruction_set == kMips) {
    // On MIPS rArg0 != rl_result, place false in result if branch is taken.
    LoadConstant(rl_result.low_reg, 0);
  }
  LIR* branch1 = OpCmpImmBranch(kCondEq, TargetReg(kArg0), 0, NULL);

  /* load object->klass_ */
  DCHECK_EQ(mirror::Object::ClassOffset().Int32Value(), 0);
  LoadWordDisp(TargetReg(kArg0),  mirror::Object::ClassOffset().Int32Value(), TargetReg(kArg1));
  /* kArg0 is ref, kArg1 is ref->klass_, kArg2 is class */
  LIR* branchover = NULL;
  if (type_known_final) {
    // rl_result == ref == null == 0.
    if (cu_->instruction_set == kThumb2) {
      OpRegReg(kOpCmp, TargetReg(kArg1), TargetReg(kArg2));  // Same?
      OpIT(kCondEq, "E");   // if-convert the test
      LoadConstant(rl_result.low_reg, 1);     // .eq case - load true
      LoadConstant(rl_result.low_reg, 0);     // .ne case - load false
    } else {
      LoadConstant(rl_result.low_reg, 0);     // ne case - load false
      branchover = OpCmpBranch(kCondNe, TargetReg(kArg1), TargetReg(kArg2), NULL);
      LoadConstant(rl_result.low_reg, 1);     // eq case - load true
    }
  } else {
    if (cu_->instruction_set == kThumb2) {
      int r_tgt = LoadHelper(QUICK_ENTRYPOINT_OFFSET(pInstanceofNonTrivial));
      if (!type_known_abstract) {
      /* Uses conditional nullification */
        OpRegReg(kOpCmp, TargetReg(kArg1), TargetReg(kArg2));  // Same?
        OpIT(kCondEq, "EE");   // if-convert the test
        LoadConstant(TargetReg(kArg0), 1);     // .eq case - load true
      }
      OpRegCopy(TargetReg(kArg0), TargetReg(kArg2));    // .ne case - arg0 <= class
      OpReg(kOpBlx, r_tgt);    // .ne case: helper(class, ref->class)
      FreeTemp(r_tgt);
    } else {
      if (!type_known_abstract) {
        /* Uses branchovers */
        LoadConstant(rl_result.low_reg, 1);     // assume true
        branchover = OpCmpBranch(kCondEq, TargetReg(kArg1), TargetReg(kArg2), NULL);
      }
      if (cu_->instruction_set != kX86) {
        int r_tgt = LoadHelper(QUICK_ENTRYPOINT_OFFSET(pInstanceofNonTrivial));
        OpRegCopy(TargetReg(kArg0), TargetReg(kArg2));    // .ne case - arg0 <= class
        OpReg(kOpBlx, r_tgt);    // .ne case: helper(class, ref->class)
        FreeTemp(r_tgt);
      } else {
        OpRegCopy(TargetReg(kArg0), TargetReg(kArg2));
        OpThreadMem(kOpBlx, QUICK_ENTRYPOINT_OFFSET(pInstanceofNonTrivial));
      }
    }
  }
  // TODO: only clobber when type isn't final?
  ClobberCalleeSave();
  /* branch targets here */
  LIR* target = NewLIR0(kPseudoTargetLabel);
  StoreValue(rl_dest, rl_result);
  branch1->target = target;
  if (branchover != NULL) {
    branchover->target = target;
  }
}

void Mir2Lir::GenInstanceof(uint32_t type_idx, RegLocation rl_dest, RegLocation rl_src) {
  bool type_known_final, type_known_abstract, use_declaring_class;
  bool needs_access_check = !cu_->compiler_driver->CanAccessTypeWithoutChecks(cu_->method_idx,
                                                                              *cu_->dex_file,
                                                                              type_idx,
                                                                              &type_known_final,
                                                                              &type_known_abstract,
                                                                              &use_declaring_class);
  bool can_assume_type_is_in_dex_cache = !needs_access_check &&
      cu_->compiler_driver->CanAssumeTypeIsPresentInDexCache(*cu_->dex_file, type_idx);

  if ((use_declaring_class || can_assume_type_is_in_dex_cache) && type_known_final) {
    GenInstanceofFinal(use_declaring_class, type_idx, rl_dest, rl_src);
  } else {
    GenInstanceofCallingHelper(needs_access_check, type_known_final, type_known_abstract,
                               use_declaring_class, can_assume_type_is_in_dex_cache,
                               type_idx, rl_dest, rl_src);
  }
}

void Mir2Lir::GenCheckCast(uint32_t insn_idx, uint32_t type_idx, RegLocation rl_src) {
  bool type_known_final, type_known_abstract, use_declaring_class;
  bool needs_access_check = !cu_->compiler_driver->CanAccessTypeWithoutChecks(cu_->method_idx,
                                                                              *cu_->dex_file,
                                                                              type_idx,
                                                                              &type_known_final,
                                                                              &type_known_abstract,
                                                                              &use_declaring_class);
  // Note: currently type_known_final is unused, as optimizing will only improve the performance
  // of the exception throw path.
  DexCompilationUnit* cu = mir_graph_->GetCurrentDexCompilationUnit();
  const MethodReference mr(cu->GetDexFile(), cu->GetDexMethodIndex());
  if (!needs_access_check && cu_->compiler_driver->IsSafeCast(mr, insn_idx)) {
    // Verifier type analysis proved this check cast would never cause an exception.
    return;
  }
  FlushAllRegs();
  // May generate a call - use explicit registers
  LockCallTemps();
  LoadCurrMethodDirect(TargetReg(kArg1));  // kArg1 <= current Method*
  int class_reg = TargetReg(kArg2);  // kArg2 will hold the Class*
  if (needs_access_check) {
    // Check we have access to type_idx and if not throw IllegalAccessError,
    // returns Class* in kRet0
    // InitializeTypeAndVerifyAccess(idx, method)
    CallRuntimeHelperImmReg(QUICK_ENTRYPOINT_OFFSET(pInitializeTypeAndVerifyAccess),
                            type_idx, TargetReg(kArg1), true);
    OpRegCopy(class_reg, TargetReg(kRet0));  // Align usage with fast path
  } else if (use_declaring_class) {
    LoadWordDisp(TargetReg(kArg1),
                 mirror::ArtMethod::DeclaringClassOffset().Int32Value(), class_reg);
  } else {
    // Load dex cache entry into class_reg (kArg2)
    LoadWordDisp(TargetReg(kArg1),
                 mirror::ArtMethod::DexCacheResolvedTypesOffset().Int32Value(), class_reg);
    int32_t offset_of_type =
        mirror::Array::DataOffset(sizeof(mirror::Class*)).Int32Value() +
        (sizeof(mirror::Class*) * type_idx);
    LoadWordDisp(class_reg, offset_of_type, class_reg);
    if (!cu_->compiler_driver->CanAssumeTypeIsPresentInDexCache(*cu_->dex_file, type_idx)) {
      // Need to test presence of type in dex cache at runtime
      LIR* hop_branch = OpCmpImmBranch(kCondNe, class_reg, 0, NULL);
      // Not resolved
      // Call out to helper, which will return resolved type in kArg0
      // InitializeTypeFromCode(idx, method)
      CallRuntimeHelperImmReg(QUICK_ENTRYPOINT_OFFSET(pInitializeType), type_idx,
                              TargetReg(kArg1), true);
      OpRegCopy(class_reg, TargetReg(kRet0));  // Align usage with fast path
      // Rejoin code paths
      LIR* hop_target = NewLIR0(kPseudoTargetLabel);
      hop_branch->target = hop_target;
    }
  }
  // At this point, class_reg (kArg2) has class
  LoadValueDirectFixed(rl_src, TargetReg(kArg0));  // kArg0 <= ref
  /* Null is OK - continue */
  LIR* branch1 = OpCmpImmBranch(kCondEq, TargetReg(kArg0), 0, NULL);
  /* load object->klass_ */
  DCHECK_EQ(mirror::Object::ClassOffset().Int32Value(), 0);
  LoadWordDisp(TargetReg(kArg0), mirror::Object::ClassOffset().Int32Value(), TargetReg(kArg1));
  /* kArg1 now contains object->klass_ */
  LIR* branch2 = NULL;
  if (!type_known_abstract) {
    branch2 = OpCmpBranch(kCondEq, TargetReg(kArg1), class_reg, NULL);
  }
  CallRuntimeHelperRegReg(QUICK_ENTRYPOINT_OFFSET(pCheckCast), TargetReg(kArg1),
                          TargetReg(kArg2), true);
  /* branch target here */
  LIR* target = NewLIR0(kPseudoTargetLabel);
  branch1->target = target;
  if (branch2 != NULL) {
    branch2->target = target;
  }
}

void Mir2Lir::GenLong3Addr(OpKind first_op, OpKind second_op, RegLocation rl_dest,
                           RegLocation rl_src1, RegLocation rl_src2) {
  RegLocation rl_result;
  if (cu_->instruction_set == kThumb2) {
    /*
     * NOTE:  This is the one place in the code in which we might have
     * as many as six live temporary registers.  There are 5 in the normal
     * set for Arm.  Until we have spill capabilities, temporarily add
     * lr to the temp set.  It is safe to do this locally, but note that
     * lr is used explicitly elsewhere in the code generator and cannot
     * normally be used as a general temp register.
     */
    MarkTemp(TargetReg(kLr));   // Add lr to the temp pool
    FreeTemp(TargetReg(kLr));   // and make it available
  }
  rl_src1 = LoadValueWide(rl_src1, kCoreReg);
  rl_src2 = LoadValueWide(rl_src2, kCoreReg);
  rl_result = EvalLoc(rl_dest, kCoreReg, true);
  // The longs may overlap - use intermediate temp if so
  if ((rl_result.low_reg == rl_src1.high_reg) || (rl_result.low_reg == rl_src2.high_reg)) {
    int t_reg = AllocTemp();
    OpRegRegReg(first_op, t_reg, rl_src1.low_reg, rl_src2.low_reg);
    OpRegRegReg(second_op, rl_result.high_reg, rl_src1.high_reg, rl_src2.high_reg);
    OpRegCopy(rl_result.low_reg, t_reg);
    FreeTemp(t_reg);
  } else {
    OpRegRegReg(first_op, rl_result.low_reg, rl_src1.low_reg, rl_src2.low_reg);
    OpRegRegReg(second_op, rl_result.high_reg, rl_src1.high_reg,
                rl_src2.high_reg);
  }
  /*
   * NOTE: If rl_dest refers to a frame variable in a large frame, the
   * following StoreValueWide might need to allocate a temp register.
   * To further work around the lack of a spill capability, explicitly
   * free any temps from rl_src1 & rl_src2 that aren't still live in rl_result.
   * Remove when spill is functional.
   */
  FreeRegLocTemps(rl_result, rl_src1);
  FreeRegLocTemps(rl_result, rl_src2);
  StoreValueWide(rl_dest, rl_result);
  if (cu_->instruction_set == kThumb2) {
    Clobber(TargetReg(kLr));
    UnmarkTemp(TargetReg(kLr));  // Remove lr from the temp pool
  }
}


void Mir2Lir::GenShiftOpLong(Instruction::Code opcode, RegLocation rl_dest,
                             RegLocation rl_src1, RegLocation rl_shift) {
  ThreadOffset func_offset(-1);

  switch (opcode) {
    case Instruction::SHL_LONG:
    case Instruction::SHL_LONG_2ADDR:
      func_offset = QUICK_ENTRYPOINT_OFFSET(pShlLong);
      break;
    case Instruction::SHR_LONG:
    case Instruction::SHR_LONG_2ADDR:
      func_offset = QUICK_ENTRYPOINT_OFFSET(pShrLong);
      break;
    case Instruction::USHR_LONG:
    case Instruction::USHR_LONG_2ADDR:
      func_offset = QUICK_ENTRYPOINT_OFFSET(pUshrLong);
      break;
    default:
      LOG(FATAL) << "Unexpected case";
  }
  FlushAllRegs();   /* Send everything to home location */
  CallRuntimeHelperRegLocationRegLocation(func_offset, rl_src1, rl_shift, false);
  RegLocation rl_result = GetReturnWide(false);
  StoreValueWide(rl_dest, rl_result);
}


void Mir2Lir::GenArithOpInt(Instruction::Code opcode, RegLocation rl_dest,
                            RegLocation rl_src1, RegLocation rl_src2) {
  OpKind op = kOpBkpt;
  bool is_div_rem = false;
  bool check_zero = false;
  bool unary = false;
  RegLocation rl_result;
  bool shift_op = false;
  switch (opcode) {
    case Instruction::NEG_INT:
      op = kOpNeg;
      unary = true;
      break;
    case Instruction::NOT_INT:
      op = kOpMvn;
      unary = true;
      break;
    case Instruction::ADD_INT:
    case Instruction::ADD_INT_2ADDR:
      op = kOpAdd;
      break;
    case Instruction::SUB_INT:
    case Instruction::SUB_INT_2ADDR:
      op = kOpSub;
      break;
    case Instruction::MUL_INT:
    case Instruction::MUL_INT_2ADDR:
      op = kOpMul;
      break;
    case Instruction::DIV_INT:
    case Instruction::DIV_INT_2ADDR:
      check_zero = true;
      op = kOpDiv;
      is_div_rem = true;
      break;
    /* NOTE: returns in kArg1 */
    case Instruction::REM_INT:
    case Instruction::REM_INT_2ADDR:
      check_zero = true;
      op = kOpRem;
      is_div_rem = true;
      break;
    case Instruction::AND_INT:
    case Instruction::AND_INT_2ADDR:
      op = kOpAnd;
      break;
    case Instruction::OR_INT:
    case Instruction::OR_INT_2ADDR:
      op = kOpOr;
      break;
    case Instruction::XOR_INT:
    case Instruction::XOR_INT_2ADDR:
      op = kOpXor;
      break;
    case Instruction::SHL_INT:
    case Instruction::SHL_INT_2ADDR:
      shift_op = true;
      op = kOpLsl;
      break;
    case Instruction::SHR_INT:
    case Instruction::SHR_INT_2ADDR:
      shift_op = true;
      op = kOpAsr;
      break;
    case Instruction::USHR_INT:
    case Instruction::USHR_INT_2ADDR:
      shift_op = true;
      op = kOpLsr;
      break;
    default:
      LOG(FATAL) << "Invalid word arith op: " << opcode;
  }
  if (!is_div_rem) {
    if (unary) {
      rl_src1 = LoadValue(rl_src1, kCoreReg);
      rl_result = EvalLoc(rl_dest, kCoreReg, true);
      OpRegReg(op, rl_result.low_reg, rl_src1.low_reg);
    } else {
      if (shift_op) {
        int t_reg = INVALID_REG;
        if (cu_->instruction_set == kX86) {
          // X86 doesn't require masking and must use ECX
          t_reg = TargetReg(kCount);  // rCX
          LoadValueDirectFixed(rl_src2, t_reg);
        } else {
          rl_src2 = LoadValue(rl_src2, kCoreReg);
          t_reg = AllocTemp();
          OpRegRegImm(kOpAnd, t_reg, rl_src2.low_reg, 31);
        }
        rl_src1 = LoadValue(rl_src1, kCoreReg);
        rl_result = EvalLoc(rl_dest, kCoreReg, true);
        OpRegRegReg(op, rl_result.low_reg, rl_src1.low_reg, t_reg);
        FreeTemp(t_reg);
      } else {
        rl_src1 = LoadValue(rl_src1, kCoreReg);
        rl_src2 = LoadValue(rl_src2, kCoreReg);
        rl_result = EvalLoc(rl_dest, kCoreReg, true);
        OpRegRegReg(op, rl_result.low_reg, rl_src1.low_reg, rl_src2.low_reg);
      }
    }
    StoreValue(rl_dest, rl_result);
  } else {
    if (cu_->instruction_set == kMips) {
      rl_src1 = LoadValue(rl_src1, kCoreReg);
      rl_src2 = LoadValue(rl_src2, kCoreReg);
      if (check_zero) {
          GenImmedCheck(kCondEq, rl_src2.low_reg, 0, kThrowDivZero);
      }
      rl_result = GenDivRem(rl_dest, rl_src1.low_reg, rl_src2.low_reg, op == kOpDiv);
    } else {
      ThreadOffset func_offset = QUICK_ENTRYPOINT_OFFSET(pIdivmod);
      FlushAllRegs();   /* Send everything to home location */
      LoadValueDirectFixed(rl_src2, TargetReg(kArg1));
      int r_tgt = CallHelperSetup(func_offset);
      LoadValueDirectFixed(rl_src1, TargetReg(kArg0));
      if (check_zero) {
        GenImmedCheck(kCondEq, TargetReg(kArg1), 0, kThrowDivZero);
      }
      // NOTE: callout here is not a safepoint
      CallHelper(r_tgt, func_offset, false /* not a safepoint */);
      if (op == kOpDiv)
        rl_result = GetReturn(false);
      else
        rl_result = GetReturnAlt();
    }
    StoreValue(rl_dest, rl_result);
  }
}

/*
 * The following are the first-level codegen routines that analyze the format
 * of each bytecode then either dispatch special purpose codegen routines
 * or produce corresponding Thumb instructions directly.
 */

static bool IsPowerOfTwo(int x) {
  return (x & (x - 1)) == 0;
}

// Returns true if no more than two bits are set in 'x'.
static bool IsPopCountLE2(unsigned int x) {
  x &= x - 1;
  return (x & (x - 1)) == 0;
}

// Returns the index of the lowest set bit in 'x'.
static int LowestSetBit(unsigned int x) {
  int bit_posn = 0;
  while ((x & 0xf) == 0) {
    bit_posn += 4;
    x >>= 4;
  }
  while ((x & 1) == 0) {
    bit_posn++;
    x >>= 1;
  }
  return bit_posn;
}

// Returns true if it added instructions to 'cu' to divide 'rl_src' by 'lit'
// and store the result in 'rl_dest'.
bool Mir2Lir::HandleEasyDivRem(Instruction::Code dalvik_opcode, bool is_div,
                               RegLocation rl_src, RegLocation rl_dest, int lit) {
  if ((lit < 2) || ((cu_->instruction_set != kThumb2) && !IsPowerOfTwo(lit))) {
    return false;
  }
  // No divide instruction for Arm, so check for more special cases
  if ((cu_->instruction_set == kThumb2) && !IsPowerOfTwo(lit)) {
    return SmallLiteralDivRem(dalvik_opcode, is_div, rl_src, rl_dest, lit);
  }
  int k = LowestSetBit(lit);
  if (k >= 30) {
    // Avoid special cases.
    return false;
  }
  rl_src = LoadValue(rl_src, kCoreReg);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  if (is_div) {
    int t_reg = AllocTemp();
    if (lit == 2) {
      // Division by 2 is by far the most common division by constant.
      OpRegRegImm(kOpLsr, t_reg, rl_src.low_reg, 32 - k);
      OpRegRegReg(kOpAdd, t_reg, t_reg, rl_src.low_reg);
      OpRegRegImm(kOpAsr, rl_result.low_reg, t_reg, k);
    } else {
      OpRegRegImm(kOpAsr, t_reg, rl_src.low_reg, 31);
      OpRegRegImm(kOpLsr, t_reg, t_reg, 32 - k);
      OpRegRegReg(kOpAdd, t_reg, t_reg, rl_src.low_reg);
      OpRegRegImm(kOpAsr, rl_result.low_reg, t_reg, k);
    }
  } else {
    int t_reg1 = AllocTemp();
    int t_reg2 = AllocTemp();
    if (lit == 2) {
      OpRegRegImm(kOpLsr, t_reg1, rl_src.low_reg, 32 - k);
      OpRegRegReg(kOpAdd, t_reg2, t_reg1, rl_src.low_reg);
      OpRegRegImm(kOpAnd, t_reg2, t_reg2, lit -1);
      OpRegRegReg(kOpSub, rl_result.low_reg, t_reg2, t_reg1);
    } else {
      OpRegRegImm(kOpAsr, t_reg1, rl_src.low_reg, 31);
      OpRegRegImm(kOpLsr, t_reg1, t_reg1, 32 - k);
      OpRegRegReg(kOpAdd, t_reg2, t_reg1, rl_src.low_reg);
      OpRegRegImm(kOpAnd, t_reg2, t_reg2, lit - 1);
      OpRegRegReg(kOpSub, rl_result.low_reg, t_reg2, t_reg1);
    }
  }
  StoreValue(rl_dest, rl_result);
  return true;
}

// Returns true if it added instructions to 'cu' to multiply 'rl_src' by 'lit'
// and store the result in 'rl_dest'.
bool Mir2Lir::HandleEasyMultiply(RegLocation rl_src, RegLocation rl_dest, int lit) {
  // Can we simplify this multiplication?
  bool power_of_two = false;
  bool pop_count_le2 = false;
  bool power_of_two_minus_one = false;
  if (lit < 2) {
    // Avoid special cases.
    return false;
  } else if (IsPowerOfTwo(lit)) {
    power_of_two = true;
  } else if (IsPopCountLE2(lit)) {
    pop_count_le2 = true;
  } else if (IsPowerOfTwo(lit + 1)) {
    power_of_two_minus_one = true;
  } else {
    return false;
  }
  rl_src = LoadValue(rl_src, kCoreReg);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  if (power_of_two) {
    // Shift.
    OpRegRegImm(kOpLsl, rl_result.low_reg, rl_src.low_reg, LowestSetBit(lit));
  } else if (pop_count_le2) {
    // Shift and add and shift.
    int first_bit = LowestSetBit(lit);
    int second_bit = LowestSetBit(lit ^ (1 << first_bit));
    GenMultiplyByTwoBitMultiplier(rl_src, rl_result, lit, first_bit, second_bit);
  } else {
    // Reverse subtract: (src << (shift + 1)) - src.
    DCHECK(power_of_two_minus_one);
    // TUNING: rsb dst, src, src lsl#LowestSetBit(lit + 1)
    int t_reg = AllocTemp();
    OpRegRegImm(kOpLsl, t_reg, rl_src.low_reg, LowestSetBit(lit + 1));
    OpRegRegReg(kOpSub, rl_result.low_reg, t_reg, rl_src.low_reg);
  }
  StoreValue(rl_dest, rl_result);
  return true;
}

void Mir2Lir::GenArithOpIntLit(Instruction::Code opcode, RegLocation rl_dest, RegLocation rl_src,
                               int lit) {
  RegLocation rl_result;
  OpKind op = static_cast<OpKind>(0);    /* Make gcc happy */
  int shift_op = false;
  bool is_div = false;

  switch (opcode) {
    case Instruction::RSUB_INT_LIT8:
    case Instruction::RSUB_INT: {
      rl_src = LoadValue(rl_src, kCoreReg);
      rl_result = EvalLoc(rl_dest, kCoreReg, true);
      if (cu_->instruction_set == kThumb2) {
        OpRegRegImm(kOpRsub, rl_result.low_reg, rl_src.low_reg, lit);
      } else {
        OpRegReg(kOpNeg, rl_result.low_reg, rl_src.low_reg);
        OpRegImm(kOpAdd, rl_result.low_reg, lit);
      }
      StoreValue(rl_dest, rl_result);
      return;
    }

    case Instruction::SUB_INT:
    case Instruction::SUB_INT_2ADDR:
      lit = -lit;
      // Intended fallthrough
    case Instruction::ADD_INT:
    case Instruction::ADD_INT_2ADDR:
    case Instruction::ADD_INT_LIT8:
    case Instruction::ADD_INT_LIT16:
      op = kOpAdd;
      break;
    case Instruction::MUL_INT:
    case Instruction::MUL_INT_2ADDR:
    case Instruction::MUL_INT_LIT8:
    case Instruction::MUL_INT_LIT16: {
      if (HandleEasyMultiply(rl_src, rl_dest, lit)) {
        return;
      }
      op = kOpMul;
      break;
    }
    case Instruction::AND_INT:
    case Instruction::AND_INT_2ADDR:
    case Instruction::AND_INT_LIT8:
    case Instruction::AND_INT_LIT16:
      op = kOpAnd;
      break;
    case Instruction::OR_INT:
    case Instruction::OR_INT_2ADDR:
    case Instruction::OR_INT_LIT8:
    case Instruction::OR_INT_LIT16:
      op = kOpOr;
      break;
    case Instruction::XOR_INT:
    case Instruction::XOR_INT_2ADDR:
    case Instruction::XOR_INT_LIT8:
    case Instruction::XOR_INT_LIT16:
      op = kOpXor;
      break;
    case Instruction::SHL_INT_LIT8:
    case Instruction::SHL_INT:
    case Instruction::SHL_INT_2ADDR:
      lit &= 31;
      shift_op = true;
      op = kOpLsl;
      break;
    case Instruction::SHR_INT_LIT8:
    case Instruction::SHR_INT:
    case Instruction::SHR_INT_2ADDR:
      lit &= 31;
      shift_op = true;
      op = kOpAsr;
      break;
    case Instruction::USHR_INT_LIT8:
    case Instruction::USHR_INT:
    case Instruction::USHR_INT_2ADDR:
      lit &= 31;
      shift_op = true;
      op = kOpLsr;
      break;

    case Instruction::DIV_INT:
    case Instruction::DIV_INT_2ADDR:
    case Instruction::DIV_INT_LIT8:
    case Instruction::DIV_INT_LIT16:
    case Instruction::REM_INT:
    case Instruction::REM_INT_2ADDR:
    case Instruction::REM_INT_LIT8:
    case Instruction::REM_INT_LIT16: {
      if (lit == 0) {
        GenImmedCheck(kCondAl, 0, 0, kThrowDivZero);
        return;
      }
      if ((opcode == Instruction::DIV_INT) ||
          (opcode == Instruction::DIV_INT_2ADDR) ||
          (opcode == Instruction::DIV_INT_LIT8) ||
          (opcode == Instruction::DIV_INT_LIT16)) {
        is_div = true;
      } else {
        is_div = false;
      }
      if (HandleEasyDivRem(opcode, is_div, rl_src, rl_dest, lit)) {
        return;
      }
      if (cu_->instruction_set == kMips) {
        rl_src = LoadValue(rl_src, kCoreReg);
        rl_result = GenDivRemLit(rl_dest, rl_src.low_reg, lit, is_div);
      } else {
        FlushAllRegs();   /* Everything to home location */
        LoadValueDirectFixed(rl_src, TargetReg(kArg0));
        Clobber(TargetReg(kArg0));
        ThreadOffset func_offset = QUICK_ENTRYPOINT_OFFSET(pIdivmod);
        CallRuntimeHelperRegImm(func_offset, TargetReg(kArg0), lit, false);
        if (is_div)
          rl_result = GetReturn(false);
        else
          rl_result = GetReturnAlt();
      }
      StoreValue(rl_dest, rl_result);
      return;
    }
    default:
      LOG(FATAL) << "Unexpected opcode " << opcode;
  }
  rl_src = LoadValue(rl_src, kCoreReg);
  rl_result = EvalLoc(rl_dest, kCoreReg, true);
  // Avoid shifts by literal 0 - no support in Thumb.  Change to copy
  if (shift_op && (lit == 0)) {
    OpRegCopy(rl_result.low_reg, rl_src.low_reg);
  } else {
    OpRegRegImm(op, rl_result.low_reg, rl_src.low_reg, lit);
  }
  StoreValue(rl_dest, rl_result);
}

void Mir2Lir::GenArithOpLong(Instruction::Code opcode, RegLocation rl_dest,
                             RegLocation rl_src1, RegLocation rl_src2) {
  RegLocation rl_result;
  OpKind first_op = kOpBkpt;
  OpKind second_op = kOpBkpt;
  bool call_out = false;
  bool check_zero = false;
  ThreadOffset func_offset(-1);
  int ret_reg = TargetReg(kRet0);

  switch (opcode) {
    case Instruction::NOT_LONG:
      rl_src2 = LoadValueWide(rl_src2, kCoreReg);
      rl_result = EvalLoc(rl_dest, kCoreReg, true);
      // Check for destructive overlap
      if (rl_result.low_reg == rl_src2.high_reg) {
        int t_reg = AllocTemp();
        OpRegCopy(t_reg, rl_src2.high_reg);
        OpRegReg(kOpMvn, rl_result.low_reg, rl_src2.low_reg);
        OpRegReg(kOpMvn, rl_result.high_reg, t_reg);
        FreeTemp(t_reg);
      } else {
        OpRegReg(kOpMvn, rl_result.low_reg, rl_src2.low_reg);
        OpRegReg(kOpMvn, rl_result.high_reg, rl_src2.high_reg);
      }
      StoreValueWide(rl_dest, rl_result);
      return;
    case Instruction::ADD_LONG:
    case Instruction::ADD_LONG_2ADDR:
      if (cu_->instruction_set != kThumb2) {
        GenAddLong(rl_dest, rl_src1, rl_src2);
        return;
      }
      first_op = kOpAdd;
      second_op = kOpAdc;
      break;
    case Instruction::SUB_LONG:
    case Instruction::SUB_LONG_2ADDR:
      if (cu_->instruction_set != kThumb2) {
        GenSubLong(rl_dest, rl_src1, rl_src2);
        return;
      }
      first_op = kOpSub;
      second_op = kOpSbc;
      break;
    case Instruction::MUL_LONG:
    case Instruction::MUL_LONG_2ADDR:
      if (cu_->instruction_set == kThumb2) {
        GenMulLong(rl_dest, rl_src1, rl_src2);
        return;
      } else {
        call_out = true;
        ret_reg = TargetReg(kRet0);
        func_offset = QUICK_ENTRYPOINT_OFFSET(pLmul);
      }
      break;
    case Instruction::DIV_LONG:
    case Instruction::DIV_LONG_2ADDR:
      call_out = true;
      check_zero = true;
      ret_reg = TargetReg(kRet0);
      func_offset = QUICK_ENTRYPOINT_OFFSET(pLdiv);
      break;
    case Instruction::REM_LONG:
    case Instruction::REM_LONG_2ADDR:
      call_out = true;
      check_zero = true;
      func_offset = QUICK_ENTRYPOINT_OFFSET(pLdivmod);
      /* NOTE - for Arm, result is in kArg2/kArg3 instead of kRet0/kRet1 */
      ret_reg = (cu_->instruction_set == kThumb2) ? TargetReg(kArg2) : TargetReg(kRet0);
      break;
    case Instruction::AND_LONG_2ADDR:
    case Instruction::AND_LONG:
      if (cu_->instruction_set == kX86) {
        return GenAndLong(rl_dest, rl_src1, rl_src2);
      }
      first_op = kOpAnd;
      second_op = kOpAnd;
      break;
    case Instruction::OR_LONG:
    case Instruction::OR_LONG_2ADDR:
      if (cu_->instruction_set == kX86) {
        GenOrLong(rl_dest, rl_src1, rl_src2);
        return;
      }
      first_op = kOpOr;
      second_op = kOpOr;
      break;
    case Instruction::XOR_LONG:
    case Instruction::XOR_LONG_2ADDR:
      if (cu_->instruction_set == kX86) {
        GenXorLong(rl_dest, rl_src1, rl_src2);
        return;
      }
      first_op = kOpXor;
      second_op = kOpXor;
      break;
    case Instruction::NEG_LONG: {
      GenNegLong(rl_dest, rl_src2);
      return;
    }
    default:
      LOG(FATAL) << "Invalid long arith op";
  }
  if (!call_out) {
    GenLong3Addr(first_op, second_op, rl_dest, rl_src1, rl_src2);
  } else {
    FlushAllRegs();   /* Send everything to home location */
    if (check_zero) {
      LoadValueDirectWideFixed(rl_src2, TargetReg(kArg2), TargetReg(kArg3));
      int r_tgt = CallHelperSetup(func_offset);
      GenDivZeroCheck(TargetReg(kArg2), TargetReg(kArg3));
      LoadValueDirectWideFixed(rl_src1, TargetReg(kArg0), TargetReg(kArg1));
      // NOTE: callout here is not a safepoint
      CallHelper(r_tgt, func_offset, false /* not safepoint */);
    } else {
      CallRuntimeHelperRegLocationRegLocation(func_offset, rl_src1, rl_src2, false);
    }
    // Adjust return regs in to handle case of rem returning kArg2/kArg3
    if (ret_reg == TargetReg(kRet0))
      rl_result = GetReturnWide(false);
    else
      rl_result = GetReturnWideAlt();
    StoreValueWide(rl_dest, rl_result);
  }
}

void Mir2Lir::GenConversionCall(ThreadOffset func_offset,
                                RegLocation rl_dest, RegLocation rl_src) {
  /*
   * Don't optimize the register usage since it calls out to support
   * functions
   */
  FlushAllRegs();   /* Send everything to home location */
  if (rl_src.wide) {
    LoadValueDirectWideFixed(rl_src, rl_src.fp ? TargetReg(kFArg0) : TargetReg(kArg0),
                             rl_src.fp ? TargetReg(kFArg1) : TargetReg(kArg1));
  } else {
    LoadValueDirectFixed(rl_src, rl_src.fp ? TargetReg(kFArg0) : TargetReg(kArg0));
  }
  CallRuntimeHelperRegLocation(func_offset, rl_src, false);
  if (rl_dest.wide) {
    RegLocation rl_result;
    rl_result = GetReturnWide(rl_dest.fp);
    StoreValueWide(rl_dest, rl_result);
  } else {
    RegLocation rl_result;
    rl_result = GetReturn(rl_dest.fp);
    StoreValue(rl_dest, rl_result);
  }
}

/* Check if we need to check for pending suspend request */
void Mir2Lir::GenSuspendTest(int opt_flags) {
  if (NO_SUSPEND || (opt_flags & MIR_IGNORE_SUSPEND_CHECK)) {
    return;
  }
  FlushAllRegs();
  LIR* branch = OpTestSuspend(NULL);
  LIR* ret_lab = NewLIR0(kPseudoTargetLabel);
  LIR* target = RawLIR(current_dalvik_offset_, kPseudoSuspendTarget,
                       reinterpret_cast<uintptr_t>(ret_lab), current_dalvik_offset_);
  branch->target = target;
  suspend_launchpads_.Insert(target);
}

/* Check if we need to check for pending suspend request */
void Mir2Lir::GenSuspendTestAndBranch(int opt_flags, LIR* target) {
  if (NO_SUSPEND || (opt_flags & MIR_IGNORE_SUSPEND_CHECK)) {
    OpUnconditionalBranch(target);
    return;
  }
  OpTestSuspend(target);
  LIR* launch_pad =
      RawLIR(current_dalvik_offset_, kPseudoSuspendTarget,
             reinterpret_cast<uintptr_t>(target), current_dalvik_offset_);
  FlushAllRegs();
  OpUnconditionalBranch(launch_pad);
  suspend_launchpads_.Insert(launch_pad);
}

}  // namespace art
