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

#include "dex/compiler_internals.h"

namespace art {

#define DEBUG_OPT(X)

/* Check RAW, WAR, and RAW dependency on the register operands */
#define CHECK_REG_DEP(use, def, check) ((def & check->use_mask) || \
                                        ((use | def) & check->def_mask))

/* Scheduler heuristics */
#define MAX_HOIST_DISTANCE 20
#define LDLD_DISTANCE 4
#define LD_LATENCY 2

static bool IsDalvikRegisterClobbered(LIR* lir1, LIR* lir2) {
  int reg1Lo = DECODE_ALIAS_INFO_REG(lir1->alias_info);
  int reg1Hi = reg1Lo + DECODE_ALIAS_INFO_WIDE(lir1->alias_info);
  int reg2Lo = DECODE_ALIAS_INFO_REG(lir2->alias_info);
  int reg2Hi = reg2Lo + DECODE_ALIAS_INFO_WIDE(lir2->alias_info);

  return (reg1Lo == reg2Lo) || (reg1Lo == reg2Hi) || (reg1Hi == reg2Lo);
}

/* Convert a more expensive instruction (ie load) into a move */
void Mir2Lir::ConvertMemOpIntoMove(LIR* orig_lir, int dest, int src) {
  /* Insert a move to replace the load */
  LIR* move_lir;
  move_lir = OpRegCopyNoInsert(dest, src);
  /*
   * Insert the converted instruction after the original since the
   * optimization is scannng in the top-down order and the new instruction
   * will need to be re-checked (eg the new dest clobbers the src used in
   * this_lir).
   */
  InsertLIRAfter(orig_lir, move_lir);
}

/*
 * Perform a pass of top-down walk, from the second-last instruction in the
 * superblock, to eliminate redundant loads and stores.
 *
 * An earlier load can eliminate a later load iff
 *   1) They are must-aliases
 *   2) The native register is not clobbered in between
 *   3) The memory location is not written to in between
 *
 * An earlier store can eliminate a later load iff
 *   1) They are must-aliases
 *   2) The native register is not clobbered in between
 *   3) The memory location is not written to in between
 *
 * A later store can be eliminated by an earlier store iff
 *   1) They are must-aliases
 *   2) The memory location is not written to in between
 */
void Mir2Lir::ApplyLoadStoreElimination(LIR* head_lir, LIR* tail_lir) {
  LIR* this_lir;

  if (head_lir == tail_lir) {
    return;
  }

  for (this_lir = PREV_LIR(tail_lir); this_lir != head_lir; this_lir = PREV_LIR(this_lir)) {
    if (is_pseudo_opcode(this_lir->opcode)) {
      continue;
    }

    int sink_distance = 0;

    uint64_t target_flags = GetTargetInstFlags(this_lir->opcode);

    /* Skip non-interesting instructions */
    if ((this_lir->flags.is_nop == true) ||
        (target_flags & IS_BRANCH) ||
        ((target_flags & (REG_DEF0 | REG_DEF1)) == (REG_DEF0 | REG_DEF1)) ||  // Skip wide loads.
        ((target_flags & (REG_USE0 | REG_USE1 | REG_USE2)) ==
         (REG_USE0 | REG_USE1 | REG_USE2)) ||  // Skip wide stores.
        !(target_flags & (IS_LOAD | IS_STORE))) {
      continue;
    }

    int native_reg_id;
    if (cu_->instruction_set == kX86) {
      // If x86, location differs depending on whether memory/reg operation.
      native_reg_id = (GetTargetInstFlags(this_lir->opcode) & IS_STORE) ? this_lir->operands[2]
          : this_lir->operands[0];
    } else {
      native_reg_id = this_lir->operands[0];
    }
    bool is_this_lir_load = GetTargetInstFlags(this_lir->opcode) & IS_LOAD;
    LIR* check_lir;
    /* Use the mem mask to determine the rough memory location */
    uint64_t this_mem_mask = (this_lir->use_mask | this_lir->def_mask) & ENCODE_MEM;

    /*
     * Currently only eliminate redundant ld/st for constant and Dalvik
     * register accesses.
     */
    if (!(this_mem_mask & (ENCODE_LITERAL | ENCODE_DALVIK_REG))) {
      continue;
    }

    uint64_t stop_def_reg_mask = this_lir->def_mask & ~ENCODE_MEM;
    uint64_t stop_use_reg_mask;
    if (cu_->instruction_set == kX86) {
      stop_use_reg_mask = (IS_BRANCH | this_lir->use_mask) & ~ENCODE_MEM;
    } else {
      /*
       * Add pc to the resource mask to prevent this instruction
       * from sinking past branch instructions. Also take out the memory
       * region bits since stop_mask is used to check data/control
       * dependencies.
       */
        stop_use_reg_mask = (GetPCUseDefEncoding() | this_lir->use_mask) & ~ENCODE_MEM;
    }

    for (check_lir = NEXT_LIR(this_lir); check_lir != tail_lir; check_lir = NEXT_LIR(check_lir)) {
      /*
       * Skip already dead instructions (whose dataflow information is
       * outdated and misleading).
       */
      if (check_lir->flags.is_nop || is_pseudo_opcode(check_lir->opcode)) {
        continue;
      }

      uint64_t check_mem_mask = (check_lir->use_mask | check_lir->def_mask) & ENCODE_MEM;
      uint64_t alias_condition = this_mem_mask & check_mem_mask;
      bool stop_here = false;

      /*
       * Potential aliases seen - check the alias relations
       */
      uint64_t check_flags = GetTargetInstFlags(check_lir->opcode);
      // TUNING: Support instructions with multiple register targets.
      if ((check_flags & (REG_DEF0 | REG_DEF1)) == (REG_DEF0 | REG_DEF1)) {
        stop_here = true;
      } else if (check_mem_mask != ENCODE_MEM && alias_condition != 0) {
        bool is_check_lir_load = check_flags & IS_LOAD;
        if  (alias_condition == ENCODE_LITERAL) {
          /*
           * Should only see literal loads in the instruction
           * stream.
           */
          DCHECK(!(check_flags & IS_STORE));
          /* Same value && same register type */
          if (check_lir->alias_info == this_lir->alias_info &&
              SameRegType(check_lir->operands[0], native_reg_id)) {
            /*
             * Different destination register - insert
             * a move
             */
            if (check_lir->operands[0] != native_reg_id) {
              ConvertMemOpIntoMove(check_lir, check_lir->operands[0], native_reg_id);
            }
            check_lir->flags.is_nop = true;
          }
        } else if (alias_condition == ENCODE_DALVIK_REG) {
          /* Must alias */
          if (check_lir->alias_info == this_lir->alias_info) {
            /* Only optimize compatible registers */
            bool reg_compatible = SameRegType(check_lir->operands[0], native_reg_id);
            if ((is_this_lir_load && is_check_lir_load) ||
                (!is_this_lir_load && is_check_lir_load)) {
              /* RAR or RAW */
              if (reg_compatible) {
                /*
                 * Different destination register -
                 * insert a move
                 */
                if (check_lir->operands[0] !=
                  native_reg_id) {
                  ConvertMemOpIntoMove(check_lir, check_lir->operands[0], native_reg_id);
                }
                check_lir->flags.is_nop = true;
              } else {
                /*
                 * Destinaions are of different types -
                 * something complicated going on so
                 * stop looking now.
                 */
                stop_here = true;
              }
            } else if (is_this_lir_load && !is_check_lir_load) {
              /* WAR - register value is killed */
              stop_here = true;
            } else if (!is_this_lir_load && !is_check_lir_load) {
              /* WAW - nuke the earlier store */
              this_lir->flags.is_nop = true;
              stop_here = true;
            }
          /* Partial overlap */
          } else if (IsDalvikRegisterClobbered(this_lir, check_lir)) {
            /*
             * It is actually ok to continue if check_lir
             * is a read. But it is hard to make a test
             * case for this so we just stop here to be
             * conservative.
             */
            stop_here = true;
          }
        }
        /* Memory content may be updated. Stop looking now. */
        if (stop_here) {
          break;
        /* The check_lir has been transformed - check the next one */
        } else if (check_lir->flags.is_nop) {
          continue;
        }
      }


      /*
       * this and check LIRs have no memory dependency. Now check if
       * their register operands have any RAW, WAR, and WAW
       * dependencies. If so, stop looking.
       */
      if (stop_here == false) {
        stop_here = CHECK_REG_DEP(stop_use_reg_mask, stop_def_reg_mask, check_lir);
      }

      if (stop_here == true) {
        if (cu_->instruction_set == kX86) {
          // Prevent stores from being sunk between ops that generate ccodes and
          // ops that use them.
          uint64_t flags = GetTargetInstFlags(check_lir->opcode);
          if (sink_distance > 0 && (flags & IS_BRANCH) && (flags & USES_CCODES)) {
            check_lir = PREV_LIR(check_lir);
            sink_distance--;
          }
        }
        DEBUG_OPT(dump_dependent_insn_pair(this_lir, check_lir, "REG CLOBBERED"));
        /* Only sink store instructions */
        if (sink_distance && !is_this_lir_load) {
          LIR* new_store_lir =
              static_cast<LIR*>(arena_->Alloc(sizeof(LIR), ArenaAllocator::kAllocLIR));
          *new_store_lir = *this_lir;
          /*
           * Stop point found - insert *before* the check_lir
           * since the instruction list is scanned in the
           * top-down order.
           */
          InsertLIRBefore(check_lir, new_store_lir);
          this_lir->flags.is_nop = true;
        }
        break;
      } else if (!check_lir->flags.is_nop) {
        sink_distance++;
      }
    }
  }
}

/*
 * Perform a pass of bottom-up walk, from the second instruction in the
 * superblock, to try to hoist loads to earlier slots.
 */
void Mir2Lir::ApplyLoadHoisting(LIR* head_lir, LIR* tail_lir) {
  LIR* this_lir, *check_lir;
  /*
   * Store the list of independent instructions that can be hoisted past.
   * Will decide the best place to insert later.
   */
  LIR* prev_inst_list[MAX_HOIST_DISTANCE];

  /* Empty block */
  if (head_lir == tail_lir) {
    return;
  }

  /* Start from the second instruction */
  for (this_lir = NEXT_LIR(head_lir); this_lir != tail_lir; this_lir = NEXT_LIR(this_lir)) {
    if (is_pseudo_opcode(this_lir->opcode)) {
      continue;
    }

    uint64_t target_flags = GetTargetInstFlags(this_lir->opcode);
    /* Skip non-interesting instructions */
    if ((this_lir->flags.is_nop == true) ||
        ((target_flags & (REG_DEF0 | REG_DEF1)) == (REG_DEF0 | REG_DEF1)) ||
        !(target_flags & IS_LOAD)) {
      continue;
    }

    uint64_t stop_use_all_mask = this_lir->use_mask;

    if (cu_->instruction_set != kX86) {
      /*
       * Branches for null/range checks are marked with the true resource
       * bits, and loads to Dalvik registers, constant pools, and non-alias
       * locations are safe to be hoisted. So only mark the heap references
       * conservatively here.
       */
      if (stop_use_all_mask & ENCODE_HEAP_REF) {
        stop_use_all_mask |= GetPCUseDefEncoding();
      }
    }

    /* Similar as above, but just check for pure register dependency */
    uint64_t stop_use_reg_mask = stop_use_all_mask & ~ENCODE_MEM;
    uint64_t stop_def_reg_mask = this_lir->def_mask & ~ENCODE_MEM;

    int next_slot = 0;
    bool stop_here = false;

    /* Try to hoist the load to a good spot */
    for (check_lir = PREV_LIR(this_lir); check_lir != head_lir; check_lir = PREV_LIR(check_lir)) {
      /*
       * Skip already dead instructions (whose dataflow information is
       * outdated and misleading).
       */
      if (check_lir->flags.is_nop) {
        continue;
      }

      uint64_t check_mem_mask = check_lir->def_mask & ENCODE_MEM;
      uint64_t alias_condition = stop_use_all_mask & check_mem_mask;
      stop_here = false;

      /* Potential WAR alias seen - check the exact relation */
      if (check_mem_mask != ENCODE_MEM && alias_condition != 0) {
        /* We can fully disambiguate Dalvik references */
        if (alias_condition == ENCODE_DALVIK_REG) {
          /* Must alias or partually overlap */
          if ((check_lir->alias_info == this_lir->alias_info) ||
            IsDalvikRegisterClobbered(this_lir, check_lir)) {
            stop_here = true;
          }
        /* Conservatively treat all heap refs as may-alias */
        } else {
          DCHECK_EQ(alias_condition, ENCODE_HEAP_REF);
          stop_here = true;
        }
        /* Memory content may be updated. Stop looking now. */
        if (stop_here) {
          prev_inst_list[next_slot++] = check_lir;
          break;
        }
      }

      if (stop_here == false) {
        stop_here = CHECK_REG_DEP(stop_use_reg_mask, stop_def_reg_mask,
                     check_lir);
      }

      /*
       * Store the dependent or non-pseudo/indepedent instruction to the
       * list.
       */
      if (stop_here || !is_pseudo_opcode(check_lir->opcode)) {
        prev_inst_list[next_slot++] = check_lir;
        if (next_slot == MAX_HOIST_DISTANCE) {
          break;
        }
      }

      /* Found a new place to put the load - move it here */
      if (stop_here == true) {
        DEBUG_OPT(dump_dependent_insn_pair(check_lir, this_lir "HOIST STOP"));
        break;
      }
    }

    /*
     * Reached the top - use head_lir as the dependent marker as all labels
     * are barriers.
     */
    if (stop_here == false && next_slot < MAX_HOIST_DISTANCE) {
      prev_inst_list[next_slot++] = head_lir;
    }

    /*
     * At least one independent instruction is found. Scan in the reversed
     * direction to find a beneficial slot.
     */
    if (next_slot >= 2) {
      int first_slot = next_slot - 2;
      int slot;
      LIR* dep_lir = prev_inst_list[next_slot-1];
      /* If there is ld-ld dependency, wait LDLD_DISTANCE cycles */
      if (!is_pseudo_opcode(dep_lir->opcode) &&
        (GetTargetInstFlags(dep_lir->opcode) & IS_LOAD)) {
        first_slot -= LDLD_DISTANCE;
      }
      /*
       * Make sure we check slot >= 0 since first_slot may be negative
       * when the loop is first entered.
       */
      for (slot = first_slot; slot >= 0; slot--) {
        LIR* cur_lir = prev_inst_list[slot];
        LIR* prev_lir = prev_inst_list[slot+1];

        /* Check the highest instruction */
        if (prev_lir->def_mask == ENCODE_ALL) {
          /*
           * If the first instruction is a load, don't hoist anything
           * above it since it is unlikely to be beneficial.
           */
          if (GetTargetInstFlags(cur_lir->opcode) & IS_LOAD) {
            continue;
          }
          /*
           * If the remaining number of slots is less than LD_LATENCY,
           * insert the hoisted load here.
           */
          if (slot < LD_LATENCY) {
            break;
          }
        }

        // Don't look across a barrier label
        if ((prev_lir->opcode == kPseudoTargetLabel) ||
            (prev_lir->opcode == kPseudoSafepointPC) ||
            (prev_lir->opcode == kPseudoBarrier)) {
          break;
        }

        /*
         * Try to find two instructions with load/use dependency until
         * the remaining instructions are less than LD_LATENCY.
         */
        bool prev_is_load = is_pseudo_opcode(prev_lir->opcode) ? false :
            (GetTargetInstFlags(prev_lir->opcode) & IS_LOAD);
        if (((cur_lir->use_mask & prev_lir->def_mask) && prev_is_load) || (slot < LD_LATENCY)) {
          break;
        }
      }

      /* Found a slot to hoist to */
      if (slot >= 0) {
        LIR* cur_lir = prev_inst_list[slot];
        LIR* new_load_lir =
          static_cast<LIR*>(arena_->Alloc(sizeof(LIR), ArenaAllocator::kAllocLIR));
        *new_load_lir = *this_lir;
        /*
         * Insertion is guaranteed to succeed since check_lir
         * is never the first LIR on the list
         */
        InsertLIRBefore(cur_lir, new_load_lir);
        this_lir->flags.is_nop = true;
      }
    }
  }
}

void Mir2Lir::ApplyLocalOptimizations(LIR* head_lir, LIR* tail_lir) {
  if (!(cu_->disable_opt & (1 << kLoadStoreElimination))) {
    ApplyLoadStoreElimination(head_lir, tail_lir);
  }
  if (!(cu_->disable_opt & (1 << kLoadHoisting))) {
    ApplyLoadHoisting(head_lir, tail_lir);
  }
}

/*
 * Nop any unconditional branches that go to the next instruction.
 * Note: new redundant branches may be inserted later, and we'll
 * use a check in final instruction assembly to nop those out.
 */
void Mir2Lir::RemoveRedundantBranches() {
  LIR* this_lir;

  for (this_lir = first_lir_insn_; this_lir != last_lir_insn_; this_lir = NEXT_LIR(this_lir)) {
    /* Branch to the next instruction */
    if (IsUnconditionalBranch(this_lir)) {
      LIR* next_lir = this_lir;

      while (true) {
        next_lir = NEXT_LIR(next_lir);

        /*
         * Is the branch target the next instruction?
         */
        if (next_lir == this_lir->target) {
          this_lir->flags.is_nop = true;
          break;
        }

        /*
         * Found real useful stuff between the branch and the target.
         * Need to explicitly check the last_lir_insn_ here because it
         * might be the last real instruction.
         */
        if (!is_pseudo_opcode(next_lir->opcode) ||
          (next_lir == last_lir_insn_))
          break;
      }
    }
  }
}

}  // namespace art
