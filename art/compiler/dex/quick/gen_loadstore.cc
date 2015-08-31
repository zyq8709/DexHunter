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

#include "dex/compiler_ir.h"
#include "dex/compiler_internals.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "invoke_type.h"

namespace art {

/* This file contains target-independent codegen and support. */

/*
 * Load an immediate value into a fixed or temp register.  Target
 * register is clobbered, and marked in_use.
 */
LIR* Mir2Lir::LoadConstant(int r_dest, int value) {
  if (IsTemp(r_dest)) {
    Clobber(r_dest);
    MarkInUse(r_dest);
  }
  return LoadConstantNoClobber(r_dest, value);
}

/*
 * Temporary workaround for Issue 7250540.  If we're loading a constant zero into a
 * promoted floating point register, also copy a zero into the int/ref identity of
 * that sreg.
 */
void Mir2Lir::Workaround7250540(RegLocation rl_dest, int zero_reg) {
  if (rl_dest.fp) {
    int pmap_index = SRegToPMap(rl_dest.s_reg_low);
    if (promotion_map_[pmap_index].fp_location == kLocPhysReg) {
      // Now, determine if this vreg is ever used as a reference.  If not, we're done.
      bool used_as_reference = false;
      int base_vreg = mir_graph_->SRegToVReg(rl_dest.s_reg_low);
      for (int i = 0; !used_as_reference && (i < mir_graph_->GetNumSSARegs()); i++) {
        if (mir_graph_->SRegToVReg(mir_graph_->reg_location_[i].s_reg_low) == base_vreg) {
          used_as_reference |= mir_graph_->reg_location_[i].ref;
        }
      }
      if (!used_as_reference) {
        return;
      }
      int temp_reg = zero_reg;
      if (temp_reg == INVALID_REG) {
        temp_reg = AllocTemp();
        LoadConstant(temp_reg, 0);
      }
      if (promotion_map_[pmap_index].core_location == kLocPhysReg) {
        // Promoted - just copy in a zero
        OpRegCopy(promotion_map_[pmap_index].core_reg, temp_reg);
      } else {
        // Lives in the frame, need to store.
        StoreBaseDisp(TargetReg(kSp), SRegOffset(rl_dest.s_reg_low), temp_reg, kWord);
      }
      if (zero_reg == INVALID_REG) {
        FreeTemp(temp_reg);
      }
    }
  }
}

/* Load a word at base + displacement.  Displacement must be word multiple */
LIR* Mir2Lir::LoadWordDisp(int rBase, int displacement, int r_dest) {
  return LoadBaseDisp(rBase, displacement, r_dest, kWord,
                      INVALID_SREG);
}

LIR* Mir2Lir::StoreWordDisp(int rBase, int displacement, int r_src) {
  return StoreBaseDisp(rBase, displacement, r_src, kWord);
}

/*
 * Load a Dalvik register into a physical register.  Take care when
 * using this routine, as it doesn't perform any bookkeeping regarding
 * register liveness.  That is the responsibility of the caller.
 */
void Mir2Lir::LoadValueDirect(RegLocation rl_src, int r_dest) {
  rl_src = UpdateLoc(rl_src);
  if (rl_src.location == kLocPhysReg) {
    OpRegCopy(r_dest, rl_src.low_reg);
  } else if (IsInexpensiveConstant(rl_src)) {
    LoadConstantNoClobber(r_dest, mir_graph_->ConstantValue(rl_src));
  } else {
    DCHECK((rl_src.location == kLocDalvikFrame) ||
           (rl_src.location == kLocCompilerTemp));
    LoadWordDisp(TargetReg(kSp), SRegOffset(rl_src.s_reg_low), r_dest);
  }
}

/*
 * Similar to LoadValueDirect, but clobbers and allocates the target
 * register.  Should be used when loading to a fixed register (for example,
 * loading arguments to an out of line call.
 */
void Mir2Lir::LoadValueDirectFixed(RegLocation rl_src, int r_dest) {
  Clobber(r_dest);
  MarkInUse(r_dest);
  LoadValueDirect(rl_src, r_dest);
}

/*
 * Load a Dalvik register pair into a physical register[s].  Take care when
 * using this routine, as it doesn't perform any bookkeeping regarding
 * register liveness.  That is the responsibility of the caller.
 */
void Mir2Lir::LoadValueDirectWide(RegLocation rl_src, int reg_lo,
             int reg_hi) {
  rl_src = UpdateLocWide(rl_src);
  if (rl_src.location == kLocPhysReg) {
    OpRegCopyWide(reg_lo, reg_hi, rl_src.low_reg, rl_src.high_reg);
  } else if (IsInexpensiveConstant(rl_src)) {
    LoadConstantWide(reg_lo, reg_hi, mir_graph_->ConstantValueWide(rl_src));
  } else {
    DCHECK((rl_src.location == kLocDalvikFrame) ||
           (rl_src.location == kLocCompilerTemp));
    LoadBaseDispWide(TargetReg(kSp), SRegOffset(rl_src.s_reg_low),
                     reg_lo, reg_hi, INVALID_SREG);
  }
}

/*
 * Similar to LoadValueDirect, but clobbers and allocates the target
 * registers.  Should be used when loading to a fixed registers (for example,
 * loading arguments to an out of line call.
 */
void Mir2Lir::LoadValueDirectWideFixed(RegLocation rl_src, int reg_lo,
                                       int reg_hi) {
  Clobber(reg_lo);
  Clobber(reg_hi);
  MarkInUse(reg_lo);
  MarkInUse(reg_hi);
  LoadValueDirectWide(rl_src, reg_lo, reg_hi);
}

RegLocation Mir2Lir::LoadValue(RegLocation rl_src, RegisterClass op_kind) {
  rl_src = EvalLoc(rl_src, op_kind, false);
  if (IsInexpensiveConstant(rl_src) || rl_src.location != kLocPhysReg) {
    LoadValueDirect(rl_src, rl_src.low_reg);
    rl_src.location = kLocPhysReg;
    MarkLive(rl_src.low_reg, rl_src.s_reg_low);
  }
  return rl_src;
}

void Mir2Lir::StoreValue(RegLocation rl_dest, RegLocation rl_src) {
  /*
   * Sanity checking - should never try to store to the same
   * ssa name during the compilation of a single instruction
   * without an intervening ClobberSReg().
   */
  if (kIsDebugBuild) {
    DCHECK((live_sreg_ == INVALID_SREG) ||
           (rl_dest.s_reg_low != live_sreg_));
    live_sreg_ = rl_dest.s_reg_low;
  }
  LIR* def_start;
  LIR* def_end;
  DCHECK(!rl_dest.wide);
  DCHECK(!rl_src.wide);
  rl_src = UpdateLoc(rl_src);
  rl_dest = UpdateLoc(rl_dest);
  if (rl_src.location == kLocPhysReg) {
    if (IsLive(rl_src.low_reg) ||
      IsPromoted(rl_src.low_reg) ||
      (rl_dest.location == kLocPhysReg)) {
      // Src is live/promoted or Dest has assigned reg.
      rl_dest = EvalLoc(rl_dest, kAnyReg, false);
      OpRegCopy(rl_dest.low_reg, rl_src.low_reg);
    } else {
      // Just re-assign the registers.  Dest gets Src's regs
      rl_dest.low_reg = rl_src.low_reg;
      Clobber(rl_src.low_reg);
    }
  } else {
    // Load Src either into promoted Dest or temps allocated for Dest
    rl_dest = EvalLoc(rl_dest, kAnyReg, false);
    LoadValueDirect(rl_src, rl_dest.low_reg);
  }

  // Dest is now live and dirty (until/if we flush it to home location)
  MarkLive(rl_dest.low_reg, rl_dest.s_reg_low);
  MarkDirty(rl_dest);


  ResetDefLoc(rl_dest);
  if (IsDirty(rl_dest.low_reg) &&
      oat_live_out(rl_dest.s_reg_low)) {
    def_start = last_lir_insn_;
    StoreBaseDisp(TargetReg(kSp), SRegOffset(rl_dest.s_reg_low),
                  rl_dest.low_reg, kWord);
    MarkClean(rl_dest);
    def_end = last_lir_insn_;
    if (!rl_dest.ref) {
      // Exclude references from store elimination
      MarkDef(rl_dest, def_start, def_end);
    }
  }
}

RegLocation Mir2Lir::LoadValueWide(RegLocation rl_src, RegisterClass op_kind) {
  DCHECK(rl_src.wide);
  rl_src = EvalLoc(rl_src, op_kind, false);
  if (IsInexpensiveConstant(rl_src) || rl_src.location != kLocPhysReg) {
    LoadValueDirectWide(rl_src, rl_src.low_reg, rl_src.high_reg);
    rl_src.location = kLocPhysReg;
    MarkLive(rl_src.low_reg, rl_src.s_reg_low);
    MarkLive(rl_src.high_reg, GetSRegHi(rl_src.s_reg_low));
  }
  return rl_src;
}

void Mir2Lir::StoreValueWide(RegLocation rl_dest, RegLocation rl_src) {
  /*
   * Sanity checking - should never try to store to the same
   * ssa name during the compilation of a single instruction
   * without an intervening ClobberSReg().
   */
  if (kIsDebugBuild) {
    DCHECK((live_sreg_ == INVALID_SREG) ||
           (rl_dest.s_reg_low != live_sreg_));
    live_sreg_ = rl_dest.s_reg_low;
  }
  LIR* def_start;
  LIR* def_end;
  DCHECK_EQ(IsFpReg(rl_src.low_reg), IsFpReg(rl_src.high_reg));
  DCHECK(rl_dest.wide);
  DCHECK(rl_src.wide);
  if (rl_src.location == kLocPhysReg) {
    if (IsLive(rl_src.low_reg) ||
        IsLive(rl_src.high_reg) ||
        IsPromoted(rl_src.low_reg) ||
        IsPromoted(rl_src.high_reg) ||
        (rl_dest.location == kLocPhysReg)) {
      // Src is live or promoted or Dest has assigned reg.
      rl_dest = EvalLoc(rl_dest, kAnyReg, false);
      OpRegCopyWide(rl_dest.low_reg, rl_dest.high_reg,
                    rl_src.low_reg, rl_src.high_reg);
    } else {
      // Just re-assign the registers.  Dest gets Src's regs
      rl_dest.low_reg = rl_src.low_reg;
      rl_dest.high_reg = rl_src.high_reg;
      Clobber(rl_src.low_reg);
      Clobber(rl_src.high_reg);
    }
  } else {
    // Load Src either into promoted Dest or temps allocated for Dest
    rl_dest = EvalLoc(rl_dest, kAnyReg, false);
    LoadValueDirectWide(rl_src, rl_dest.low_reg, rl_dest.high_reg);
  }

  // Dest is now live and dirty (until/if we flush it to home location)
  MarkLive(rl_dest.low_reg, rl_dest.s_reg_low);
  MarkLive(rl_dest.high_reg, GetSRegHi(rl_dest.s_reg_low));
  MarkDirty(rl_dest);
  MarkPair(rl_dest.low_reg, rl_dest.high_reg);


  ResetDefLocWide(rl_dest);
  if ((IsDirty(rl_dest.low_reg) ||
      IsDirty(rl_dest.high_reg)) &&
      (oat_live_out(rl_dest.s_reg_low) ||
      oat_live_out(GetSRegHi(rl_dest.s_reg_low)))) {
    def_start = last_lir_insn_;
    DCHECK_EQ((mir_graph_->SRegToVReg(rl_dest.s_reg_low)+1),
              mir_graph_->SRegToVReg(GetSRegHi(rl_dest.s_reg_low)));
    StoreBaseDispWide(TargetReg(kSp), SRegOffset(rl_dest.s_reg_low),
                      rl_dest.low_reg, rl_dest.high_reg);
    MarkClean(rl_dest);
    def_end = last_lir_insn_;
    MarkDefWide(rl_dest, def_start, def_end);
  }
}

/* Utilities to load the current Method* */
void Mir2Lir::LoadCurrMethodDirect(int r_tgt) {
  LoadValueDirectFixed(mir_graph_->GetMethodLoc(), r_tgt);
}

RegLocation Mir2Lir::LoadCurrMethod() {
  return LoadValue(mir_graph_->GetMethodLoc(), kCoreReg);
}

}  // namespace art
