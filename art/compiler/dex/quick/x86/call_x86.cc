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
#include "x86_lir.h"

namespace art {

void X86Mir2Lir::GenSpecialCase(BasicBlock* bb, MIR* mir,
                                SpecialCaseHandler special_case) {
  // TODO
}

/*
 * The sparse table in the literal pool is an array of <key,displacement>
 * pairs.
 */
void X86Mir2Lir::GenSparseSwitch(MIR* mir, uint32_t table_offset,
                                 RegLocation rl_src) {
  const uint16_t* table = cu_->insns + current_dalvik_offset_ + table_offset;
  if (cu_->verbose) {
    DumpSparseSwitchTable(table);
  }
  int entries = table[1];
  const int* keys = reinterpret_cast<const int*>(&table[2]);
  const int* targets = &keys[entries];
  rl_src = LoadValue(rl_src, kCoreReg);
  for (int i = 0; i < entries; i++) {
    int key = keys[i];
    BasicBlock* case_block =
        mir_graph_->FindBlock(current_dalvik_offset_ + targets[i]);
    OpCmpImmBranch(kCondEq, rl_src.low_reg, key,
                   &block_label_list_[case_block->id]);
  }
}

/*
 * Code pattern will look something like:
 *
 * mov  r_val, ..
 * call 0
 * pop  r_start_of_method
 * sub  r_start_of_method, ..
 * mov  r_key_reg, r_val
 * sub  r_key_reg, low_key
 * cmp  r_key_reg, size-1  ; bound check
 * ja   done
 * mov  r_disp, [r_start_of_method + r_key_reg * 4 + table_offset]
 * add  r_start_of_method, r_disp
 * jmp  r_start_of_method
 * done:
 */
void X86Mir2Lir::GenPackedSwitch(MIR* mir, uint32_t table_offset,
                                 RegLocation rl_src) {
  const uint16_t* table = cu_->insns + current_dalvik_offset_ + table_offset;
  if (cu_->verbose) {
    DumpPackedSwitchTable(table);
  }
  // Add the table to the list - we'll process it later
  SwitchTable *tab_rec =
      static_cast<SwitchTable *>(arena_->Alloc(sizeof(SwitchTable), ArenaAllocator::kAllocData));
  tab_rec->table = table;
  tab_rec->vaddr = current_dalvik_offset_;
  int size = table[1];
  tab_rec->targets = static_cast<LIR**>(arena_->Alloc(size * sizeof(LIR*),
                                                      ArenaAllocator::kAllocLIR));
  switch_tables_.Insert(tab_rec);

  // Get the switch value
  rl_src = LoadValue(rl_src, kCoreReg);
  int start_of_method_reg = AllocTemp();
  // Materialize a pointer to the switch table
  // NewLIR0(kX86Bkpt);
  NewLIR1(kX86StartOfMethod, start_of_method_reg);
  int low_key = s4FromSwitchData(&table[2]);
  int keyReg;
  // Remove the bias, if necessary
  if (low_key == 0) {
    keyReg = rl_src.low_reg;
  } else {
    keyReg = AllocTemp();
    OpRegRegImm(kOpSub, keyReg, rl_src.low_reg, low_key);
  }
  // Bounds check - if < 0 or >= size continue following switch
  OpRegImm(kOpCmp, keyReg, size-1);
  LIR* branch_over = OpCondBranch(kCondHi, NULL);

  // Load the displacement from the switch table
  int disp_reg = AllocTemp();
  NewLIR5(kX86PcRelLoadRA, disp_reg, start_of_method_reg, keyReg, 2,
          reinterpret_cast<uintptr_t>(tab_rec));
  // Add displacement to start of method
  OpRegReg(kOpAdd, start_of_method_reg, disp_reg);
  // ..and go!
  LIR* switch_branch = NewLIR1(kX86JmpR, start_of_method_reg);
  tab_rec->anchor = switch_branch;

  /* branch_over target here */
  LIR* target = NewLIR0(kPseudoTargetLabel);
  branch_over->target = target;
}

/*
 * Array data table format:
 *  ushort ident = 0x0300   magic value
 *  ushort width            width of each element in the table
 *  uint   size             number of elements in the table
 *  ubyte  data[size*width] table of data values (may contain a single-byte
 *                          padding at the end)
 *
 * Total size is 4+(width * size + 1)/2 16-bit code units.
 */
void X86Mir2Lir::GenFillArrayData(uint32_t table_offset, RegLocation rl_src) {
  const uint16_t* table = cu_->insns + current_dalvik_offset_ + table_offset;
  // Add the table to the list - we'll process it later
  FillArrayData *tab_rec =
      static_cast<FillArrayData*>(arena_->Alloc(sizeof(FillArrayData), ArenaAllocator::kAllocData));
  tab_rec->table = table;
  tab_rec->vaddr = current_dalvik_offset_;
  uint16_t width = tab_rec->table[1];
  uint32_t size = tab_rec->table[2] | ((static_cast<uint32_t>(tab_rec->table[3])) << 16);
  tab_rec->size = (size * width) + 8;

  fill_array_data_.Insert(tab_rec);

  // Making a call - use explicit registers
  FlushAllRegs();   /* Everything to home location */
  LoadValueDirectFixed(rl_src, rX86_ARG0);
  // Materialize a pointer to the fill data image
  NewLIR1(kX86StartOfMethod, rX86_ARG2);
  NewLIR2(kX86PcRelAdr, rX86_ARG1, reinterpret_cast<uintptr_t>(tab_rec));
  NewLIR2(kX86Add32RR, rX86_ARG1, rX86_ARG2);
  CallRuntimeHelperRegReg(QUICK_ENTRYPOINT_OFFSET(pHandleFillArrayData), rX86_ARG0,
                          rX86_ARG1, true);
}

void X86Mir2Lir::GenMonitorEnter(int opt_flags, RegLocation rl_src) {
  FlushAllRegs();
  LoadValueDirectFixed(rl_src, rCX);  // Get obj
  LockCallTemps();  // Prepare for explicit register usage
  GenNullCheck(rl_src.s_reg_low, rCX, opt_flags);
  // If lock is unheld, try to grab it quickly with compare and exchange
  // TODO: copy and clear hash state?
  NewLIR2(kX86Mov32RT, rDX, Thread::ThinLockIdOffset().Int32Value());
  NewLIR2(kX86Sal32RI, rDX, LW_LOCK_OWNER_SHIFT);
  NewLIR2(kX86Xor32RR, rAX, rAX);
  NewLIR3(kX86LockCmpxchgMR, rCX, mirror::Object::MonitorOffset().Int32Value(), rDX);
  LIR* branch = NewLIR2(kX86Jcc8, 0, kX86CondEq);
  // If lock is held, go the expensive route - artLockObjectFromCode(self, obj);
  CallRuntimeHelperReg(QUICK_ENTRYPOINT_OFFSET(pLockObject), rCX, true);
  branch->target = NewLIR0(kPseudoTargetLabel);
}

void X86Mir2Lir::GenMonitorExit(int opt_flags, RegLocation rl_src) {
  FlushAllRegs();
  LoadValueDirectFixed(rl_src, rAX);  // Get obj
  LockCallTemps();  // Prepare for explicit register usage
  GenNullCheck(rl_src.s_reg_low, rAX, opt_flags);
  // If lock is held by the current thread, clear it to quickly release it
  // TODO: clear hash state?
  NewLIR2(kX86Mov32RT, rDX, Thread::ThinLockIdOffset().Int32Value());
  NewLIR2(kX86Sal32RI, rDX, LW_LOCK_OWNER_SHIFT);
  NewLIR3(kX86Mov32RM, rCX, rAX, mirror::Object::MonitorOffset().Int32Value());
  OpRegReg(kOpSub, rCX, rDX);
  LIR* branch = NewLIR2(kX86Jcc8, 0, kX86CondNe);
  NewLIR3(kX86Mov32MR, rAX, mirror::Object::MonitorOffset().Int32Value(), rCX);
  LIR* branch2 = NewLIR1(kX86Jmp8, 0);
  branch->target = NewLIR0(kPseudoTargetLabel);
  // Otherwise, go the expensive route - UnlockObjectFromCode(obj);
  CallRuntimeHelperReg(QUICK_ENTRYPOINT_OFFSET(pUnlockObject), rAX, true);
  branch2->target = NewLIR0(kPseudoTargetLabel);
}

void X86Mir2Lir::GenMoveException(RegLocation rl_dest) {
  int ex_offset = Thread::ExceptionOffset().Int32Value();
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  NewLIR2(kX86Mov32RT, rl_result.low_reg, ex_offset);
  NewLIR2(kX86Mov32TI, ex_offset, 0);
  StoreValue(rl_dest, rl_result);
}

/*
 * Mark garbage collection card. Skip if the value we're storing is null.
 */
void X86Mir2Lir::MarkGCCard(int val_reg, int tgt_addr_reg) {
  int reg_card_base = AllocTemp();
  int reg_card_no = AllocTemp();
  LIR* branch_over = OpCmpImmBranch(kCondEq, val_reg, 0, NULL);
  NewLIR2(kX86Mov32RT, reg_card_base, Thread::CardTableOffset().Int32Value());
  OpRegRegImm(kOpLsr, reg_card_no, tgt_addr_reg, gc::accounting::CardTable::kCardShift);
  StoreBaseIndexed(reg_card_base, reg_card_no, reg_card_base, 0,
                   kUnsignedByte);
  LIR* target = NewLIR0(kPseudoTargetLabel);
  branch_over->target = target;
  FreeTemp(reg_card_base);
  FreeTemp(reg_card_no);
}

void X86Mir2Lir::GenEntrySequence(RegLocation* ArgLocs, RegLocation rl_method) {
  /*
   * On entry, rX86_ARG0, rX86_ARG1, rX86_ARG2 are live.  Let the register
   * allocation mechanism know so it doesn't try to use any of them when
   * expanding the frame or flushing.  This leaves the utility
   * code with no spare temps.
   */
  LockTemp(rX86_ARG0);
  LockTemp(rX86_ARG1);
  LockTemp(rX86_ARG2);

  /* Build frame, return address already on stack */
  // TODO: 64 bit.
  OpRegImm(kOpSub, rX86_SP, frame_size_ - 4);

  /*
   * We can safely skip the stack overflow check if we're
   * a leaf *and* our frame size < fudge factor.
   */
  const bool skip_overflow_check = (mir_graph_->MethodIsLeaf() &&
      (static_cast<size_t>(frame_size_) < Thread::kStackOverflowReservedBytes));
  NewLIR0(kPseudoMethodEntry);
  /* Spill core callee saves */
  SpillCoreRegs();
  /* NOTE: promotion of FP regs currently unsupported, thus no FP spill */
  DCHECK_EQ(num_fp_spills_, 0);
  if (!skip_overflow_check) {
    // cmp rX86_SP, fs:[stack_end_]; jcc throw_launchpad
    LIR* tgt = RawLIR(0, kPseudoThrowTarget, kThrowStackOverflow, 0, 0, 0, 0);
    OpRegThreadMem(kOpCmp, rX86_SP, Thread::StackEndOffset());
    OpCondBranch(kCondUlt, tgt);
    // Remember branch target - will process later
    throw_launchpads_.Insert(tgt);
  }

  FlushIns(ArgLocs, rl_method);

  FreeTemp(rX86_ARG0);
  FreeTemp(rX86_ARG1);
  FreeTemp(rX86_ARG2);
}

void X86Mir2Lir::GenExitSequence() {
  /*
   * In the exit path, rX86_RET0/rX86_RET1 are live - make sure they aren't
   * allocated by the register utilities as temps.
   */
  LockTemp(rX86_RET0);
  LockTemp(rX86_RET1);

  NewLIR0(kPseudoMethodExit);
  UnSpillCoreRegs();
  /* Remove frame except for return address */
  OpRegImm(kOpAdd, rX86_SP, frame_size_ - 4);
  NewLIR0(kX86Ret);
}

}  // namespace art
