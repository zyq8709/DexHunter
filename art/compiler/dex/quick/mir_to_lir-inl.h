/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_QUICK_MIR_TO_LIR_INL_H_
#define ART_COMPILER_DEX_QUICK_MIR_TO_LIR_INL_H_

#include "mir_to_lir.h"

#include "dex/compiler_internals.h"

namespace art {

/* Mark a temp register as dead.  Does not affect allocation state. */
inline void Mir2Lir::ClobberBody(RegisterInfo* p) {
  if (p->is_temp) {
    DCHECK(!(p->live && p->dirty))  << "Live & dirty temp in clobber";
    p->live = false;
    p->s_reg = INVALID_SREG;
    p->def_start = NULL;
    p->def_end = NULL;
    if (p->pair) {
      p->pair = false;
      Clobber(p->partner);
    }
  }
}

inline LIR* Mir2Lir::RawLIR(int dalvik_offset, int opcode, int op0,
                            int op1, int op2, int op3, int op4, LIR* target) {
  LIR* insn = static_cast<LIR*>(arena_->Alloc(sizeof(LIR), ArenaAllocator::kAllocLIR));
  insn->dalvik_offset = dalvik_offset;
  insn->opcode = opcode;
  insn->operands[0] = op0;
  insn->operands[1] = op1;
  insn->operands[2] = op2;
  insn->operands[3] = op3;
  insn->operands[4] = op4;
  insn->target = target;
  SetupResourceMasks(insn);
  if ((opcode == kPseudoTargetLabel) || (opcode == kPseudoSafepointPC) ||
      (opcode == kPseudoExportedPC)) {
    // Always make labels scheduling barriers
    insn->use_mask = insn->def_mask = ENCODE_ALL;
  }
  return insn;
}

/*
 * The following are building blocks to construct low-level IRs with 0 - 4
 * operands.
 */
inline LIR* Mir2Lir::NewLIR0(int opcode) {
  DCHECK(is_pseudo_opcode(opcode) || (GetTargetInstFlags(opcode) & NO_OPERAND))
      << GetTargetInstName(opcode) << " " << opcode << " "
      << PrettyMethod(cu_->method_idx, *cu_->dex_file) << " "
      << current_dalvik_offset_;
  LIR* insn = RawLIR(current_dalvik_offset_, opcode);
  AppendLIR(insn);
  return insn;
}

inline LIR* Mir2Lir::NewLIR1(int opcode, int dest) {
  DCHECK(is_pseudo_opcode(opcode) || (GetTargetInstFlags(opcode) & IS_UNARY_OP))
      << GetTargetInstName(opcode) << " " << opcode << " "
      << PrettyMethod(cu_->method_idx, *cu_->dex_file) << " "
      << current_dalvik_offset_;
  LIR* insn = RawLIR(current_dalvik_offset_, opcode, dest);
  AppendLIR(insn);
  return insn;
}

inline LIR* Mir2Lir::NewLIR2(int opcode, int dest, int src1) {
  DCHECK(is_pseudo_opcode(opcode) || (GetTargetInstFlags(opcode) & IS_BINARY_OP))
      << GetTargetInstName(opcode) << " " << opcode << " "
      << PrettyMethod(cu_->method_idx, *cu_->dex_file) << " "
      << current_dalvik_offset_;
  LIR* insn = RawLIR(current_dalvik_offset_, opcode, dest, src1);
  AppendLIR(insn);
  return insn;
}

inline LIR* Mir2Lir::NewLIR3(int opcode, int dest, int src1, int src2) {
  DCHECK(is_pseudo_opcode(opcode) || (GetTargetInstFlags(opcode) & IS_TERTIARY_OP))
      << GetTargetInstName(opcode) << " " << opcode << " "
      << PrettyMethod(cu_->method_idx, *cu_->dex_file) << " "
      << current_dalvik_offset_;
  LIR* insn = RawLIR(current_dalvik_offset_, opcode, dest, src1, src2);
  AppendLIR(insn);
  return insn;
}

inline LIR* Mir2Lir::NewLIR4(int opcode, int dest, int src1, int src2, int info) {
  DCHECK(is_pseudo_opcode(opcode) || (GetTargetInstFlags(opcode) & IS_QUAD_OP))
      << GetTargetInstName(opcode) << " " << opcode << " "
      << PrettyMethod(cu_->method_idx, *cu_->dex_file) << " "
      << current_dalvik_offset_;
  LIR* insn = RawLIR(current_dalvik_offset_, opcode, dest, src1, src2, info);
  AppendLIR(insn);
  return insn;
}

inline LIR* Mir2Lir::NewLIR5(int opcode, int dest, int src1, int src2, int info1,
                             int info2) {
  DCHECK(is_pseudo_opcode(opcode) || (GetTargetInstFlags(opcode) & IS_QUIN_OP))
      << GetTargetInstName(opcode) << " " << opcode << " "
      << PrettyMethod(cu_->method_idx, *cu_->dex_file) << " "
      << current_dalvik_offset_;
  LIR* insn = RawLIR(current_dalvik_offset_, opcode, dest, src1, src2, info1, info2);
  AppendLIR(insn);
  return insn;
}

/*
 * Mark the corresponding bit(s).
 */
inline void Mir2Lir::SetupRegMask(uint64_t* mask, int reg) {
  *mask |= GetRegMaskCommon(reg);
}

/*
 * Set up the proper fields in the resource mask
 */
inline void Mir2Lir::SetupResourceMasks(LIR* lir) {
  int opcode = lir->opcode;

  if (opcode <= 0) {
    lir->use_mask = lir->def_mask = 0;
    return;
  }

  uint64_t flags = GetTargetInstFlags(opcode);

  if (flags & NEEDS_FIXUP) {
    lir->flags.pcRelFixup = true;
  }

  /* Get the starting size of the instruction's template */
  lir->flags.size = GetInsnSize(lir);

  /* Set up the mask for resources that are updated */
  if (flags & (IS_LOAD | IS_STORE)) {
    /* Default to heap - will catch specialized classes later */
    SetMemRefType(lir, flags & IS_LOAD, kHeapRef);
  }

  /*
   * Conservatively assume the branch here will call out a function that in
   * turn will trash everything.
   */
  if (flags & IS_BRANCH) {
    lir->def_mask = lir->use_mask = ENCODE_ALL;
    return;
  }

  if (flags & REG_DEF0) {
    SetupRegMask(&lir->def_mask, lir->operands[0]);
  }

  if (flags & REG_DEF1) {
    SetupRegMask(&lir->def_mask, lir->operands[1]);
  }


  if (flags & SETS_CCODES) {
    lir->def_mask |= ENCODE_CCODE;
  }

  if (flags & (REG_USE0 | REG_USE1 | REG_USE2 | REG_USE3)) {
    int i;

    for (i = 0; i < 4; i++) {
      if (flags & (1 << (kRegUse0 + i))) {
        SetupRegMask(&lir->use_mask, lir->operands[i]);
      }
    }
  }

  if (flags & USES_CCODES) {
    lir->use_mask |= ENCODE_CCODE;
  }

  // Handle target-specific actions
  SetupTargetResourceMasks(lir);
}

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_MIR_TO_LIR_INL_H_
