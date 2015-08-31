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

/* This file contains register alloction support. */

#include "dex/compiler_ir.h"
#include "dex/compiler_internals.h"
#include "mir_to_lir-inl.h"

namespace art {

/*
 * Free all allocated temps in the temp pools.  Note that this does
 * not affect the "liveness" of a temp register, which will stay
 * live until it is either explicitly killed or reallocated.
 */
void Mir2Lir::ResetRegPool() {
  for (int i = 0; i < reg_pool_->num_core_regs; i++) {
    if (reg_pool_->core_regs[i].is_temp)
      reg_pool_->core_regs[i].in_use = false;
  }
  for (int i = 0; i < reg_pool_->num_fp_regs; i++) {
    if (reg_pool_->FPRegs[i].is_temp)
      reg_pool_->FPRegs[i].in_use = false;
  }
  // Reset temp tracking sanity check.
  if (kIsDebugBuild) {
    live_sreg_ = INVALID_SREG;
  }
}

 /*
  * Set up temp & preserved register pools specialized by target.
  * Note: num_regs may be zero.
  */
void Mir2Lir::CompilerInitPool(RegisterInfo* regs, int* reg_nums, int num) {
  for (int i = 0; i < num; i++) {
    regs[i].reg = reg_nums[i];
    regs[i].in_use = false;
    regs[i].is_temp = false;
    regs[i].pair = false;
    regs[i].live = false;
    regs[i].dirty = false;
    regs[i].s_reg = INVALID_SREG;
  }
}

void Mir2Lir::DumpRegPool(RegisterInfo* p, int num_regs) {
  LOG(INFO) << "================================================";
  for (int i = 0; i < num_regs; i++) {
    LOG(INFO) << StringPrintf(
        "R[%d]: T:%d, U:%d, P:%d, p:%d, LV:%d, D:%d, SR:%d, ST:%x, EN:%x",
        p[i].reg, p[i].is_temp, p[i].in_use, p[i].pair, p[i].partner,
        p[i].live, p[i].dirty, p[i].s_reg, reinterpret_cast<uintptr_t>(p[i].def_start),
        reinterpret_cast<uintptr_t>(p[i].def_end));
  }
  LOG(INFO) << "================================================";
}

void Mir2Lir::DumpCoreRegPool() {
  DumpRegPool(reg_pool_->core_regs, reg_pool_->num_core_regs);
}

void Mir2Lir::DumpFpRegPool() {
  DumpRegPool(reg_pool_->FPRegs, reg_pool_->num_fp_regs);
}

void Mir2Lir::ClobberSRegBody(RegisterInfo* p, int num_regs, int s_reg) {
  for (int i = 0; i< num_regs; i++) {
    if (p[i].s_reg == s_reg) {
      if (p[i].is_temp) {
        p[i].live = false;
      }
      p[i].def_start = NULL;
      p[i].def_end = NULL;
    }
  }
}

/*
 * Break the association between a Dalvik vreg and a physical temp register of either register
 * class.
 * TODO: Ideally, the public version of this code should not exist.  Besides its local usage
 * in the register utilities, is is also used by code gen routines to work around a deficiency in
 * local register allocation, which fails to distinguish between the "in" and "out" identities
 * of Dalvik vregs.  This can result in useless register copies when the same Dalvik vreg
 * is used both as the source and destination register of an operation in which the type
 * changes (for example: INT_TO_FLOAT v1, v1).  Revisit when improved register allocation is
 * addressed.
 */
void Mir2Lir::ClobberSReg(int s_reg) {
  /* Reset live temp tracking sanity checker */
  if (kIsDebugBuild) {
    if (s_reg == live_sreg_) {
      live_sreg_ = INVALID_SREG;
    }
  }
  ClobberSRegBody(reg_pool_->core_regs, reg_pool_->num_core_regs, s_reg);
  ClobberSRegBody(reg_pool_->FPRegs, reg_pool_->num_fp_regs, s_reg);
}

/*
 * SSA names associated with the initial definitions of Dalvik
 * registers are the same as the Dalvik register number (and
 * thus take the same position in the promotion_map.  However,
 * the special Method* and compiler temp resisters use negative
 * v_reg numbers to distinguish them and can have an arbitrary
 * ssa name (above the last original Dalvik register).  This function
 * maps SSA names to positions in the promotion_map array.
 */
int Mir2Lir::SRegToPMap(int s_reg) {
  DCHECK_LT(s_reg, mir_graph_->GetNumSSARegs());
  DCHECK_GE(s_reg, 0);
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  if (v_reg >= 0) {
    DCHECK_LT(v_reg, cu_->num_dalvik_registers);
    return v_reg;
  } else {
    int pos = std::abs(v_reg) - std::abs(SSA_METHOD_BASEREG);
    DCHECK_LE(pos, cu_->num_compiler_temps);
    return cu_->num_dalvik_registers + pos;
  }
}

void Mir2Lir::RecordCorePromotion(int reg, int s_reg) {
  int p_map_idx = SRegToPMap(s_reg);
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  GetRegInfo(reg)->in_use = true;
  core_spill_mask_ |= (1 << reg);
  // Include reg for later sort
  core_vmap_table_.push_back(reg << VREG_NUM_WIDTH | (v_reg & ((1 << VREG_NUM_WIDTH) - 1)));
  num_core_spills_++;
  promotion_map_[p_map_idx].core_location = kLocPhysReg;
  promotion_map_[p_map_idx].core_reg = reg;
}

/* Reserve a callee-save register.  Return -1 if none available */
int Mir2Lir::AllocPreservedCoreReg(int s_reg) {
  int res = -1;
  RegisterInfo* core_regs = reg_pool_->core_regs;
  for (int i = 0; i < reg_pool_->num_core_regs; i++) {
    if (!core_regs[i].is_temp && !core_regs[i].in_use) {
      res = core_regs[i].reg;
      RecordCorePromotion(res, s_reg);
      break;
    }
  }
  return res;
}

void Mir2Lir::RecordFpPromotion(int reg, int s_reg) {
  int p_map_idx = SRegToPMap(s_reg);
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  GetRegInfo(reg)->in_use = true;
  MarkPreservedSingle(v_reg, reg);
  promotion_map_[p_map_idx].fp_location = kLocPhysReg;
  promotion_map_[p_map_idx].FpReg = reg;
}

/*
 * Reserve a callee-save fp single register.  Try to fullfill request for
 * even/odd  allocation, but go ahead and allocate anything if not
 * available.  If nothing's available, return -1.
 */
int Mir2Lir::AllocPreservedSingle(int s_reg, bool even) {
  int res = -1;
  RegisterInfo* FPRegs = reg_pool_->FPRegs;
  for (int i = 0; i < reg_pool_->num_fp_regs; i++) {
    if (!FPRegs[i].is_temp && !FPRegs[i].in_use &&
      ((FPRegs[i].reg & 0x1) == 0) == even) {
      res = FPRegs[i].reg;
      RecordFpPromotion(res, s_reg);
      break;
    }
  }
  return res;
}

/*
 * Somewhat messy code here.  We want to allocate a pair of contiguous
 * physical single-precision floating point registers starting with
 * an even numbered reg.  It is possible that the paired s_reg (s_reg+1)
 * has already been allocated - try to fit if possible.  Fail to
 * allocate if we can't meet the requirements for the pair of
 * s_reg<=sX[even] & (s_reg+1)<= sX+1.
 */
int Mir2Lir::AllocPreservedDouble(int s_reg) {
  int res = -1;  // Assume failure
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  int p_map_idx = SRegToPMap(s_reg);
  if (promotion_map_[p_map_idx+1].fp_location == kLocPhysReg) {
    // Upper reg is already allocated.  Can we fit?
    int high_reg = promotion_map_[p_map_idx+1].FpReg;
    if ((high_reg & 1) == 0) {
      // High reg is even - fail.
      return res;
    }
    // Is the low reg of the pair free?
    RegisterInfo* p = GetRegInfo(high_reg-1);
    if (p->in_use || p->is_temp) {
      // Already allocated or not preserved - fail.
      return res;
    }
    // OK - good to go.
    res = p->reg;
    p->in_use = true;
    DCHECK_EQ((res & 1), 0);
    MarkPreservedSingle(v_reg, res);
  } else {
    RegisterInfo* FPRegs = reg_pool_->FPRegs;
    for (int i = 0; i < reg_pool_->num_fp_regs; i++) {
      if (!FPRegs[i].is_temp && !FPRegs[i].in_use &&
        ((FPRegs[i].reg & 0x1) == 0x0) &&
        !FPRegs[i+1].is_temp && !FPRegs[i+1].in_use &&
        ((FPRegs[i+1].reg & 0x1) == 0x1) &&
        (FPRegs[i].reg + 1) == FPRegs[i+1].reg) {
        res = FPRegs[i].reg;
        FPRegs[i].in_use = true;
        MarkPreservedSingle(v_reg, res);
        FPRegs[i+1].in_use = true;
        DCHECK_EQ(res + 1, FPRegs[i+1].reg);
        MarkPreservedSingle(v_reg+1, res+1);
        break;
      }
    }
  }
  if (res != -1) {
    promotion_map_[p_map_idx].fp_location = kLocPhysReg;
    promotion_map_[p_map_idx].FpReg = res;
    promotion_map_[p_map_idx+1].fp_location = kLocPhysReg;
    promotion_map_[p_map_idx+1].FpReg = res + 1;
  }
  return res;
}


/*
 * Reserve a callee-save fp register.   If this register can be used
 * as the first of a double, attempt to allocate an even pair of fp
 * single regs (but if can't still attempt to allocate a single, preferring
 * first to allocate an odd register.
 */
int Mir2Lir::AllocPreservedFPReg(int s_reg, bool double_start) {
  int res = -1;
  if (double_start) {
    res = AllocPreservedDouble(s_reg);
  }
  if (res == -1) {
    res = AllocPreservedSingle(s_reg, false /* try odd # */);
  }
  if (res == -1)
    res = AllocPreservedSingle(s_reg, true /* try even # */);
  return res;
}

int Mir2Lir::AllocTempBody(RegisterInfo* p, int num_regs, int* next_temp,
                           bool required) {
  int next = *next_temp;
  for (int i = 0; i< num_regs; i++) {
    if (next >= num_regs)
      next = 0;
    if (p[next].is_temp && !p[next].in_use && !p[next].live) {
      Clobber(p[next].reg);
      p[next].in_use = true;
      p[next].pair = false;
      *next_temp = next + 1;
      return p[next].reg;
    }
    next++;
  }
  next = *next_temp;
  for (int i = 0; i< num_regs; i++) {
    if (next >= num_regs)
      next = 0;
    if (p[next].is_temp && !p[next].in_use) {
      Clobber(p[next].reg);
      p[next].in_use = true;
      p[next].pair = false;
      *next_temp = next + 1;
      return p[next].reg;
    }
    next++;
  }
  if (required) {
    CodegenDump();
    DumpRegPool(reg_pool_->core_regs,
          reg_pool_->num_core_regs);
    LOG(FATAL) << "No free temp registers";
  }
  return -1;  // No register available
}

// REDO: too many assumptions.
int Mir2Lir::AllocTempDouble() {
  RegisterInfo* p = reg_pool_->FPRegs;
  int num_regs = reg_pool_->num_fp_regs;
  /* Start looking at an even reg */
  int next = reg_pool_->next_fp_reg & ~0x1;

  // First try to avoid allocating live registers
  for (int i = 0; i < num_regs; i+=2) {
    if (next >= num_regs)
      next = 0;
    if ((p[next].is_temp && !p[next].in_use && !p[next].live) &&
      (p[next+1].is_temp && !p[next+1].in_use && !p[next+1].live)) {
      Clobber(p[next].reg);
      Clobber(p[next+1].reg);
      p[next].in_use = true;
      p[next+1].in_use = true;
      DCHECK_EQ((p[next].reg+1), p[next+1].reg);
      DCHECK_EQ((p[next].reg & 0x1), 0);
      reg_pool_->next_fp_reg = next + 2;
      if (reg_pool_->next_fp_reg >= num_regs) {
        reg_pool_->next_fp_reg = 0;
      }
      return p[next].reg;
    }
    next += 2;
  }
  next = reg_pool_->next_fp_reg & ~0x1;

  // No choice - find a pair and kill it.
  for (int i = 0; i < num_regs; i+=2) {
    if (next >= num_regs)
      next = 0;
    if (p[next].is_temp && !p[next].in_use && p[next+1].is_temp &&
      !p[next+1].in_use) {
      Clobber(p[next].reg);
      Clobber(p[next+1].reg);
      p[next].in_use = true;
      p[next+1].in_use = true;
      DCHECK_EQ((p[next].reg+1), p[next+1].reg);
      DCHECK_EQ((p[next].reg & 0x1), 0);
      reg_pool_->next_fp_reg = next + 2;
      if (reg_pool_->next_fp_reg >= num_regs) {
        reg_pool_->next_fp_reg = 0;
      }
      return p[next].reg;
    }
    next += 2;
  }
  LOG(FATAL) << "No free temp registers (pair)";
  return -1;
}

/* Return a temp if one is available, -1 otherwise */
int Mir2Lir::AllocFreeTemp() {
  return AllocTempBody(reg_pool_->core_regs,
             reg_pool_->num_core_regs,
             &reg_pool_->next_core_reg, true);
}

int Mir2Lir::AllocTemp() {
  return AllocTempBody(reg_pool_->core_regs,
             reg_pool_->num_core_regs,
             &reg_pool_->next_core_reg, true);
}

int Mir2Lir::AllocTempFloat() {
  return AllocTempBody(reg_pool_->FPRegs,
             reg_pool_->num_fp_regs,
             &reg_pool_->next_fp_reg, true);
}

Mir2Lir::RegisterInfo* Mir2Lir::AllocLiveBody(RegisterInfo* p, int num_regs, int s_reg) {
  if (s_reg == -1)
    return NULL;
  for (int i = 0; i < num_regs; i++) {
    if (p[i].live && (p[i].s_reg == s_reg)) {
      if (p[i].is_temp)
        p[i].in_use = true;
      return &p[i];
    }
  }
  return NULL;
}

Mir2Lir::RegisterInfo* Mir2Lir::AllocLive(int s_reg, int reg_class) {
  RegisterInfo* res = NULL;
  switch (reg_class) {
    case kAnyReg:
      res = AllocLiveBody(reg_pool_->FPRegs,
                reg_pool_->num_fp_regs, s_reg);
      if (res)
        break;
      /* Intentional fallthrough */
    case kCoreReg:
      res = AllocLiveBody(reg_pool_->core_regs,
                reg_pool_->num_core_regs, s_reg);
      break;
    case kFPReg:
      res = AllocLiveBody(reg_pool_->FPRegs,
                reg_pool_->num_fp_regs, s_reg);
      break;
    default:
      LOG(FATAL) << "Invalid register type";
  }
  return res;
}

void Mir2Lir::FreeTemp(int reg) {
  RegisterInfo* p = reg_pool_->core_regs;
  int num_regs = reg_pool_->num_core_regs;
  for (int i = 0; i< num_regs; i++) {
    if (p[i].reg == reg) {
      if (p[i].is_temp) {
        p[i].in_use = false;
      }
      p[i].pair = false;
      return;
    }
  }
  p = reg_pool_->FPRegs;
  num_regs = reg_pool_->num_fp_regs;
  for (int i = 0; i< num_regs; i++) {
    if (p[i].reg == reg) {
      if (p[i].is_temp) {
        p[i].in_use = false;
      }
      p[i].pair = false;
      return;
    }
  }
  LOG(FATAL) << "Tried to free a non-existant temp: r" << reg;
}

Mir2Lir::RegisterInfo* Mir2Lir::IsLive(int reg) {
  RegisterInfo* p = reg_pool_->core_regs;
  int num_regs = reg_pool_->num_core_regs;
  for (int i = 0; i< num_regs; i++) {
    if (p[i].reg == reg) {
      return p[i].live ? &p[i] : NULL;
    }
  }
  p = reg_pool_->FPRegs;
  num_regs = reg_pool_->num_fp_regs;
  for (int i = 0; i< num_regs; i++) {
    if (p[i].reg == reg) {
      return p[i].live ? &p[i] : NULL;
    }
  }
  return NULL;
}

Mir2Lir::RegisterInfo* Mir2Lir::IsTemp(int reg) {
  RegisterInfo* p = GetRegInfo(reg);
  return (p->is_temp) ? p : NULL;
}

Mir2Lir::RegisterInfo* Mir2Lir::IsPromoted(int reg) {
  RegisterInfo* p = GetRegInfo(reg);
  return (p->is_temp) ? NULL : p;
}

bool Mir2Lir::IsDirty(int reg) {
  RegisterInfo* p = GetRegInfo(reg);
  return p->dirty;
}

/*
 * Similar to AllocTemp(), but forces the allocation of a specific
 * register.  No check is made to see if the register was previously
 * allocated.  Use with caution.
 */
void Mir2Lir::LockTemp(int reg) {
  RegisterInfo* p = reg_pool_->core_regs;
  int num_regs = reg_pool_->num_core_regs;
  for (int i = 0; i< num_regs; i++) {
    if (p[i].reg == reg) {
      DCHECK(p[i].is_temp);
      p[i].in_use = true;
      p[i].live = false;
      return;
    }
  }
  p = reg_pool_->FPRegs;
  num_regs = reg_pool_->num_fp_regs;
  for (int i = 0; i< num_regs; i++) {
    if (p[i].reg == reg) {
      DCHECK(p[i].is_temp);
      p[i].in_use = true;
      p[i].live = false;
      return;
    }
  }
  LOG(FATAL) << "Tried to lock a non-existant temp: r" << reg;
}

void Mir2Lir::ResetDef(int reg) {
  ResetDefBody(GetRegInfo(reg));
}

void Mir2Lir::NullifyRange(LIR *start, LIR *finish, int s_reg1, int s_reg2) {
  if (start && finish) {
    LIR *p;
    DCHECK_EQ(s_reg1, s_reg2);
    for (p = start; ; p = p->next) {
      NopLIR(p);
      if (p == finish)
        break;
    }
  }
}

/*
 * Mark the beginning and end LIR of a def sequence.  Note that
 * on entry start points to the LIR prior to the beginning of the
 * sequence.
 */
void Mir2Lir::MarkDef(RegLocation rl, LIR *start, LIR *finish) {
  DCHECK(!rl.wide);
  DCHECK(start && start->next);
  DCHECK(finish);
  RegisterInfo* p = GetRegInfo(rl.low_reg);
  p->def_start = start->next;
  p->def_end = finish;
}

/*
 * Mark the beginning and end LIR of a def sequence.  Note that
 * on entry start points to the LIR prior to the beginning of the
 * sequence.
 */
void Mir2Lir::MarkDefWide(RegLocation rl, LIR *start, LIR *finish) {
  DCHECK(rl.wide);
  DCHECK(start && start->next);
  DCHECK(finish);
  RegisterInfo* p = GetRegInfo(rl.low_reg);
  ResetDef(rl.high_reg);  // Only track low of pair
  p->def_start = start->next;
  p->def_end = finish;
}

RegLocation Mir2Lir::WideToNarrow(RegLocation rl) {
  DCHECK(rl.wide);
  if (rl.location == kLocPhysReg) {
    RegisterInfo* info_lo = GetRegInfo(rl.low_reg);
    RegisterInfo* info_hi = GetRegInfo(rl.high_reg);
    if (info_lo->is_temp) {
      info_lo->pair = false;
      info_lo->def_start = NULL;
      info_lo->def_end = NULL;
    }
    if (info_hi->is_temp) {
      info_hi->pair = false;
      info_hi->def_start = NULL;
      info_hi->def_end = NULL;
    }
  }
  rl.wide = false;
  return rl;
}

void Mir2Lir::ResetDefLoc(RegLocation rl) {
  DCHECK(!rl.wide);
  RegisterInfo* p = IsTemp(rl.low_reg);
  if (p && !(cu_->disable_opt & (1 << kSuppressLoads))) {
    DCHECK(!p->pair);
    NullifyRange(p->def_start, p->def_end, p->s_reg, rl.s_reg_low);
  }
  ResetDef(rl.low_reg);
}

void Mir2Lir::ResetDefLocWide(RegLocation rl) {
  DCHECK(rl.wide);
  RegisterInfo* p_low = IsTemp(rl.low_reg);
  RegisterInfo* p_high = IsTemp(rl.high_reg);
  if (p_low && !(cu_->disable_opt & (1 << kSuppressLoads))) {
    DCHECK(p_low->pair);
    NullifyRange(p_low->def_start, p_low->def_end, p_low->s_reg, rl.s_reg_low);
  }
  if (p_high && !(cu_->disable_opt & (1 << kSuppressLoads))) {
    DCHECK(p_high->pair);
  }
  ResetDef(rl.low_reg);
  ResetDef(rl.high_reg);
}

void Mir2Lir::ResetDefTracking() {
  for (int i = 0; i< reg_pool_->num_core_regs; i++) {
    ResetDefBody(&reg_pool_->core_regs[i]);
  }
  for (int i = 0; i< reg_pool_->num_fp_regs; i++) {
    ResetDefBody(&reg_pool_->FPRegs[i]);
  }
}

void Mir2Lir::ClobberAllRegs() {
  for (int i = 0; i< reg_pool_->num_core_regs; i++) {
    ClobberBody(&reg_pool_->core_regs[i]);
  }
  for (int i = 0; i< reg_pool_->num_fp_regs; i++) {
    ClobberBody(&reg_pool_->FPRegs[i]);
  }
}

// Make sure nothing is live and dirty
void Mir2Lir::FlushAllRegsBody(RegisterInfo* info, int num_regs) {
  for (int i = 0; i < num_regs; i++) {
    if (info[i].live && info[i].dirty) {
      if (info[i].pair) {
        FlushRegWide(info[i].reg, info[i].partner);
      } else {
        FlushReg(info[i].reg);
      }
    }
  }
}

void Mir2Lir::FlushAllRegs() {
  FlushAllRegsBody(reg_pool_->core_regs,
           reg_pool_->num_core_regs);
  FlushAllRegsBody(reg_pool_->FPRegs,
           reg_pool_->num_fp_regs);
  ClobberAllRegs();
}


// TUNING: rewrite all of this reg stuff.  Probably use an attribute table
bool Mir2Lir::RegClassMatches(int reg_class, int reg) {
  if (reg_class == kAnyReg) {
    return true;
  } else if (reg_class == kCoreReg) {
    return !IsFpReg(reg);
  } else {
    return IsFpReg(reg);
  }
}

void Mir2Lir::MarkLive(int reg, int s_reg) {
  RegisterInfo* info = GetRegInfo(reg);
  if ((info->reg == reg) && (info->s_reg == s_reg) && info->live) {
    return;  /* already live */
  } else if (s_reg != INVALID_SREG) {
    ClobberSReg(s_reg);
    if (info->is_temp) {
      info->live = true;
    }
  } else {
    /* Can't be live if no associated s_reg */
    DCHECK(info->is_temp);
    info->live = false;
  }
  info->s_reg = s_reg;
}

void Mir2Lir::MarkTemp(int reg) {
  RegisterInfo* info = GetRegInfo(reg);
  info->is_temp = true;
}

void Mir2Lir::UnmarkTemp(int reg) {
  RegisterInfo* info = GetRegInfo(reg);
  info->is_temp = false;
}

void Mir2Lir::MarkPair(int low_reg, int high_reg) {
  RegisterInfo* info_lo = GetRegInfo(low_reg);
  RegisterInfo* info_hi = GetRegInfo(high_reg);
  info_lo->pair = info_hi->pair = true;
  info_lo->partner = high_reg;
  info_hi->partner = low_reg;
}

void Mir2Lir::MarkClean(RegLocation loc) {
  RegisterInfo* info = GetRegInfo(loc.low_reg);
  info->dirty = false;
  if (loc.wide) {
    info = GetRegInfo(loc.high_reg);
    info->dirty = false;
  }
}

void Mir2Lir::MarkDirty(RegLocation loc) {
  if (loc.home) {
    // If already home, can't be dirty
    return;
  }
  RegisterInfo* info = GetRegInfo(loc.low_reg);
  info->dirty = true;
  if (loc.wide) {
    info = GetRegInfo(loc.high_reg);
    info->dirty = true;
  }
}

void Mir2Lir::MarkInUse(int reg) {
    RegisterInfo* info = GetRegInfo(reg);
    info->in_use = true;
}

void Mir2Lir::CopyRegInfo(int new_reg, int old_reg) {
  RegisterInfo* new_info = GetRegInfo(new_reg);
  RegisterInfo* old_info = GetRegInfo(old_reg);
  // Target temp status must not change
  bool is_temp = new_info->is_temp;
  *new_info = *old_info;
  // Restore target's temp status
  new_info->is_temp = is_temp;
  new_info->reg = new_reg;
}

bool Mir2Lir::CheckCorePoolSanity() {
  for (static int i = 0; i < reg_pool_->num_core_regs; i++) {
    if (reg_pool_->core_regs[i].pair) {
      static int my_reg = reg_pool_->core_regs[i].reg;
      static int my_sreg = reg_pool_->core_regs[i].s_reg;
      static int partner_reg = reg_pool_->core_regs[i].partner;
      static RegisterInfo* partner = GetRegInfo(partner_reg);
      DCHECK(partner != NULL);
      DCHECK(partner->pair);
      DCHECK_EQ(my_reg, partner->partner);
      static int partner_sreg = partner->s_reg;
      if (my_sreg == INVALID_SREG) {
        DCHECK_EQ(partner_sreg, INVALID_SREG);
      } else {
        int diff = my_sreg - partner_sreg;
        DCHECK((diff == -1) || (diff == 1));
      }
    }
    if (!reg_pool_->core_regs[i].live) {
      DCHECK(reg_pool_->core_regs[i].def_start == NULL);
      DCHECK(reg_pool_->core_regs[i].def_end == NULL);
    }
  }
  return true;
}

/*
 * Return an updated location record with current in-register status.
 * If the value lives in live temps, reflect that fact.  No code
 * is generated.  If the live value is part of an older pair,
 * clobber both low and high.
 * TUNING: clobbering both is a bit heavy-handed, but the alternative
 * is a bit complex when dealing with FP regs.  Examine code to see
 * if it's worthwhile trying to be more clever here.
 */

RegLocation Mir2Lir::UpdateLoc(RegLocation loc) {
  DCHECK(!loc.wide);
  DCHECK(CheckCorePoolSanity());
  if (loc.location != kLocPhysReg) {
    DCHECK((loc.location == kLocDalvikFrame) ||
         (loc.location == kLocCompilerTemp));
    RegisterInfo* info_lo = AllocLive(loc.s_reg_low, kAnyReg);
    if (info_lo) {
      if (info_lo->pair) {
        Clobber(info_lo->reg);
        Clobber(info_lo->partner);
        FreeTemp(info_lo->reg);
      } else {
        loc.low_reg = info_lo->reg;
        loc.location = kLocPhysReg;
      }
    }
  }

  return loc;
}

/* see comments for update_loc */
RegLocation Mir2Lir::UpdateLocWide(RegLocation loc) {
  DCHECK(loc.wide);
  DCHECK(CheckCorePoolSanity());
  if (loc.location != kLocPhysReg) {
    DCHECK((loc.location == kLocDalvikFrame) ||
         (loc.location == kLocCompilerTemp));
    // Are the dalvik regs already live in physical registers?
    RegisterInfo* info_lo = AllocLive(loc.s_reg_low, kAnyReg);
    RegisterInfo* info_hi = AllocLive(GetSRegHi(loc.s_reg_low), kAnyReg);
    bool match = true;
    match = match && (info_lo != NULL);
    match = match && (info_hi != NULL);
    // Are they both core or both FP?
    match = match && (IsFpReg(info_lo->reg) == IsFpReg(info_hi->reg));
    // If a pair of floating point singles, are they properly aligned?
    if (match && IsFpReg(info_lo->reg)) {
      match &= ((info_lo->reg & 0x1) == 0);
      match &= ((info_hi->reg - info_lo->reg) == 1);
    }
    // If previously used as a pair, it is the same pair?
    if (match && (info_lo->pair || info_hi->pair)) {
      match = (info_lo->pair == info_hi->pair);
      match &= ((info_lo->reg == info_hi->partner) &&
            (info_hi->reg == info_lo->partner));
    }
    if (match) {
      // Can reuse - update the register usage info
      loc.low_reg = info_lo->reg;
      loc.high_reg = info_hi->reg;
      loc.location = kLocPhysReg;
      MarkPair(loc.low_reg, loc.high_reg);
      DCHECK(!IsFpReg(loc.low_reg) || ((loc.low_reg & 0x1) == 0));
      return loc;
    }
    // Can't easily reuse - clobber and free any overlaps
    if (info_lo) {
      Clobber(info_lo->reg);
      FreeTemp(info_lo->reg);
      if (info_lo->pair)
        Clobber(info_lo->partner);
    }
    if (info_hi) {
      Clobber(info_hi->reg);
      FreeTemp(info_hi->reg);
      if (info_hi->pair)
        Clobber(info_hi->partner);
    }
  }
  return loc;
}


/* For use in cases we don't know (or care) width */
RegLocation Mir2Lir::UpdateRawLoc(RegLocation loc) {
  if (loc.wide)
    return UpdateLocWide(loc);
  else
    return UpdateLoc(loc);
}

RegLocation Mir2Lir::EvalLocWide(RegLocation loc, int reg_class, bool update) {
  DCHECK(loc.wide);
  int new_regs;
  int low_reg;
  int high_reg;

  loc = UpdateLocWide(loc);

  /* If already in registers, we can assume proper form.  Right reg class? */
  if (loc.location == kLocPhysReg) {
    DCHECK_EQ(IsFpReg(loc.low_reg), IsFpReg(loc.high_reg));
    DCHECK(!IsFpReg(loc.low_reg) || ((loc.low_reg & 0x1) == 0));
    if (!RegClassMatches(reg_class, loc.low_reg)) {
      /* Wrong register class.  Reallocate and copy */
      new_regs = AllocTypedTempPair(loc.fp, reg_class);
      low_reg = new_regs & 0xff;
      high_reg = (new_regs >> 8) & 0xff;
      OpRegCopyWide(low_reg, high_reg, loc.low_reg, loc.high_reg);
      CopyRegInfo(low_reg, loc.low_reg);
      CopyRegInfo(high_reg, loc.high_reg);
      Clobber(loc.low_reg);
      Clobber(loc.high_reg);
      loc.low_reg = low_reg;
      loc.high_reg = high_reg;
      MarkPair(loc.low_reg, loc.high_reg);
      DCHECK(!IsFpReg(loc.low_reg) || ((loc.low_reg & 0x1) == 0));
    }
    return loc;
  }

  DCHECK_NE(loc.s_reg_low, INVALID_SREG);
  DCHECK_NE(GetSRegHi(loc.s_reg_low), INVALID_SREG);

  new_regs = AllocTypedTempPair(loc.fp, reg_class);
  loc.low_reg = new_regs & 0xff;
  loc.high_reg = (new_regs >> 8) & 0xff;

  MarkPair(loc.low_reg, loc.high_reg);
  if (update) {
    loc.location = kLocPhysReg;
    MarkLive(loc.low_reg, loc.s_reg_low);
    MarkLive(loc.high_reg, GetSRegHi(loc.s_reg_low));
  }
  DCHECK(!IsFpReg(loc.low_reg) || ((loc.low_reg & 0x1) == 0));
  return loc;
}

RegLocation Mir2Lir::EvalLoc(RegLocation loc, int reg_class, bool update) {
  int new_reg;

  if (loc.wide)
    return EvalLocWide(loc, reg_class, update);

  loc = UpdateLoc(loc);

  if (loc.location == kLocPhysReg) {
    if (!RegClassMatches(reg_class, loc.low_reg)) {
      /* Wrong register class.  Realloc, copy and transfer ownership */
      new_reg = AllocTypedTemp(loc.fp, reg_class);
      OpRegCopy(new_reg, loc.low_reg);
      CopyRegInfo(new_reg, loc.low_reg);
      Clobber(loc.low_reg);
      loc.low_reg = new_reg;
    }
    return loc;
  }

  DCHECK_NE(loc.s_reg_low, INVALID_SREG);

  new_reg = AllocTypedTemp(loc.fp, reg_class);
  loc.low_reg = new_reg;

  if (update) {
    loc.location = kLocPhysReg;
    MarkLive(loc.low_reg, loc.s_reg_low);
  }
  return loc;
}

/* USE SSA names to count references of base Dalvik v_regs. */
void Mir2Lir::CountRefs(RefCounts* core_counts, RefCounts* fp_counts) {
  for (int i = 0; i < mir_graph_->GetNumSSARegs(); i++) {
    RegLocation loc = mir_graph_->reg_location_[i];
    RefCounts* counts = loc.fp ? fp_counts : core_counts;
    int p_map_idx = SRegToPMap(loc.s_reg_low);
    // Don't count easily regenerated immediates
    if (loc.fp || !IsInexpensiveConstant(loc)) {
      counts[p_map_idx].count += mir_graph_->GetUseCount(i);
    }
    if (loc.wide && loc.fp && !loc.high_word) {
      counts[p_map_idx].double_start = true;
    }
  }
}

/* qsort callback function, sort descending */
static int SortCounts(const void *val1, const void *val2) {
  const Mir2Lir::RefCounts* op1 = reinterpret_cast<const Mir2Lir::RefCounts*>(val1);
  const Mir2Lir::RefCounts* op2 = reinterpret_cast<const Mir2Lir::RefCounts*>(val2);
  // Note that we fall back to sorting on reg so we get stable output
  // on differing qsort implementations (such as on host and target or
  // between local host and build servers).
  return (op1->count == op2->count)
          ? (op1->s_reg - op2->s_reg)
          : (op1->count < op2->count ? 1 : -1);
}

void Mir2Lir::DumpCounts(const RefCounts* arr, int size, const char* msg) {
  LOG(INFO) << msg;
  for (int i = 0; i < size; i++) {
    LOG(INFO) << "s_reg[" << arr[i].s_reg << "]: " << arr[i].count;
  }
}

/*
 * Note: some portions of this code required even if the kPromoteRegs
 * optimization is disabled.
 */
void Mir2Lir::DoPromotion() {
  int reg_bias = cu_->num_compiler_temps + 1;
  int dalvik_regs = cu_->num_dalvik_registers;
  int num_regs = dalvik_regs + reg_bias;
  const int promotion_threshold = 1;

  // Allow target code to add any special registers
  AdjustSpillMask();

  /*
   * Simple register promotion. Just do a static count of the uses
   * of Dalvik registers.  Note that we examine the SSA names, but
   * count based on original Dalvik register name.  Count refs
   * separately based on type in order to give allocation
   * preference to fp doubles - which must be allocated sequential
   * physical single fp registers started with an even-numbered
   * reg.
   * TUNING: replace with linear scan once we have the ability
   * to describe register live ranges for GC.
   */
  RefCounts *core_regs =
      static_cast<RefCounts*>(arena_->Alloc(sizeof(RefCounts) * num_regs,
                                            ArenaAllocator::kAllocRegAlloc));
  RefCounts *FpRegs =
      static_cast<RefCounts *>(arena_->Alloc(sizeof(RefCounts) * num_regs,
                                             ArenaAllocator::kAllocRegAlloc));
  // Set ssa names for original Dalvik registers
  for (int i = 0; i < dalvik_regs; i++) {
    core_regs[i].s_reg = FpRegs[i].s_reg = i;
  }
  // Set ssa name for Method*
  core_regs[dalvik_regs].s_reg = mir_graph_->GetMethodSReg();
  FpRegs[dalvik_regs].s_reg = mir_graph_->GetMethodSReg();  // For consistecy
  // Set ssa names for compiler_temps
  for (int i = 1; i <= cu_->num_compiler_temps; i++) {
    CompilerTemp* ct = mir_graph_->compiler_temps_.Get(i);
    core_regs[dalvik_regs + i].s_reg = ct->s_reg;
    FpRegs[dalvik_regs + i].s_reg = ct->s_reg;
  }

  // Sum use counts of SSA regs by original Dalvik vreg.
  CountRefs(core_regs, FpRegs);

  /*
   * Ideally, we'd allocate doubles starting with an even-numbered
   * register.  Bias the counts to try to allocate any vreg that's
   * used as the start of a pair first.
   */
  for (int i = 0; i < num_regs; i++) {
    if (FpRegs[i].double_start) {
      FpRegs[i].count *= 2;
    }
  }

  // Sort the count arrays
  qsort(core_regs, num_regs, sizeof(RefCounts), SortCounts);
  qsort(FpRegs, num_regs, sizeof(RefCounts), SortCounts);

  if (cu_->verbose) {
    DumpCounts(core_regs, num_regs, "Core regs after sort");
    DumpCounts(FpRegs, num_regs, "Fp regs after sort");
  }

  if (!(cu_->disable_opt & (1 << kPromoteRegs))) {
    // Promote FpRegs
    for (int i = 0; (i < num_regs) && (FpRegs[i].count >= promotion_threshold); i++) {
      int p_map_idx = SRegToPMap(FpRegs[i].s_reg);
      if (promotion_map_[p_map_idx].fp_location != kLocPhysReg) {
        int reg = AllocPreservedFPReg(FpRegs[i].s_reg,
          FpRegs[i].double_start);
        if (reg < 0) {
          break;  // No more left
        }
      }
    }

    // Promote core regs
    for (int i = 0; (i < num_regs) &&
            (core_regs[i].count >= promotion_threshold); i++) {
      int p_map_idx = SRegToPMap(core_regs[i].s_reg);
      if (promotion_map_[p_map_idx].core_location !=
          kLocPhysReg) {
        int reg = AllocPreservedCoreReg(core_regs[i].s_reg);
        if (reg < 0) {
           break;  // No more left
        }
      }
    }
  }

  // Now, update SSA names to new home locations
  for (int i = 0; i < mir_graph_->GetNumSSARegs(); i++) {
    RegLocation *curr = &mir_graph_->reg_location_[i];
    int p_map_idx = SRegToPMap(curr->s_reg_low);
    if (!curr->wide) {
      if (curr->fp) {
        if (promotion_map_[p_map_idx].fp_location == kLocPhysReg) {
          curr->location = kLocPhysReg;
          curr->low_reg = promotion_map_[p_map_idx].FpReg;
          curr->home = true;
        }
      } else {
        if (promotion_map_[p_map_idx].core_location == kLocPhysReg) {
          curr->location = kLocPhysReg;
          curr->low_reg = promotion_map_[p_map_idx].core_reg;
          curr->home = true;
        }
      }
      curr->high_reg = INVALID_REG;
    } else {
      if (curr->high_word) {
        continue;
      }
      if (curr->fp) {
        if ((promotion_map_[p_map_idx].fp_location == kLocPhysReg) &&
          (promotion_map_[p_map_idx+1].fp_location ==
          kLocPhysReg)) {
          int low_reg = promotion_map_[p_map_idx].FpReg;
          int high_reg = promotion_map_[p_map_idx+1].FpReg;
          // Doubles require pair of singles starting at even reg
          if (((low_reg & 0x1) == 0) && ((low_reg + 1) == high_reg)) {
            curr->location = kLocPhysReg;
            curr->low_reg = low_reg;
            curr->high_reg = high_reg;
            curr->home = true;
          }
        }
      } else {
        if ((promotion_map_[p_map_idx].core_location == kLocPhysReg)
           && (promotion_map_[p_map_idx+1].core_location ==
           kLocPhysReg)) {
          curr->location = kLocPhysReg;
          curr->low_reg = promotion_map_[p_map_idx].core_reg;
          curr->high_reg = promotion_map_[p_map_idx+1].core_reg;
          curr->home = true;
        }
      }
    }
  }
  if (cu_->verbose) {
    DumpPromotionMap();
  }
}

/* Returns sp-relative offset in bytes for a VReg */
int Mir2Lir::VRegOffset(int v_reg) {
  return StackVisitor::GetVRegOffset(cu_->code_item, core_spill_mask_,
                                     fp_spill_mask_, frame_size_, v_reg);
}

/* Returns sp-relative offset in bytes for a SReg */
int Mir2Lir::SRegOffset(int s_reg) {
  return VRegOffset(mir_graph_->SRegToVReg(s_reg));
}

/* Mark register usage state and return long retloc */
RegLocation Mir2Lir::GetReturnWide(bool is_double) {
  RegLocation gpr_res = LocCReturnWide();
  RegLocation fpr_res = LocCReturnDouble();
  RegLocation res = is_double ? fpr_res : gpr_res;
  Clobber(res.low_reg);
  Clobber(res.high_reg);
  LockTemp(res.low_reg);
  LockTemp(res.high_reg);
  MarkPair(res.low_reg, res.high_reg);
  return res;
}

RegLocation Mir2Lir::GetReturn(bool is_float) {
  RegLocation gpr_res = LocCReturn();
  RegLocation fpr_res = LocCReturnFloat();
  RegLocation res = is_float ? fpr_res : gpr_res;
  Clobber(res.low_reg);
  if (cu_->instruction_set == kMips) {
    MarkInUse(res.low_reg);
  } else {
    LockTemp(res.low_reg);
  }
  return res;
}

void Mir2Lir::SimpleRegAlloc() {
  DoPromotion();

  if (cu_->verbose && !(cu_->disable_opt & (1 << kPromoteRegs))) {
    LOG(INFO) << "After Promotion";
    mir_graph_->DumpRegLocTable(mir_graph_->reg_location_, mir_graph_->GetNumSSARegs());
  }

  /* Set the frame size */
  frame_size_ = ComputeFrameSize();
}

/*
 * Get the "real" sreg number associated with an s_reg slot.  In general,
 * s_reg values passed through codegen are the SSA names created by
 * dataflow analysis and refer to slot numbers in the mir_graph_->reg_location
 * array.  However, renaming is accomplished by simply replacing RegLocation
 * entries in the reglocation[] array.  Therefore, when location
 * records for operands are first created, we need to ask the locRecord
 * identified by the dataflow pass what it's new name is.
 */
int Mir2Lir::GetSRegHi(int lowSreg) {
  return (lowSreg == INVALID_SREG) ? INVALID_SREG : lowSreg + 1;
}

bool Mir2Lir::oat_live_out(int s_reg) {
  // For now.
  return true;
}

int Mir2Lir::oatSSASrc(MIR* mir, int num) {
  DCHECK_GT(mir->ssa_rep->num_uses, num);
  return mir->ssa_rep->uses[num];
}

}  // namespace art
